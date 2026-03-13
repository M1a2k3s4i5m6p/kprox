#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_clock.h"
#include <time.h>
#include <WiFi.h>

namespace Cardputer {

void AppClock::onEnter() {
    _lastDraw = 0;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    _drawClock();
}

void AppClock::_drawClock() {
    auto& disp = M5Cardputer.Display;
    int cx = disp.width() / 2;
    int cy = disp.height() / 2 - 8;

    disp.fillScreen(TFT_BLACK);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    bool validTime = (now > 100000);

    // Large time display
    char timeBuf[16];
    if (validTime) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        strncpy(timeBuf, "--:--:--", sizeof(timeBuf));
    }

    disp.setTextSize(3);
    disp.setTextColor(TFT_WHITE, TFT_BLACK);
    int tw = disp.textWidth(timeBuf);
    disp.drawString(timeBuf, cx - tw / 2, cy - 16);

    // Date
    char dateBuf[24];
    if (validTime) {
        snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    } else {
        strncpy(dateBuf, "No NTP sync", sizeof(dateBuf));
    }
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(150, 150, 150), TFT_BLACK);
    tw = disp.textWidth(dateBuf);
    disp.drawString(dateBuf, cx - tw / 2, cy + 24);

    // WiFi / UTC info
    String info = (WiFi.status() == WL_CONNECTED)
                  ? WiFi.localIP().toString()
                  : "WiFi disconnected";
    disp.setTextColor(disp.color565(80, 80, 80), TFT_BLACK);
    tw = disp.textWidth(info.c_str());
    disp.drawString(info.c_str(), cx - tw / 2, cy + 42);

    // Footer
    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(15, 15, 15));
    disp.setTextColor(disp.color565(80, 80, 80), disp.color565(15, 15, 15));
    disp.drawString("ESC = menu", 4, disp.height() - 12);

    _lastDraw = millis();
}

void AppClock::onUpdate() {
    if (millis() - _lastDraw >= 1000) _drawClock();

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();
    if (ki.esc) uiManager.returnToLauncher();
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
