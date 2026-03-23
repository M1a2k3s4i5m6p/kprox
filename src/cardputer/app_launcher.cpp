#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_launcher.h"

namespace Cardputer {

static constexpr int STATUS_BAR_H = 16;
static constexpr int ICON_SIZE    = 48;
static constexpr int ICON_SEL_SIZE = 58;
static constexpr int ICON_GAP     = 14;
static constexpr int LABEL_H      = 16;
static constexpr int TOP_MARGIN   = STATUS_BAR_H + 4;

static constexpr int VISIBLE_COUNT = 4;

void AppLauncher::onEnter() {
    _needsRedraw = true;
}

void AppLauncher::_drawStatusBar() {
    auto& disp = M5Cardputer.Display;
    uint16_t barBg = disp.color565(20, 20, 20);
    disp.fillRect(0, 0, disp.width(), STATUS_BAR_H, barBg);
    disp.setTextSize(1);

    // App title / branding
    disp.setTextColor(disp.color565(100, 160, 255), barBg);
    disp.drawString("KProx", 4, 4);

    // WiFi status
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    uint16_t wifiColor = wifiOk
        ? disp.color565(80, 200, 80)
        : disp.color565(140, 140, 140);
    disp.setTextColor(wifiColor, barBg);
    disp.drawString(wifiOk ? "WiFi" : "No WiFi", 52, 4);

    // Bluetooth status
    bool btOn = bluetoothEnabled && bluetoothInitialized;
    uint16_t btColor = btOn
        ? (BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected()
            ? disp.color565(100, 160, 255)
            : disp.color565(80, 80, 180))
        : disp.color565(100, 100, 100);
    disp.setTextColor(btColor, barBg);
    disp.drawString(btOn ? "BT" : "--", 108, 4);

    // USB status
#ifdef BOARD_HAS_USB_HID
    bool usbOn = usbEnabled && usbInitialized;
    uint16_t usbColor = usbOn
        ? (usbKeyboardReady
            ? disp.color565(100, 220, 180)
            : disp.color565(60, 130, 100))
        : disp.color565(100, 100, 100);
    disp.setTextColor(usbColor, barBg);
    disp.drawString(usbOn ? "USB" : "---", 126, 4);
#endif

    // Battery level
    int batLevel = M5Cardputer.Power.getBatteryLevel();
    if (batLevel < 0) batLevel = 0;
    if (batLevel > 100) batLevel = 100;

    // Battery icon: outline + fill
    int bx = disp.width() - 34;
    int by = 3;
    int bw = 24;
    int bh = 10;
    // Terminal nub
    disp.fillRect(bx + bw, by + 2, 2, 6, disp.color565(150, 150, 150));
    // Outline
    disp.drawRect(bx, by, bw, bh, disp.color565(150, 150, 150));
    // Fill
    int fillW = (bw - 2) * batLevel / 100;
    uint16_t fillColor = batLevel > 40
        ? disp.color565(60, 200, 60)
        : (batLevel > 15 ? disp.color565(220, 180, 0) : disp.color565(220, 60, 60));
    if (fillW > 0) disp.fillRect(bx + 1, by + 1, fillW, bh - 2, fillColor);

    // Percentage text
    char pct[6];
    snprintf(pct, sizeof(pct), "%d%%", batLevel);
    int pw = disp.textWidth(pct);
    disp.setTextColor(disp.color565(170, 170, 170), barBg);
    disp.drawString(pct, bx - pw - 2, 4);
}

void AppLauncher::_drawIcon(int appIndex, int screenX, bool selected) {
    auto& disp = M5Cardputer.Display;
    int iconSz  = selected ? ICON_SEL_SIZE : ICON_SIZE;
    int yOffset = selected ? 0 : (ICON_SEL_SIZE - ICON_SIZE) / 2;
    int iconY   = TOP_MARGIN + yOffset;

    int slotW = ICON_SEL_SIZE + ICON_GAP;
    int drawX = screenX + (slotW - iconSz) / 2;

    // appIndex is already the real registered index (1-based)
    const uint16_t* icon = uiManager.apps()[appIndex]->appIcon();
    if (icon) {
        // Always push at the fixed 48px size — the array is exactly ICON_SIZE×ICON_SIZE.
        // Centre it within the (larger) selected slot so the selection highlight is
        // provided by position/label colour rather than by scaling the bitmap.
        int imgX = screenX + (slotW - ICON_SIZE) / 2;
        int imgY = TOP_MARGIN + (ICON_SEL_SIZE - ICON_SIZE) / 2;
        disp.pushImage(imgX, imgY, ICON_SIZE, ICON_SIZE, icon);
    } else {
        uint16_t col = uiManager.apps()[appIndex]->iconColor();
        disp.fillRoundRect(drawX, iconY, iconSz, iconSz, 8, col);

        disp.setTextSize(selected ? 3 : 2);
        disp.setTextColor(TFT_WHITE, col);
        char letter[2] = { uiManager.apps()[appIndex]->appName()[0], 0 };
        int tw = disp.textWidth(letter);
        int th = disp.fontHeight();
        disp.drawString(letter, drawX + (iconSz - tw) / 2, iconY + (iconSz - th) / 2);
    }

    const char* name = uiManager.apps()[appIndex]->appName();
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

    _drawStatusBar();

    std::vector<int> visible = uiManager.visibleApps();
    int numApps = (int)visible.size();
    if (numApps <= 0) return;

    // Clamp selection
    if (_selected >= numApps) _selected = numApps - 1;

    int slotW   = ICON_SEL_SIZE + ICON_GAP;
    int centerX = disp.width() / 2;
    int startX  = centerX - _selected * slotW - ICON_SEL_SIZE / 2;

    for (int i = 0; i < numApps; i++) {
        int sx = startX + i * slotW;
        if (sx + slotW < 0 || sx > disp.width()) continue;
        _drawIcon(visible[i], sx, i == _selected);
    }

    disp.setTextSize(1);

    // Active register strip above bottom bar
    {
        int stripY = disp.height() - 25;
        uint16_t stripBg = disp.color565(15, 15, 15);
        disp.fillRect(0, stripY, disp.width(), 12, stripBg);
        disp.setTextColor(disp.color565(70, 70, 70), stripBg);
        if (registers.empty()) {
            disp.drawString("No registers", 4, stripY + 2);
        } else {
            String rname = (activeRegister < (int)registerNames.size())
                ? registerNames[activeRegister] : "";
            String rtxt = "Active: [" + String(activeRegister + 1) + "]";
            if (!rname.isEmpty()) rtxt += " " + rname;
            if ((int)rtxt.length() > 30) rtxt = rtxt.substring(0, 28) + "..";
            disp.setTextColor(disp.color565(100, 140, 100), stripBg);
            disp.drawString(rtxt, 4, stripY + 2);
        }
    }

    // Bottom hint
    disp.setTextColor(disp.color565(70, 70, 70), TFT_BLACK);
    disp.drawString("< > select   ENTER open   Tab help", 4, disp.height() - 11);
}

void AppLauncher::onUpdate() {
    if (_needsRedraw) {
        _drawMenu();
        _needsRedraw = false;
    }

    // Refresh status bar every 10s for battery
    if (millis() - _lastStatusUpdate > 10000) {
        _drawStatusBar();
        _lastStatusUpdate = millis();
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    std::vector<int> visible = uiManager.visibleApps();
    int numApps = (int)visible.size();

    if (ki.arrowLeft || ki.arrowUp) {
        _selected = (_selected - 1 + numApps) % numApps;
        _needsRedraw = true;
    } else if (ki.arrowRight || ki.arrowDown) {
        _selected = (_selected + 1) % numApps;
        _needsRedraw = true;
    } else if (ki.enter) {
        if (_selected < numApps) uiManager.launchApp(visible[_selected]);
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
