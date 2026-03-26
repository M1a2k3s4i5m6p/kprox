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
    enum Phase { PH_SPLASH, PH_PREVIEW, PH_PLAYING, PH_DEAD, PH_WIN };
    Phase _phase = PH_SPLASH;

    static constexpr int AW     = 240;
    static constexpr int AH     = 135;
    static constexpr int BAR_H  = 13;
    static constexpr int PLAY_H = AH - BAR_H;

    static constexpr int VAW     = 480;
    static constexpr int VPLAY_H = 240;

    struct Tank {
        float x, y;
        float angle;
        int   hp;
        unsigned long lastShot;
        int   aiState;
        int   patrolTimer;
        float patrolTarget;
        int   stuckTicks;
        int8_t pathX[32];
        int8_t pathY[32];
        int    pathLen;
        int    pathIdx;
        unsigned long pathTimer;
        uint8_t wanderBias;
    };

    static constexpr float SPEED               = 1.4f;
    static constexpr float BSPEED              = 3.8f;
    static constexpr float MAX_BULLET_DIST     = 200.f;
    static constexpr float ROTATE_SPEED        = 0.07f;
    static constexpr float AI_TURN_SPEED       = 0.35f;
    static constexpr int   TANK_R              = 5;
    static constexpr int   BULLET_R            = 2;
    static constexpr int   FIRE_COOLDOWN       = 350;
    static constexpr int   ENEMY_FIRE_COOLDOWN = 600;
    static constexpr int   PLAYER_HP           = 3;
    static constexpr float KAMIKAZE_SPEED      = 3.0f;
    static constexpr int   KAMIKAZE_HP          = 3;
    static constexpr uint16_t C_KAMIKAZE        = 0xFD20;  // orange
    static constexpr int   ENEMY_COUNT         = 9;  // 3 initial + up to 5 reinforcements + 1 kamikaze
    static constexpr int   AI_TICK             = 60;
    static constexpr int   PATH_REPLAN_MS      = 700;
    static constexpr float AGGRO_DIST          = 9999.f;
    static constexpr float MIN_DIST            = 30.f;
    static constexpr float RETREAT_DIST        = 22.f;
    static constexpr float AI_SPEED_SCALE      = 0.95f;
    static constexpr float WAYPOINT_RADIUS     = 8.f;
    static constexpr float REPULSE_DIST        = 20.f;
    static constexpr float REPULSE_STRENGTH    = 0.8f;

    Tank   _player;
    Tank   _enemies[ENEMY_COUNT];

    struct Bullet {
        float x, y, vx, vy;
        float dist;
        bool  active;
        bool  fromPlayer;
    };
    static constexpr int MAX_BULLETS = 8;
    Bullet _bullets[MAX_BULLETS];

    int           _score              = 0;
    int           _arenaIdx           = 0;
    int           _activeEnemies      = 0;  // slots 0..2 = initial wave
    bool          _initialWaveCleared = false;
    bool          _reinforceSent      = false;
    bool          _kamikazeSent       = false;
    int           _kamikazeIdx        = -1;
    unsigned long _explodeStart       = 0;
    bool          _exploding          = false;
    int           _explodeX           = 0, _explodeY = 0;
    int           _previewLastSec     = -1;
    bool          _needsRedraw   = true;
    bool          _reisub        = true;
    bool          _pendingReisub = false;
    unsigned long _lastUpdate    = 0;
    unsigned long _phaseEnter    = 0;
    unsigned long _aiTimer       = 0;
    int           _lastFirer     = -1;

    static constexpr int CELL = 12;
    static constexpr int COLS = VAW    / CELL;
    static constexpr int ROWS = VPLAY_H / CELL;

    // Pointer to the active arena's wall data — set in _resetGame()
    const uint8_t (*_wallMap)[COLS] = nullptr;

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
    void _drawPreview();
    void _drawExplosion();
    void _spawnReinforcements();
    void _spawnKamikaze();
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
