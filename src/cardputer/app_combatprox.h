#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "fa_icons.h"

namespace Cardputer {

class AppCombatProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;

    const char* appName() const override { return "psCombatProx"; }
    const char* appHelp() const override {
        return "psCombatProx — a ps command replacement.\n\n"
               "Kill enemy: echo name + kill -9 <real pid>\n"
               "You die: Magic SysRq REISUB reboot.\n\n"
               "Open a ROOT terminal on target first!\n\n"
               "up=forward  dn=backward\n"
               "left=rotate CCW  right=rotate CW\n"
               "fn / ENTER / BtnA = fire\n"
               "ESC = quit\n\n"
               "d=disable reisub (testing)";
    }
    const uint16_t* appIcon() const override { return fa_crosshairs_48; }

private:
    enum Phase { PH_SPLASH, PH_PLAYING, PH_DEAD, PH_WIN };
    Phase _phase = PH_SPLASH;

    // Display area (physical screen)
    static constexpr int   AW     = 240;
    static constexpr int   AH     = 135;
    static constexpr int   BAR_H  = 13;
    static constexpr int   PLAY_H = AH - BAR_H;

    // Virtual arena (2× the display)
    static constexpr int   VAW    = 480;
    static constexpr int   VPLAY_H= 240;

    struct Tank {
        float x, y;
        float angle;
        int   hp;
        unsigned long lastShot;
        int   aiState;
        int   patrolTimer;
        float patrolTarget;
        int   stuckTicks;
        // A* path following
        int8_t pathX[32];
        int8_t pathY[32];
        int    pathLen;
        int    pathIdx;
        unsigned long pathTimer;
        uint8_t wanderBias;   // 0-100: % chance of random detour each waypoint
    };

    static constexpr float SPEED               = 1.4f;
    static constexpr float BSPEED              = 3.8f;
    static constexpr float MAX_BULLET_DIST     = 200.f;
    static constexpr float ROTATE_SPEED        = 0.07f;
    static constexpr float AI_TURN_SPEED       = 0.35f;   // sharper turns
    static constexpr int   TANK_R              = 5;
    static constexpr int   BULLET_R            = 2;
    static constexpr int   FIRE_COOLDOWN       = 350;
    static constexpr int   ENEMY_FIRE_COOLDOWN = 600;     // faster fire
    static constexpr int   PLAYER_HP           = 3;
    static constexpr int   ENEMY_COUNT         = 3;
    static constexpr int   AI_TICK             = 60;      // faster AI ticks
    static constexpr int   PATH_REPLAN_MS      = 700;     // replan more often
    static constexpr float AGGRO_DIST          = 9999.f;  // always aggressive
    static constexpr float MIN_DIST            = 30.f;
    static constexpr float RETREAT_DIST        = 22.f;
    static constexpr float AI_SPEED_SCALE      = 0.95f;   // near full speed
    static constexpr float WAYPOINT_RADIUS     = 8.f;
    static constexpr float REPULSE_DIST        = 20.f;
    static constexpr float REPULSE_STRENGTH    = 0.8f;

    Tank  _player;
    Tank  _enemies[ENEMY_COUNT];

    struct Bullet {
        float x, y, vx, vy;
        float dist;       // accumulated travel distance
        bool  active;
        bool  fromPlayer;
    };
    static constexpr int MAX_BULLETS = 8;
    Bullet _bullets[MAX_BULLETS];

    int           _score        = 0;
    bool          _needsRedraw   = true;
    bool          _reisub        = true;   // toggled by secret 'd' key
    bool          _pendingReisub = false;
    unsigned long _lastUpdate   = 0;
    unsigned long _phaseEnter   = 0;
    unsigned long _aiTimer      = 0;
    int           _lastFirer    = -1;     // which enemy last fired

    static constexpr int CELL = 12;
    static constexpr int COLS = VAW    / CELL;   // 40
    static constexpr int ROWS = VPLAY_H / CELL;  // 20
    static const uint8_t _wallMap[ROWS][COLS];

    // Scrolling camera — top-left of viewport in virtual coords
    int _camX = 0, _camY = 0;

    void _resetGame();
    void _spawnEnemies();
    void _update(unsigned long now);
    void _drawArena();
    void _drawTank(const Tank& t, uint16_t col);
    void _drawBullet(const Bullet& b);
    void _fireBullet(Tank& shooter, bool fromPlayer);
    void _aiStep(unsigned long now);
    bool _astar(int sx, int sy, int gx, int gy, Tank& e);
    void _pathStep(Tank& e, int enemyIdx, unsigned long now);
    bool _wallAt(float cx, float cy);
    bool _circleWall(float cx, float cy, float r);
    void _drawSplash();
    void _drawDead();
    void _drawWin();
    void _drawHUD();
    void _onPlayerKill();
    void _onPlayerDied();

    static constexpr uint16_t C_BG      = 0x0000;
    static constexpr uint16_t C_WALL    = 0x630C;
    static constexpr uint16_t C_PLAYER  = 0x07E0;
    static constexpr uint16_t C_ENEMY   = 0xF800;
    static constexpr uint16_t C_BULLET_P= 0xFFE0;
    static constexpr uint16_t C_BULLET_E= 0xFC00;
    static constexpr uint16_t C_HUD_BG  = 0x1082;
    static constexpr uint16_t C_TEXT    = 0xFFFF;
};

} // namespace Cardputer
#endif
