#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_clock.h"
#include <time.h>
#include <WiFi.h>

namespace Cardputer {

static constexpr int CLOCK_BAR_H = 18;

void AppClock::onEnter() {
    _lastDraw = 0;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    _drawClock();
}

void AppClock::_drawClock() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TFT_BLACK);

    uint16_t barColor = disp.color565(30, 80, 160);
    disp.fillRect(0, 0, disp.width(), CLOCK_BAR_H, barColor);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barColor);
    disp.drawString("Clock", 4, 3);

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        int iw = disp.textWidth(ip.c_str());
        disp.drawString(ip.c_str(), disp.width() - iw - 4, 3);
    } else {
        disp.setTextColor(disp.color565(180, 80, 80), barColor);
        disp.drawString("No WiFi", disp.width() - disp.textWidth("No WiFi") - 4, 3);
    }

    int contentH = disp.height() - CLOCK_BAR_H - 14;
    int cx       = disp.width() / 2;
    int cy       = CLOCK_BAR_H + contentH / 2;

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    bool validTime = (now > 100000);

    char timeBuf[16];
    if (validTime) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        strncpy(timeBuf, "--:--:--", sizeof(timeBuf));
    }

    disp.setTextSize(3);
    disp.setTextColor(TFT_WHITE, TFT_BLACK);
    int tw = disp.textWidth(timeBuf);
    disp.drawString(timeBuf, cx - tw / 2, cy - 14);

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
    disp.drawString(dateBuf, cx - tw / 2, cy + 16);

    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
    disp.setTextColor(disp.color565(80, 80, 80), disp.color565(16, 16, 16));
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
