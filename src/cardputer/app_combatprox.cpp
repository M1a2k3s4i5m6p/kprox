#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_combatprox.h"
#include "ui_manager.h"
#include <M5Cardputer.h>
#include <stdlib.h>
#include <math.h>

namespace Cardputer {

static const char* REISUB_TOKEN =
    "{CHORD ALT+SYSRQ+R}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+E}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+I}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+S}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+U}{SLEEP 2000}"
    "{CHORD ALT+SYSRQ+B}";

#include "combatprox_arena.h"

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
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return true;
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
    _lastFirer = -1;
    memset(_bullets, 0, sizeof(_bullets));
    _player = { ARENA_PLAYER_X, ARENA_PLAYER_Y, 0.f, PLAYER_HP, 0, 0, 0, 0.f };
    _camX = (int)_player.x - AW/2;
    _camY = (int)_player.y - PLAY_H/2;
    _camX = (int)fclamp((float)_camX, 0, VAW    - AW);
    _camY = (int)fclamp((float)_camY, 0, VPLAY_H - PLAY_H);
    _spawnEnemies();
    _lastUpdate = millis();
    _aiTimer    = millis();
}

void AppCombatProx::_spawnEnemies() {
    const uint8_t wanderBiases[3] = { 5, 12, 20 };  // low wander — enemies hunt aggressively
    for (int i = 0; i < ENEMY_COUNT && i < ARENA_ENEMY_COUNT; i++) {
        _enemies[i] = {};
        _enemies[i].x           = arenaEnemySpawns[i][0];
        _enemies[i].y           = arenaEnemySpawns[i][1];
        _enemies[i].angle       = (float)M_PI;
        _enemies[i].hp          = 1;
        _enemies[i].aiState     = 0;
        _enemies[i].patrolTimer = i * 6;
        _enemies[i].patrolTarget= (float)(M_PI + 0.3f * i);
        _enemies[i].wanderBias  = wanderBiases[i];
        _enemies[i].pathLen     = 0;
        _enemies[i].pathIdx     = 0;
        _enemies[i].pathTimer   = 0;
    }
    for (int i = ARENA_ENEMY_COUNT; i < ENEMY_COUNT; i++) {
        _enemies[i] = {};
        _enemies[i].x          = 210.f;
        _enemies[i].y          = 30.f + i * 30.f;
        _enemies[i].angle      = (float)M_PI;
        _enemies[i].hp         = 1;
        _enemies[i].wanderBias = 25;
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
        if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return true;
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

// Steer enemy toward next path waypoint; replans when needed.
void AppCombatProx::_pathStep(Tank& e, int enemyIdx, unsigned long now) {
    int ec = (int)(e.x / CELL);
    int er = (int)(e.y / CELL);
    int pc = (int)(_player.x / CELL);
    int pr = (int)(_player.y / CELL);

    bool needReplan = (e.pathLen == 0) ||
                      (e.pathIdx >= e.pathLen) ||
                      (now - e.pathTimer > (unsigned long)PATH_REPLAN_MS);

    if (needReplan) {
        e.pathTimer = now;
        if (!_astar(ec, er, pc, pr, e)) {
            // No path — fall back to direct approach
            e.pathLen = 0;
        }
    }

    if (e.pathLen == 0 || e.pathIdx >= e.pathLen) return;

    // Advance waypoint if arrived
    int wx = e.pathX[e.pathIdx];
    int wy = e.pathY[e.pathIdx];
    float wpx = wx * CELL + CELL * 0.5f;
    float wpy = wy * CELL + CELL * 0.5f;
    float dwx = wpx - e.x, dwy = wpy - e.y;
    if (sqrtf(dwx*dwx + dwy*dwy) < WAYPOINT_RADIUS) {
        e.pathIdx++;
        if (e.pathIdx >= e.pathLen) return;

        // Random detour: occasionally swap next waypoint for a random open neighbour
        if ((rand() % 100) < e.wanderBias) {
            int nx2 = e.pathX[e.pathIdx];
            int ny2 = e.pathY[e.pathIdx];
            static const int8_t DX4[4] = { 1,-1, 0, 0 };
            static const int8_t DY4[4] = { 0, 0, 1,-1 };
            int tries = 0;
            while (tries++ < 8) {
                int di = rand() % 4;
                int cx2 = nx2 + DX4[di], cy2 = ny2 + DY4[di];
                if (cx2 >= 0 && cx2 < COLS && cy2 >= 0 && cy2 < ROWS
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

    // Steer toward waypoint
    float targetAngle = atan2f(wpy - e.y, wpx - e.x);
    float diff  = wrapAngle(targetAngle - e.angle);
    float turn  = fminf(fabsf(diff), AI_TURN_SPEED);
    e.angle     = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
}

// ---- AI step ----

void AppCombatProx::_aiStep(unsigned long now) {
    if (now - _aiTimer < AI_TICK) return;
    _aiTimer = now;

    int nextFirer = -1;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        int idx = (_lastFirer + 1 + i) % ENEMY_COUNT;
        if (_enemies[idx].hp > 0) { nextFirer = idx; break; }
    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        Tank& e = _enemies[i];
        if (e.hp <= 0) continue;

        float dx   = _player.x - e.x;
        float dy   = _player.y - e.y;
        float dist = sqrtf(dx*dx + dy*dy);

        float towardAngle = atan2f(dy, dx);
        float awayAngle   = wrapAngle(towardAngle + (float)M_PI);

        if      (dist < RETREAT_DIST) e.aiState = 3;
        else if (dist > AGGRO_DIST)   e.aiState = 0;
        else if (dist > MIN_DIST)     e.aiState = 1;
        else                          e.aiState = 2;

        float moveAngle = e.angle;

        switch (e.aiState) {
            case 0: {  // Patrol via A* toward centre area or wander
                e.patrolTimer++;
                bool blocked = _circleWall(
                    e.x + cosf(e.angle) * SPEED * 1.4f,
                    e.y + sinf(e.angle) * SPEED * 1.4f, TANK_R);
                if (e.patrolTimer > 22 || blocked) {
                    e.patrolTimer = 0;
                    e.patrolTarget = (float)(rand() % 628) / 100.f;
                    // Force path replan so we don't track a stale player path
                    e.pathLen = 0;
                }
                float diff = wrapAngle(e.patrolTarget - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED * 0.8f);
                e.angle    = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                moveAngle  = e.angle;
                break;
            }
            case 1: {  // Approach via A*
                _pathStep(e, i, now);
                moveAngle = e.angle;
                break;
            }
            case 2: {  // Strafe — stay facing player, orbit
                float perpAngle = wrapAngle(towardAngle + (i & 1 ? (float)M_PI/2.f : -(float)M_PI/2.f));
                float diff  = wrapAngle(perpAngle - e.angle);
                float turn  = fminf(fabsf(diff), AI_TURN_SPEED);
                e.angle     = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                float faceDiff = wrapAngle(towardAngle - e.angle);
                float faceTurn = fminf(fabsf(faceDiff), AI_TURN_SPEED * 0.5f);
                e.angle     = wrapAngle(e.angle + (faceDiff >= 0 ? faceTurn : -faceTurn));
                moveAngle   = perpAngle;
                // Invalidate path so we replan fresh when we switch back to approach
                e.pathLen   = 0;
                break;
            }
            case 3: {  // Retreat via A* to a random point on the opposite side
                // Pick a new retreat target if we don't have a valid path
                bool needNewTarget = (e.pathLen == 0 || e.pathIdx >= e.pathLen);
                if (needNewTarget) {
                    // "Opposite side" = cols far from player, rows anywhere
                    int pc2 = (int)(_player.x / CELL);
                    int pr2 = (int)(_player.y / CELL);
                    int ec2 = (int)(e.x / CELL);
                    int er2 = (int)(e.y / CELL);
                    // Try up to 12 random candidates on the opposite half
                    int bestC = -1, bestR = -1;
                    for (int attempt = 0; attempt < 12; attempt++) {
                        int tc, tr;
                        if (pc2 < COLS / 2) {
                            // player is on left — flee right
                            tc = COLS / 2 + rand() % (COLS / 2);
                        } else {
                            // player is on right — flee left
                            tc = rand() % (COLS / 2);
                        }
                        tr = rand() % ROWS;
                        if (!_wallMap[tr][tc] && (tc != ec2 || tr != er2)) {
                            bestC = tc; bestR = tr;
                            break;
                        }
                    }
                    if (bestC >= 0 && _astar(ec2, er2, bestC, bestR, e)) {
                        // path filled — follow it
                    } else {
                        e.pathLen = 0;
                    }
                }
                if (e.pathLen > 0 && e.pathIdx < e.pathLen) {
                    _pathStep(e, i, now);
                } else {
                    // Fallback: turn directly away
                    float diff = wrapAngle(awayAngle - e.angle);
                    float turn = fminf(fabsf(diff), AI_TURN_SPEED);
                    e.angle    = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                }
                moveAngle = e.angle;
                break;
            }
        }

        float spd = (e.aiState == 0) ? SPEED * 0.4f : SPEED * AI_SPEED_SCALE;
        float nx  = e.x + cosf(moveAngle) * spd;
        float ny  = e.y + sinf(moveAngle) * spd;

        if (!_circleWall(nx, ny, TANK_R)) {
            e.x = fclamp(nx, TANK_R, VAW - TANK_R);
            e.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
            e.stuckTicks = 0;
        } else {
            bool slidX = !_circleWall(nx, e.y, TANK_R);
            bool slidY = !_circleWall(e.x, ny, TANK_R);
            if (slidX) e.x = fclamp(nx, TANK_R, VAW - TANK_R);
            if (slidY) e.y = fclamp(ny, TANK_R, VPLAY_H - TANK_R);
            if (!slidX && !slidY) {
                e.stuckTicks++;
                int spinDir = (i & 1) ? 1 : -1;
                if (e.stuckTicks > 4) spinDir = -spinDir;
                e.angle = wrapAngle(e.angle + (float)(spinDir) * (float)M_PI / 4.f);
                e.patrolTarget = e.angle;
                // Force path replan after getting unstuck
                if (e.stuckTicks > 6) { e.pathLen = 0; e.stuckTicks = 0; }
            }
        }

        float angleDiff = fabsf(wrapAngle(towardAngle - e.angle));
        if (i == nextFirer && angleDiff < 0.45f && dist < 300.f
            && now - e.lastShot > ENEMY_FIRE_COOLDOWN) {
            _fireBullet(e, false);
            _lastFirer = i;
        }

        // Repel from other living enemies
        for (int j = 0; j < ENEMY_COUNT; j++) {
            if (j == i || _enemies[j].hp <= 0) continue;
            float rx = e.x - _enemies[j].x;
            float ry = e.y - _enemies[j].y;
            float rd = sqrtf(rx*rx + ry*ry);
            if (rd > 0.01f && rd < REPULSE_DIST) {
                float push = REPULSE_STRENGTH * (1.f - rd / REPULSE_DIST);
                float rpx  = e.x + rx / rd * push;
                float rpy  = e.y + ry / rd * push;
                if (!_circleWall(rpx, e.y, TANK_R)) e.x = fclamp(rpx, TANK_R, VAW - TANK_R);
                if (!_circleWall(e.x, rpy, TANK_R)) e.y = fclamp(rpy, TANK_R, VPLAY_H - TANK_R);
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

void AppCombatProx::_onPlayerKill() {
    // Single awk pipeline: no $() subshells, no semicolon-separated assignments.
    // $() and $VAR both trigger kprox variable interpolation and get consumed.
    // Awk braces escaped as \{ \} for the kprox token parser.
    String cmd =
        "ps -eo pid=,comm= | awk 'NR>1\\{a[NR]=$1;n[NR]=$2\\}"
        "END\\{srand();r=int(rand()*(NR-1))+2;"
        "print \"Ops, I guess you did not need \"n[r]\"...\";system(\"kill -9 \"a[r])\\}'\n";
    pendingTokenStrings.push_back(cmd);
    _score++;
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
                    _enemies[e].hp = 0; b.active = false; _onPlayerKill(); break;
                }
            }
        } else {
            float ddx = b.x - _player.x, ddy = b.y - _player.y;
            if (ddx*ddx + ddy*ddy < (TANK_R+BULLET_R)*(TANK_R+BULLET_R)) {
                _player.hp--; b.active = false;
                if (_player.hp <= 0) {
                    _pendingReisub = true;  // send after death screen draws
                    _phase = PH_DEAD; _phaseEnter = now; return;
                }
            }
        }
    }

    bool anyAlive = false;
    for (int e = 0; e < ENEMY_COUNT; e++) if (_enemies[e].hp > 0) anyAlive = true;
    if (!anyAlive) { _phase = PH_WIN; _phaseEnter = now; return; }

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
    String sc = "Kills:" + String(_score);
    d.drawString(sc, AW - d.textWidth(sc) - 2, PLAY_H + 3);
    int alive = 0;
    for (int e = 0; e < ENEMY_COUNT; e++) if (_enemies[e].hp > 0) alive++;
    String ec = "Tanks:" + String(alive);
    d.drawString(ec, AW/2 - d.textWidth(ec)/2, PLAY_H + 3);
}

void AppCombatProx::_drawTank(const Tank& t, uint16_t col) {
    if (t.hp <= 0) return;

    int sx = (int)t.x - _camX;
    int sy = (int)t.y - _camY;
    if (sx < -12 || sx > AW+12 || sy < -12 || sy > PLAY_H+12) return;

    auto& d = M5Cardputer.Display;
    float ca = cosf(t.angle), sa = sinf(t.angle);
    float pa = -sa, pb = ca;  // perpendicular

    static constexpr int TRACK_LEN  = 8;
    static constexpr int TRACK_W    = 2;
    static constexpr int TRACK_OFFS = 4;
    static constexpr int BODY_HALF  = 3;
    static constexpr int BARREL_LEN = 6;

    uint16_t trackCol = (col == C_PLAYER) ? 0x0380 : 0x7800;

    // Left track
    float tlx = sx - pa * TRACK_OFFS, tly = sy - pb * TRACK_OFFS;
    for (int i = -TRACK_LEN/2; i <= TRACK_LEN/2; i++) {
        int tx = (int)(tlx + ca * i);
        int ty = (int)(tly + sa * i);
        d.fillRect(tx - (int)(pa * TRACK_W/2), ty - (int)(pb * TRACK_W/2),
                   TRACK_W, TRACK_W, trackCol);
    }
    // Right track
    float trx = sx + pa * TRACK_OFFS, try_ = sy + pb * TRACK_OFFS;
    for (int i = -TRACK_LEN/2; i <= TRACK_LEN/2; i++) {
        int tx = (int)(trx + ca * i);
        int ty = (int)(try_ + sa * i);
        d.fillRect(tx - (int)(pa * TRACK_W/2), ty - (int)(pb * TRACK_W/2),
                   TRACK_W, TRACK_W, trackCol);
    }

    // Body
    d.fillRect(sx - BODY_HALF, sy - BODY_HALF, BODY_HALF*2+1, BODY_HALF*2+1, col);

    // Barrel (two parallel lines for width)
    int bx1 = sx + (int)(ca * BODY_HALF);
    int by1 = sy + (int)(sa * BODY_HALF);
    int bx2 = sx + (int)(ca * (BODY_HALF + BARREL_LEN));
    int by2 = sy + (int)(sa * (BODY_HALF + BARREL_LEN));
    d.drawLine(bx1, by1, bx2, by2, 0xFFFF);
    d.drawLine(bx1 + (int)(pa), by1 + (int)(pb),
               bx2 + (int)(pa), by2 + (int)(pb), 0xFFFF);
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
    for (int e = 0; e < ENEMY_COUNT; e++) _drawTank(_enemies[e], C_ENEMY);
    for (int i = 0; i < MAX_BULLETS; i++) _drawBullet(_bullets[i]);
    _drawHUD();
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
    d.drawString("TANK DESTROYED", AW/2 - d.textWidth("TANK DESTROYED")/2, 26);
    d.setTextSize(1); d.setTextColor(0xFFFF, 0x0000);
    if (_reisub) {
        d.drawString("Magic SysRq REISUB sent!", AW/2 - d.textWidth("Magic SysRq REISUB sent!")/2, 56);
        d.setTextColor(0xFC00, 0x0000);
        d.drawString("R-E-I-S-U-B  System going down...", AW/2 - d.textWidth("R-E-I-S-U-B  System going down...")/2, 70);
    } else {
        d.setTextColor(0xF800, 0x0000);
        d.drawString("REISUB disabled (safe mode)", AW/2 - d.textWidth("REISUB disabled (safe mode)")/2, 60);
    }
    d.setTextColor(0x8410, 0x0000);
    String sc = "Kills this round: " + String(_score);
    d.drawString(sc, AW/2 - d.textWidth(sc)/2, 90);
    d.setTextColor(0xFFFF, 0x0000);
    d.drawString("ENTER=retry   ESC=quit", AW/2 - d.textWidth("ENTER=retry   ESC=quit")/2, 118);
}

void AppCombatProx::_drawWin() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(2); d.setTextColor(0x07E0, 0x0000);
    d.drawString("VICTORY!", AW/2 - d.textWidth("VICTORY!")/2, 26);
    d.setTextSize(1); d.setTextColor(0xFFFF, 0x0000);
    String sc = "Killed " + String(_score) + " processes.";
    d.drawString(sc, AW/2 - d.textWidth(sc)/2, 56);
    d.setTextColor(0xFFE0, 0x0000);
    d.drawString("Target system destabilised.", AW/2 - d.textWidth("Target system destabilised.")/2, 70);
    d.setTextColor(0xFFFF, 0x0000);
    d.drawString("ENTER=play again   ESC=quit", AW/2 - d.textWidth("ENTER=play again   ESC=quit")/2, 118);
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
        if (ki.enter)      { _resetGame(); _phase = PH_PLAYING; _needsRedraw = true; }
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
        if (ki.enter) { _resetGame(); _phase = PH_PLAYING; _needsRedraw = true; }
        return;
    }

    if (_phase == PH_WIN) {
        if (_needsRedraw) { _drawWin(); _needsRedraw = false; }
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)   { _phase = PH_SPLASH; _needsRedraw = true; return; }
        if (ki.enter) { _resetGame(); _phase = PH_PLAYING; _needsRedraw = true; }
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
