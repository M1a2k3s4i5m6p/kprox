#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../hid.h"
#include "app_mediacontrol.h"

namespace Cardputer {

// 6 controls arranged in a 3-column × 2-row grid
// Row 0: Prev  Play  Next
// Row 1: Stop  Mute  Vol-  Vol+   (4 in row 1, 3 in row 0 — use a flat 3x2 approach)
//
// Actually lay out as 3 cols × 2 rows = 6 buttons:
// [0] Prev    [1] Play    [2] Next
// [3] Stop    [4] Mute    [5] Vol-   [WAIT — only 6 slots, Vol+ accessible via hotkey)
//
// Simpler: 3x2 flat grid, reading order:
// [0] |<<  Prev       hotkey P
// [1] >|| Play/Pause  hotkey SPACE
// [2] >>| Next        hotkey N
// [3] []  Stop        hotkey S
// [4] (X) Mute        hotkey M
// [5] v-  Vol Down    hotkey D / -
//     v+  Vol Up is hotkey U / + only (no tile — 6 fit cleanly in 3x2)
//
// Re-think: keep Vol+ as a 7th would mean 4+3 split. Instead make it 4x2 = 8
// but only populate 6, leaving last 2 blank. That's messy.
//
// Best: 3 cols × 2 rows, 6 buttons, Vol+ is U/+ shortcut and wraps to index 5+1=0.
// The user asked to drop EJECTCD and MICMUTE only, not Vol+.
// So we have exactly 7 controls: Prev, Stop, Play, Next, Mute, Vol-, Vol+
// Lay them out 4+3: row0=Prev,Stop,Play,Next  row1=Mute,Vol-,Vol+,[empty]
// Cell width = 240/4 = 60, same as before. Just leave index 7 blank.

static constexpr int BAR_H   = 18;
static constexpr int BOT_H   = 14;
static constexpr int BODY_Y  = BAR_H;
static constexpr int BODY_H  = 135 - BAR_H - BOT_H;
static constexpr int COLS    = 4;
static constexpr int ROWS    = 2;
static constexpr int CELL_W  = 240 / COLS;
static constexpr int CELL_H  = BODY_H / ROWS;
static constexpr int BTN_PAD = 4;
static constexpr int NUM_CTRL = 7;

struct ControlDef {
    const char* label;
    const char* token;
    char        hotkey;
};

static const ControlDef CONTROLS[NUM_CTRL] = {
    { "Prev",   "{PREVTRACK}",  'p' },  // 0
    { "Stop",   "{STOPTRACK}",  's' },  // 1
    { "Play",   "{PLAYPAUSE}",  ' ' },  // 2
    { "Next",   "{NEXTTRACK}",  'n' },  // 3
    { "Mute",   "{MUTE}",       'm' },  // 4
    { "Vol -",  "{VOLUMEDOWN}", 'd' },  // 5
    { "Vol +",  "{VOLUMEUP}",   'u' },  // 6
};

static constexpr uint16_t COL_BG        = 0x0841;
static constexpr uint16_t COL_BAR       = 0x0C41;
static constexpr uint16_t COL_BTN_IDLE  = 0x2104;
static constexpr uint16_t COL_BTN_SEL   = 0x1A5F;
static constexpr uint16_t COL_BTN_FLASH = 0x07E0;
static constexpr uint16_t COL_SYM_IDLE  = 0xC618;
static constexpr uint16_t COL_SYM_SEL   = 0xFFFF;
static constexpr uint16_t COL_SYM_FLASH = 0x0000;
static constexpr uint16_t COL_LBL       = 0x6B4D;
static constexpr uint16_t COL_LBL_SEL   = 0xFD20;
static constexpr uint16_t COL_BORDER    = 0x4228;
static constexpr uint16_t COL_BORDER_SEL= 0x051F;
static constexpr uint16_t COL_BOT_BAR   = 0x1082;
static constexpr uint16_t COL_BOT_TXT   = 0x4228;

// ── Symbol draw routines ──────────────────────────────────────────────────────

static void _drawPrev(int cx, int cy, uint16_t c) {
    int h = 9;
    M5Cardputer.Display.fillRect(cx-9, cy-h, 2, h*2, c);
    M5Cardputer.Display.fillTriangle(cx+1,cy-h, cx-4,cy, cx+1,cy+h, c);
    M5Cardputer.Display.fillTriangle(cx+6,cy-h, cx+1,cy, cx+6,cy+h, c);
}

static void _drawStop(int cx, int cy, uint16_t c) {
    M5Cardputer.Display.fillRect(cx-8, cy-8, 16, 16, c);
}

static void _drawPlayPause(int cx, int cy, uint16_t c) {
    int h = 9;
    M5Cardputer.Display.fillTriangle(cx-8,cy-h, cx+1,cy, cx-8,cy+h, c);
    M5Cardputer.Display.fillRect(cx+3, cy-h, 3, h*2, c);
    M5Cardputer.Display.fillRect(cx+8, cy-h, 3, h*2, c);
}

static void _drawNext(int cx, int cy, uint16_t c) {
    int h = 9;
    M5Cardputer.Display.fillTriangle(cx-8,cy-h, cx-3,cy, cx-8,cy+h, c);
    M5Cardputer.Display.fillTriangle(cx-3,cy-h, cx+2,cy, cx-3,cy+h, c);
    M5Cardputer.Display.fillRect(cx+3, cy-h, 2, h*2, c);
}

static void _drawMute(int cx, int cy, uint16_t c) {
    auto& d = M5Cardputer.Display;
    int bx = cx-8, bw=3, bh=8;
    d.fillRect(bx, cy-bh/2, bw, bh, c);
    d.fillTriangle(bx+bw, cy-6,  bx+bw+6, cy-2,  bx+bw, cy+6, c);
    d.fillTriangle(bx+bw, cy-2,  bx+bw+6, cy-2,  bx+bw, cy+2, c);
    int x0=cx+1, x1=cx+9, y0=cy-5, y1=cy+5;
    for (int t=-1; t<=1; t++) {
        d.drawLine(x0+t, y0, x1+t, y1, c);
        d.drawLine(x0+t, y1, x1+t, y0, c);
    }
}

static void _drawVolDown(int cx, int cy, uint16_t c) {
    auto& d = M5Cardputer.Display;
    int bx = cx-9, bw=3, bh=6;
    d.fillRect(bx, cy-bh/2, bw, bh, c);
    d.fillTriangle(bx+bw, cy-5, bx+bw+5, cy-2, bx+bw, cy+5, c);
    d.drawArc(bx+bw, cy, 8, 2, 300, 60, c);
    int ax = cx+5;
    d.fillRect(ax-1, cy-7, 3, 8, c);
    d.fillTriangle(ax-4, cy+1, ax+4, cy+1, ax, cy+7, c);
}

static void _drawVolUp(int cx, int cy, uint16_t c) {
    auto& d = M5Cardputer.Display;
    int bx = cx-9, bw=3, bh=6;
    d.fillRect(bx, cy-bh/2+2, bw, bh, c);
    d.fillTriangle(bx+bw, cy-3, bx+bw+5, cy, bx+bw, cy+7, c);
    d.drawArc(bx+bw, cy+2, 8, 2, 300, 60, c);
    int ax = cx+5;
    d.fillTriangle(ax-4, cy, ax+4, cy, ax, cy-6, c);
    d.fillRect(ax-1, cy, 3, 8, c);
}

static void _drawSymbol(int idx, int cx, int cy, uint16_t sym, uint16_t bg) {
    switch (idx) {
        case 0: _drawPrev(cx, cy, sym);      break;
        case 1: _drawStop(cx, cy, sym);      break;
        case 2: _drawPlayPause(cx, cy, sym); break;
        case 3: _drawNext(cx, cy, sym);      break;
        case 4: _drawMute(cx, cy, sym);      break;
        case 5: _drawVolDown(cx, cy, sym);   break;
        case 6: _drawVolUp(cx, cy, sym);     break;
    }
}

// ── Button ────────────────────────────────────────────────────────────────────

static void _drawButton(int idx, bool selected, bool flashing) {
    auto& disp = M5Cardputer.Display;
    int col = idx % COLS, row = idx / COLS;
    int bx  = col * CELL_W + BTN_PAD;
    int by  = BODY_Y + row * CELL_H + BTN_PAD;
    int bw  = CELL_W - BTN_PAD * 2;
    int bh  = CELL_H - BTN_PAD * 2;

    uint16_t bg  = flashing ? COL_BTN_FLASH : (selected ? COL_BTN_SEL  : COL_BTN_IDLE);
    uint16_t sym = flashing ? COL_SYM_FLASH : (selected ? COL_SYM_SEL  : COL_SYM_IDLE);
    uint16_t lbl = flashing ? COL_SYM_FLASH : (selected ? COL_LBL_SEL  : COL_LBL);

    disp.fillRoundRect(bx, by, bw, bh, 5, bg);
    if (selected || flashing)
        disp.drawRoundRect(bx, by, bw, bh, 5, selected ? COL_BORDER_SEL : COL_BORDER);

    int symAreaH = bh * 6 / 10;
    _drawSymbol(idx, bx + bw/2, by + symAreaH/2, sym, bg);

    disp.setTextSize(1);
    disp.setTextColor(lbl, bg);
    const char* name = CONTROLS[idx].label;
    disp.drawString(name, bx + (bw - disp.textWidth(name))/2,
                    by + bh - disp.fontHeight() - 2);
}

// ── Render ────────────────────────────────────────────────────────────────────

void AppMediaControl::_draw() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(COL_BG);

    disp.fillRect(0, 0, disp.width(), BAR_H, COL_BAR);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(100,160,255), COL_BAR);
    disp.drawString("MediaCtrl", 4, 3);
    drawTabHint(4 + disp.textWidth("MediaCtrl") + 3);

    bool conn = isBLEConnected();
#ifdef BOARD_HAS_USB_HID
    if (!conn) conn = (usbEnabled && usbInitialized && usbKeyboardReady);
#endif
    uint16_t connCol = conn ? disp.color565(80,200,80) : disp.color565(160,80,80);
    const char* connLbl = conn ? "HID" : "---";
    disp.setTextColor(connCol, COL_BAR);
    disp.drawString(connLbl, disp.width() - disp.textWidth(connLbl) - 4, 3);

    disp.drawRect(1, BODY_Y, disp.width()-2, BODY_H, COL_BORDER);

    for (int i = 0; i < NUM_CTRL; i++)
        _drawButton(i, i == _sel, _flashActive && i == _flashSel);

    // Fill the empty 8th cell so it doesn't look like a ghost button
    {
        int bx = 3 * CELL_W + BTN_PAD;
        int by = BODY_Y + 1 * CELL_H + BTN_PAD;
        int bw = CELL_W - BTN_PAD * 2;
        int bh = CELL_H - BTN_PAD * 2;
        disp.fillRoundRect(bx, by, bw, bh, 5, COL_BG);
    }

    disp.drawFastHLine(2, BODY_Y + CELL_H, disp.width()-4, COL_BORDER);

    int botY = 135 - BOT_H;
    disp.fillRect(0, botY, disp.width(), BOT_H, COL_BOT_BAR);
    disp.setTextSize(1);
    disp.setTextColor(COL_BOT_TXT, COL_BOT_BAR);
    disp.drawString("arrows sel  ENTER fire  SPC=play  ESC back", 3, botY+2);

    _needsRedraw = false;
}

// ── Token dispatch ────────────────────────────────────────────────────────────

void AppMediaControl::_fire(int idx) {
    pendingTokenStrings.push_back(String(CONTROLS[idx].token));
}

void AppMediaControl::_flashAndFire(int idx) {
    _sel         = idx;
    _flashSel    = idx;
    _flashActive = true;
    _flashUntil  = millis() + 150;
    _fire(idx);
    _needsRedraw = true;
}

// ── AppBase ───────────────────────────────────────────────────────────────────

void AppMediaControl::onEnter() {
    _needsRedraw = true;
    _flashActive = false;
    _flashSel    = -1;
}

void AppMediaControl::onUpdate() {
    if (_flashActive && millis() >= _flashUntil) {
        _flashActive = false;
        _needsRedraw = true;
    }
    if (_needsRedraw) _draw();

    if (M5Cardputer.BtnA.wasPressed()) { uiManager.returnToLauncher(); return; }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    if (ki.arrowLeft) {
        // Wrap within row; skip the empty 8th slot
        int row = _sel / COLS, col = _sel % COLS;
        int newCol = (col - 1 + COLS) % COLS;
        int newIdx = row * COLS + newCol;
        if (newIdx < NUM_CTRL) _sel = newIdx;
        else _sel = row * COLS + (COLS - 1 - 1); // skip empty
        _needsRedraw = true; return;
    }
    if (ki.arrowRight) {
        int row = _sel / COLS, col = _sel % COLS;
        int newCol = (col + 1) % COLS;
        int newIdx = row * COLS + newCol;
        if (newIdx < NUM_CTRL) _sel = newIdx;
        else _sel = row * COLS; // wrap to row start
        _needsRedraw = true; return;
    }
    if (ki.arrowUp) {
        int col = _sel % COLS;
        int newIdx = ((_sel / COLS - 1 + ROWS) % ROWS) * COLS + col;
        if (newIdx >= NUM_CTRL) newIdx = col; // if empty slot, go to row 0
        _sel = newIdx;
        _needsRedraw = true; return;
    }
    if (ki.arrowDown) {
        int col = _sel % COLS;
        int newIdx = ((_sel / COLS + 1) % ROWS) * COLS + col;
        if (newIdx >= NUM_CTRL) newIdx = (_sel / COLS) * COLS + col; // stay if empty
        _sel = newIdx;
        _needsRedraw = true; return;
    }

    if (ki.enter) { _flashAndFire(_sel); return; }

    if (ki.ch) {
        char c = ki.ch;
        if (c >= 'A' && c <= 'Z') c += 32;
        for (int i = 0; i < NUM_CTRL; i++)
            if (c == CONTROLS[i].hotkey) { _flashAndFire(i); return; }
        if (c == '+' || c == '=') { _flashAndFire(6); return; }
        if (c == '-' || c == '_') { _flashAndFire(5); return; }
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
