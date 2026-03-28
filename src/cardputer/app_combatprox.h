#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "fa_icons.h"
#include "combat_highscores.h"

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
    enum Phase { PH_SPLASH, PH_CODEX, PH_PREVIEW, PH_PLAYING, PH_DEAD, PH_ROUND_CLEAR, PH_HIGHSCORES };
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
        uint8_t role;
        uint8_t type;
    };

    // enemy types
    static constexpr uint8_t ET_STANDARD  = 0;
    static constexpr uint8_t ET_SCOUT     = 1;  // fast, cyan, flanks
    static constexpr uint8_t ET_HEAVY     = 2;  // 3HP, slow, fat bullet
    static constexpr uint8_t ET_SNIPER    = 3;  // stays at range, fast thin bullet
    static constexpr uint8_t ET_ARTILLERY = 4;  // 2HP, stays back, spread volley
    static constexpr uint8_t ET_RUSHER    = 5;  // very fast, rapid-fire
    static constexpr uint8_t ET_STEALTH   = 6;  // flickers invisible
    static constexpr uint8_t ET_BOUNCER   = 7;  // bullets bounce off walls
    static constexpr uint8_t ET_MEDIC     = 8;  // 2HP, heals nearby allies
    static constexpr uint8_t ET_SHIELDER  = 9;  // 2HP, frontal bullet deflection
    static constexpr uint8_t ET_TWINS     = 10; // fires twin parallel rounds

    // bullet types
    static constexpr uint8_t BT_NORMAL    = 0;
    static constexpr uint8_t BT_FAST      = 1;
    static constexpr uint8_t BT_HEAVY     = 2;
    static constexpr uint8_t BT_SNIPER    = 3;
    static constexpr uint8_t BT_ARTILLERY = 4;
    static constexpr uint8_t BT_RUSHER    = 5;
    static constexpr uint8_t BT_BOUNCE    = 6;
    static constexpr uint8_t BT_PLAYER_H  = 7;

    // powerup types
    static constexpr uint8_t PU_REPAIR    = 1;
    static constexpr uint8_t PU_HP_UP     = 2;
    static constexpr uint8_t PU_FIRE_RATE = 3;
    static constexpr uint8_t PU_SPREAD    = 4;
    static constexpr uint8_t PU_HEAVY_SHOT= 5;
    static constexpr uint8_t PU_SPEED     = 6;

    struct Powerup {
        float   x, y;
        uint8_t type;
        bool    active;
    };
    static constexpr int MAX_POWERUPS = 6;
    Powerup _powerups[MAX_POWERUPS];

    // game constants
    static constexpr float SPEED               = 1.4f;
    static constexpr float BSPEED              = 3.8f;
    static constexpr float BSPEED_FAST         = 5.5f;
    static constexpr float BSPEED_SNIPER       = 6.5f;
    static constexpr float BSPEED_HEAVY        = 2.8f;
    static constexpr float BSPEED_RUSHER       = 4.5f;
    static constexpr float MAX_BULLET_DIST     = 200.f;
    static constexpr float MAX_BULLET_DIST_SNP = 320.f;
    static constexpr float ROTATE_SPEED        = 0.07f;
    static constexpr float AI_TURN_SPEED       = 0.35f;
    static constexpr int   TANK_R              = 5;
    static constexpr int   BULLET_R            = 2;
    static constexpr int   FIRE_COOLDOWN_BASE  = 350;
    static constexpr int   ENEMY_FIRE_COOLDOWN = 600;
    static constexpr int   PLAYER_HP_BASE      = 3;
    static constexpr int   PLAYER_HP_MAX       = 6;
    static constexpr float KAMIKAZE_SPEED      = 1.8f;
    static constexpr int   KAMIKAZE_HP         = 3;
    static constexpr uint16_t C_KAMIKAZE       = 0xFD20;
    static constexpr int   ENEMY_COUNT         = 10;
    static constexpr int   AI_TICK             = 60;
    static constexpr int   PATH_REPLAN_MS      = 700;
    static constexpr float MIN_DIST            = 30.f;
    static constexpr float PANIC_DIST          = 14.f;
    static constexpr float SNIPER_PREF_DIST    = 80.f;
    static constexpr float ARTILLERY_PREF_DIST = 110.f;
    static constexpr float AI_SPEED_SCALE      = 0.95f;
    static constexpr float WAYPOINT_RADIUS     = 8.f;
    static constexpr float REPULSE_DIST        = 20.f;
    static constexpr float REPULSE_STRENGTH    = 0.8f;

    // Point values
    static constexpr int PTS_BASE         = 10;
    static constexpr int PTS_POWERUP      = 10;
    // Per enemy type multipliers × PTS_BASE
    static constexpr int PTS_MULT[11]     = { 1, 2, 3, 2, 3, 2, 2, 2, 3, 3, 2 };

    Tank   _player;
    Tank   _enemies[ENEMY_COUNT];

    struct Bullet {
        float   x, y, vx, vy;
        float   dist;
        bool    active;
        bool    fromPlayer;
        uint8_t btype;
        int     damage;
        int     bounces;
    };
    static constexpr int MAX_BULLETS = 16;
    Bullet _bullets[MAX_BULLETS];

    // game state
    int           _score              = 0;
    int           _round              = 1;
    int           _arenaIdx           = 0;
    int           _activeEnemies      = 0;
    bool          _initialWaveCleared = false;
    bool          _reinforceSent      = false;
    bool          _kamikazeSent       = false;
    int           _kamikazeIdx        = -1;
    unsigned long _explodeStart       = 0;
    bool          _exploding          = false;
    int           _explodeX           = 0, _explodeY = 0;
    int           _previewLastSec     = -1;
    bool          _needsRedraw        = true;
    bool          _reisub             = true;
    bool          _pendingReisub      = false;
    unsigned long _lastUpdate         = 0;
    unsigned long _phaseEnter         = 0;
    unsigned long _aiTimer            = 0;
    int           _lastFirer          = -1;
    const char*   _notifyMsg          = nullptr;
    unsigned long _notifyEnd          = 0;
    unsigned long _stealthFlash       = 0;
    // key / exit mechanic — multiple keys + locked doors
    static constexpr int MAX_KEYS  = 4;
    static constexpr int MAX_DOORS = 3;
    struct LockedDoor { int c, r, keysNeeded; };
    float       _keyX[MAX_KEYS]   = {};
    float       _keyY[MAX_KEYS]   = {};
    bool        _keyPickedUp[MAX_KEYS] = {};
    int         _numKeys          = 0;
    int         _keysCollected    = 0;
    bool        _keyActive        = false;
    LockedDoor  _doors[MAX_DOORS] = {};
    int         _numDoors         = 0;
    int         _exitCellC        = 0, _exitCellR = 0;
    // maze
    bool        _isMaze           = false;
    uint8_t     _mazeWalls[20][40] = {};

    // player persistent state (carries over between rounds)
    int     _playerMaxHP  = PLAYER_HP_BASE;
    int     _fireCooldown = FIRE_COOLDOWN_BASE;
    float   _playerSpeed  = 1.0f;
    bool    _puSpread     = false;
    bool    _puHeavyShot  = false;
    static constexpr int PU_STACK_MAX = 5;
    uint8_t _puStack[PU_STACK_MAX];
    int     _puCount      = 0;
    int     _codexPage    = 0;
    // High score / initials entry
    char            _initialsEntry[4] = {'A','A','A','\0'};
    int             _initialsCursor   = 0;  // 0-2
    int             _pendingScore     = 0;
    int             _pendingRound     = 0;
    bool            _hsLoaded         = false;

    static constexpr int CELL = 12;
    static constexpr int COLS = VAW    / CELL;
    static constexpr int ROWS = VPLAY_H / CELL;

    const uint8_t (*_wallMap)[COLS] = nullptr;
    int _camX = 0, _camY = 0;

    float _diffSpeed()    const { return 1.f + (_round - 1) * 0.07f; }
    float _diffFireRate() const { return 1.f + (_round - 1) * 0.08f; }

    void _resetGame();
    void _startNextRound();
    void _resetPlayerState();
    void _spawnEnemies();
    void _spawnPowerups();
    void _applyPuStack();
    void _popPowerup();
    void _update(unsigned long now);
    void _drawArena();
    void _drawTank(const Tank& t, uint16_t col);
    void _drawBullet(const Bullet& b);
    void _drawPowerups();
    void _fireBullet(Tank& shooter, bool fromPlayer,
                     uint8_t btype = BT_NORMAL, float angleOffset = 0.f, int damage = 1);
    void _aiStep(unsigned long now);
    bool _astar(int sx, int sy, int gx, int gy, Tank& e);
    void _pathStep(Tank& e, int enemyIdx, unsigned long now, int gc, int gr);
    bool _wallAt(float cx, float cy);
    bool _circleWall(float cx, float cy, float r);
    void _drawSplash();
    void _drawCodex();
    void _drawPreview();
    void _drawExplosion();
    void _spawnReinforcements();
    void _spawnKamikaze();
    void _drawDead();
    void _drawRoundClear();
    void _drawHUD();
    void _onPlayerKill(int points);
    void _onPlayerDied();
    void _drawMiniTank(int cx, int cy, uint8_t type, uint16_t col);
    void _drawHighScores();
    void _drawInitialsEntry();
    void _submitFinalScore();

    CombatHighScores _hs;
    char   _hsInitials[4]  = { 'A', 'A', 'A', 0 };
    int    _hsCursor       = 0;
    bool   _hsEntryDone    = false;
    bool   _hsOnlineDone   = false;
    int    _hsTab          = 0;
    int    _hsFinalScore   = 0;
    int    _hsFinalRound   = 0;
    void _generateMaze();
    void _spawnMazeEnemies();

    static constexpr uint16_t C_BG        = 0x0000;
    static constexpr uint16_t C_WALL      = 0x630C;
    static constexpr uint16_t C_PLAYER    = 0x07E0;
    static constexpr uint16_t C_ENEMY     = 0xF800;
    static constexpr uint16_t C_SCOUT     = 0x07FF;
    static constexpr uint16_t C_HEAVY     = 0x7800;
    static constexpr uint16_t C_SNIPER    = 0xF81F;
    static constexpr uint16_t C_ARTILLERY = 0xFC60;
    static constexpr uint16_t C_RUSHER    = 0xFFE0;
    static constexpr uint16_t C_STEALTH   = 0x4208;
    static constexpr uint16_t C_BOUNCER   = 0x03EF;
    static constexpr uint16_t C_MEDIC     = 0x87E0;  // pale green
    static constexpr uint16_t C_SHIELDER  = 0xAD55;  // silver-grey
    static constexpr uint16_t C_TWINS     = 0xFC1F;
    static constexpr uint16_t C_KEY       = 0xFFE0;
    static constexpr uint16_t C_EXIT      = 0x07E0;
    static constexpr uint16_t C_BULLET_P  = 0xFFE0;
    static constexpr uint16_t C_BULLET_E  = 0xFC00;
    static constexpr uint16_t C_HUD_BG    = 0x1082;
    static constexpr uint16_t C_TEXT      = 0xFFFF;
    static constexpr uint16_t C_PU_REPAIR = 0x07E0;
    static constexpr uint16_t C_PU_HPUP   = 0x07FF;
    static constexpr uint16_t C_PU_FIRE   = 0xFFE0;
    static constexpr uint16_t C_PU_SPREAD = 0xFD20;
    static constexpr uint16_t C_PU_HEAVY  = 0xF81F;
    static constexpr uint16_t C_PU_SPEED  = 0x87FF;  // light cyan
};

} // namespace Cardputer
#endif
