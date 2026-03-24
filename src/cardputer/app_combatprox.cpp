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

const uint8_t AppCombatProx::_wallMap[5][10] = {
    { 0,0,0,0,0,0,0,0,0,0 },
    { 0,0,1,0,0,0,0,1,0,0 },
    { 0,0,0,0,1,1,0,0,0,0 },
    { 0,0,1,0,0,0,0,1,0,0 },
    { 0,0,0,0,0,0,0,0,0,0 },
};

static float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float wrapAngle(float a) {
    while (a >  M_PI) a -= 2.f * M_PI;
    while (a < -M_PI) a += 2.f * M_PI;
    return a;
}

bool AppCombatProx::_wallAt(float cx, float cy) {
    if (cx < 0 || cy < 0 || cx >= AW || cy >= PLAY_H) return true;
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
    _player = { 24.f, PLAY_H / 2.f, 0.f, PLAYER_HP, 0, 0, 0, 0.f };
    _spawnEnemies();
    _lastUpdate = millis();
    _aiTimer    = millis();
}

void AppCombatProx::_spawnEnemies() {
    struct { float x, y; } pos[3] = {
        { 205.f, 20.f },
        { 215.f, 61.f },
        { 205.f, 102.f },
    };
    for (int i = 0; i < ENEMY_COUNT; i++) {
        _enemies[i] = { pos[i].x, pos[i].y, (float)M_PI,
                        1, 0, 0, i * 6, (float)(M_PI + 0.3f * i), 0 };
        int safety = 0;
        while (_circleWall(_enemies[i].x, _enemies[i].y, TANK_R) && safety++ < 40)
            _enemies[i].x -= 2.f;
    }
}

// ---- AI ----

void AppCombatProx::_aiStep(unsigned long now) {
    if (now - _aiTimer < AI_TICK) return;
    _aiTimer = now;

    // Determine which enemy is allowed to fire this tick (round-robin)
    // to ensure only one fires at a time
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
        float dist = sqrtf(dx * dx + dy * dy);

        float towardAngle = atan2f(dy, dx);
        float awayAngle   = wrapAngle(towardAngle + M_PI);

        if      (dist < RETREAT_DIST) e.aiState = 3;
        else if (dist > AGGRO_DIST)   e.aiState = 0;
        else if (dist > MIN_DIST)     e.aiState = 1;
        else                          e.aiState = 2;

        float moveAngle = e.angle;

        switch (e.aiState) {
            case 0: { // Patrol
                e.patrolTimer++;
                bool blocked = _circleWall(
                    e.x + cosf(e.angle) * SPEED * 1.4f,
                    e.y + sinf(e.angle) * SPEED * 1.4f, TANK_R);
                if (e.patrolTimer > 22 || blocked) {
                    e.patrolTimer = 0;
                    e.patrolTarget = (float)(rand() % 628) / 100.f;
                }
                float diff = wrapAngle(e.patrolTarget - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED * 0.8f);
                e.angle = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                moveAngle = e.angle;
                break;
            }
            case 1: { // Approach
                float diff = wrapAngle(towardAngle - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED);
                e.angle = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                moveAngle = e.angle;
                break;
            }
            case 2: { // Strafe
                float perpAngle = wrapAngle(towardAngle + (i & 1 ? M_PI/2.f : -M_PI/2.f));
                float diff = wrapAngle(perpAngle - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED);
                e.angle = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                // Face the player while strafing but move perpendicular
                float faceDiff = wrapAngle(towardAngle - e.angle);
                float faceTurn = fminf(fabsf(faceDiff), AI_TURN_SPEED * 0.5f);
                e.angle = wrapAngle(e.angle + (faceDiff >= 0 ? faceTurn : -faceTurn));
                moveAngle = perpAngle;
                break;
            }
            case 3: { // Retreat
                float diff = wrapAngle(awayAngle - e.angle);
                float turn = fminf(fabsf(diff), AI_TURN_SPEED);
                e.angle = wrapAngle(e.angle + (diff >= 0 ? turn : -turn));
                moveAngle = e.angle;
                break;
            }
        }

        float spd = (e.aiState == 0) ? SPEED * 0.4f : SPEED * AI_SPEED_SCALE;
        float nx = e.x + cosf(moveAngle) * spd;
        float ny = e.y + sinf(moveAngle) * spd;
        if (!_circleWall(nx, ny, TANK_R)) {
            e.x = fclamp(nx, TANK_R, AW - TANK_R);
            e.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
            e.stuckTicks = 0;
        } else {
            // Try sliding along each axis independently
            bool slidX = !_circleWall(nx, e.y, TANK_R);
            bool slidY = !_circleWall(e.x, ny, TANK_R);
            if (slidX) e.x = fclamp(nx, TANK_R, AW - TANK_R);
            if (slidY) e.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
            if (!slidX && !slidY) {
                // Completely stuck — spin until clear
                e.stuckTicks++;
                int spinDir = (i & 1) ? 1 : -1;
                // After a few ticks stuck, try the other spin direction
                if (e.stuckTicks > 4) spinDir = -spinDir;
                e.angle = wrapAngle(e.angle + (float)(spinDir) * (float)M_PI / 4.f);
                // Force patrol target to current angle so we don't fight ourselves
                e.patrolTarget = e.angle;
            }
        }

        // Fire: only the designated firer this tick
        float angleDiff = fabsf(wrapAngle(towardAngle - e.angle));
        if (i == nextFirer && angleDiff < 0.28f && dist < 160.f
            && now - e.lastShot > ENEMY_FIRE_COOLDOWN) {
            _fireBullet(e, false);
            _lastFirer = i;
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
            || b.x < 0 || b.x >= AW || b.y < 0 || b.y >= PLAY_H) {
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
    auto& d = M5Cardputer.Display;
    int x = (int)t.x, y = (int)t.y;
    d.fillCircle(x, y, TANK_R, col);
    int bx = x + (int)(cosf(t.angle) * (TANK_R + 4));
    int by = y + (int)(sinf(t.angle) * (TANK_R + 4));
    d.drawLine(x, y, bx, by, C_TEXT);
    d.fillCircle(bx, by, 1, C_TEXT);
}

void AppCombatProx::_drawBullet(const Bullet& b) {
    if (!b.active) return;
    M5Cardputer.Display.fillCircle((int)b.x, (int)b.y, BULLET_R,
        b.fromPlayer ? C_BULLET_P : C_BULLET_E);
}

void AppCombatProx::_drawArena() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, AW, PLAY_H, C_BG);
    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            if (_wallMap[row][col])
                d.fillRect(col*CELL+1, row*CELL+1, CELL-2, CELL-2, C_WALL);
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
                    _player.x = fclamp(nx, TANK_R, AW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
                }
            }
            if (c == '.') {
                float nx = _player.x - cosf(_player.angle) * SPEED;
                float ny = _player.y - sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, AW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
                }
            }
        }

        // HID arrow keys
        for (uint8_t hk : ks.hid_keys) {
            if (hk == 0x52) { // UP arrow
                float nx = _player.x + cosf(_player.angle) * SPEED;
                float ny = _player.y + sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, AW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
                }
            }
            if (hk == 0x51) { // DOWN arrow
                float nx = _player.x - cosf(_player.angle) * SPEED;
                float ny = _player.y - sinf(_player.angle) * SPEED;
                if (!_circleWall(nx, ny, TANK_R)) {
                    _player.x = fclamp(nx, TANK_R, AW - TANK_R);
                    _player.y = fclamp(ny, TANK_R, PLAY_H - TANK_R);
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
