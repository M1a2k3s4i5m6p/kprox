#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_launcher.h"

namespace Cardputer {

static constexpr int ICON_SIZE    = 48;
static constexpr int ICON_SEL_SIZE = 60;
static constexpr int ICON_GAP     = 16;
static constexpr int LABEL_H      = 16;
static constexpr int TOP_MARGIN   = 8;

// Number of apps visible on screen at once (approximate)
static constexpr int VISIBLE_COUNT = 4;

void AppLauncher::onEnter() {
    _needsRedraw = true;
}

void AppLauncher::_drawIcon(int appIndex, int screenX, bool selected) {
    auto& disp = M5Cardputer.Display;
    int iconSz = selected ? ICON_SEL_SIZE : ICON_SIZE;
    int yOffset = selected ? 0 : (ICON_SEL_SIZE - ICON_SIZE) / 2;
    int iconY = TOP_MARGIN + yOffset;

    // Center icon horizontally in its slot
    int slotW = ICON_SEL_SIZE + ICON_GAP;
    int drawX = screenX + (slotW - iconSz) / 2;

    // Get the app (index+1 to skip launcher itself at index 0)
    // Apps registered in UIManager: 0=Launcher, 1..N = real apps
    // Launcher icon entries correspond to apps 1..N
    int appIdx = appIndex + 1;

    const uint16_t* icon = uiManager.apps()[appIdx]->appIcon();
    if (icon) {
        disp.pushImage(drawX, iconY, iconSz, iconSz, icon);
    } else {
        uint16_t col = uiManager.apps()[appIdx]->iconColor();
        if (selected) col = disp.color565(255, 200, 0);
        disp.fillRoundRect(drawX, iconY, iconSz, iconSz, 8, col);

        // First letter of app name
        disp.setTextSize(selected ? 3 : 2);
        disp.setTextColor(TFT_WHITE, col);
        char letter[2] = { uiManager.apps()[appIdx]->appName()[0], 0 };
        int tw = disp.textWidth(letter);
        int th = disp.fontHeight();
        disp.drawString(letter, drawX + (iconSz - tw) / 2, iconY + (iconSz - th) / 2);
    }

    // App name label below icon
    const char* name = uiManager.apps()[appIdx]->appName();
    disp.setTextSize(1);
    uint16_t labelColor = selected ? TFT_YELLOW : disp.color565(200, 200, 200);
    disp.setTextColor(labelColor, TFT_BLACK);
    int tw = disp.textWidth(name);
    int labelX = screenX + (slotW - tw) / 2;
    int labelY = TOP_MARGIN + ICON_SEL_SIZE + 4;
    disp.drawString(name, labelX, labelY);
}

void AppLauncher::_drawMenu() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TFT_BLACK);

    // Number of real apps (skip launcher at index 0)
    int numApps = (int)uiManager.apps().size() - 1;
    if (numApps <= 0) return;

    int slotW = ICON_SEL_SIZE + ICON_GAP;

    // Scroll offset so selected item is centered
    int centerX = disp.width() / 2;
    int startX  = centerX - _selected * slotW - ICON_SEL_SIZE / 2;

    for (int i = 0; i < numApps; i++) {
        int sx = startX + i * slotW;
        // Only draw if visible
        if (sx + slotW < 0 || sx > disp.width()) continue;
        _drawIcon(i, sx, i == _selected);
    }

    // Bottom hint bar
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(120, 120, 120), TFT_BLACK);
    disp.drawString("< > navigate   ENTER select   ESC back", 4, disp.height() - 14);
}

void AppLauncher::onUpdate() {
    if (_needsRedraw) {
        _drawMenu();
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    int numApps = (int)uiManager.apps().size() - 1;

    if (ki.arrowLeft || ki.arrowUp) {
        _selected = (_selected - 1 + numApps) % numApps;
        _needsRedraw = true;
    } else if (ki.arrowRight || ki.arrowDown) {
        _selected = (_selected + 1) % numApps;
        _needsRedraw = true;
    } else if (ki.enter) {
        uiManager.launchApp(_selected + 1);
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
