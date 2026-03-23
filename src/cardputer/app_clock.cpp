#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_clock.h"
#include "../storage.h"
#include <time.h>
#include <WiFi.h>

namespace Cardputer {

// Comprehensive timezone table: abbreviation + UTC offset in seconds.
// Sorted by offset for readability; duplicates intentional (region variants).
const TZEntry AppClock::_tzTable[] = {
    { "UTC-12",  -43200 },
    { "BIT",     -43200 },
    { "NUT",     -39600 },
    { "HST",     -36000 },
    { "AKST",    -32400 },
    { "AKDT",    -28800 },
    { "PST",     -28800 },
    { "PDT",     -25200 },
    { "MST",     -25200 },
    { "MDT",     -21600 },
    { "CST",     -21600 },
    { "CDT",     -18000 },
    { "EST",     -18000 },
    { "EDT",     -14400 },
    { "AST",     -14400 },
    { "NST",     -12600 },
    { "NDT",      -9000 },
    { "BRT",     -10800 },
    { "ART",     -10800 },
    { "FKST",     -9000 },
    { "UTC-2",    -7200 },
    { "AZOT",     -3600 },
    { "CVT",      -3600 },
    { "UTC",          0 },
    { "GMT",          0 },
    { "WET",          0 },
    { "BST",       3600 },
    { "CET",       3600 },
    { "MET",       3600 },
    { "WAT",       3600 },
    { "WEST",      3600 },
    { "CEST",      7200 },
    { "CAT",       7200 },
    { "EET",       7200 },
    { "SAST",      7200 },
    { "IST-IL",    7200 },
    { "EEST",     10800 },
    { "AST-AR",   10800 },
    { "EAT",      10800 },
    { "MSK",      10800 },
    { "IRST",     12600 },
    { "GST",      14400 },
    { "AZT",      14400 },
    { "MUT",      14400 },
    { "AFT",      16200 },
    { "PKT",      18000 },
    { "YEKT",     18000 },
    { "IST",      19800 },
    { "NPT",      20700 },
    { "BST-BD",   21600 },
    { "BTT",      21600 },
    { "OMST",     21600 },
    { "MMT",      23400 },
    { "ICT",      25200 },
    { "KRAT",     25200 },
    { "WIB",      25200 },
    { "CST-CN",   28800 },
    { "HKT",      28800 },
    { "SGT",      28800 },
    { "AWST",     28800 },
    { "IRKT",     28800 },
    { "JST",      32400 },
    { "KST",      32400 },
    { "WIT",      32400 },
    { "ACST",     34200 },
    { "AEST",     36000 },
    { "YAKT",     32400 },
    { "VLAT",     36000 },
    { "ACDT",     37800 },
    { "AEDT",     39600 },
    { "SBT",      39600 },
    { "NFT",      39600 },
    { "NZST",     43200 },
    { "FJT",      43200 },
    { "CHAST",    45900 },
    { "NZDT",     46800 },
    { "LINT",     50400 },
};
const int AppClock::_tzCount = sizeof(AppClock::_tzTable) / sizeof(AppClock::_tzTable[0]);

int AppClock::_findTZByOffset(long offsetSec) {
    // Find closest match
    int best = 0;
    long bestDiff = abs((long)_tzTable[0].offsetSeconds - offsetSec);
    for (int i = 1; i < _tzCount; i++) {
        long diff = abs((long)_tzTable[i].offsetSeconds - offsetSec);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    return best;
}

void AppClock::_applyTZ() {
    utcOffsetSeconds = _tzTable[_tzIdx].offsetSeconds;
    saveUtcOffsetSettings();
    configTime(utcOffsetSeconds, 0, "pool.ntp.org", "time.nist.gov");
}

void AppClock::onEnter() {
    _tzIdx    = _findTZByOffset(utcOffsetSeconds);
    _lastDraw = 0;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    _drawClock();
}

void AppClock::_drawClock() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TFT_BLACK);

    uint16_t barColor = disp.color565(20, 60, 130);
    disp.fillRect(0, 0, disp.width(), 18, barColor);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barColor);
    disp.drawString("Clock", 4, 3);
    drawTabHint(4 + disp.textWidth("Clock") + 3);

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        int iw = disp.textWidth(ip.c_str());
        disp.drawString(ip.c_str(), disp.width() - iw - 4, 3);
    } else {
        disp.setTextColor(disp.color565(180, 80, 80), barColor);
        disp.drawString("No WiFi", disp.width() - disp.textWidth("No WiFi") - 4, 3);
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    bool validTime = (now > 100000);

    int cx = disp.width() / 2;
    int bodyTop = 18 + 4;

    // Timezone row — above the clock
    {
        const TZEntry& tz = _tzTable[_tzIdx];
        int offsetH = tz.offsetSeconds / 3600;
        int offsetM = abs((tz.offsetSeconds % 3600) / 60);
        char tzBuf[24];
        if (offsetM == 0)
            snprintf(tzBuf, sizeof(tzBuf), "%s  UTC%+d", tz.abbr, offsetH);
        else
            snprintf(tzBuf, sizeof(tzBuf), "%s  UTC%+d:%02d", tz.abbr, offsetH, offsetM);
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(100, 200, 255), TFT_BLACK);
        int tw = disp.textWidth(tzBuf);
        disp.drawString(tzBuf, cx - tw / 2, bodyTop);
    }

    // Time — large, centred
    char timeBuf[16];
    if (validTime)
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    else
        strncpy(timeBuf, "--:--:--", sizeof(timeBuf));

    int timeY = bodyTop + 20;
    disp.setTextSize(3);
    disp.setTextColor(TFT_WHITE, TFT_BLACK);
    int tw3 = disp.textWidth(timeBuf);
    disp.drawString(timeBuf, cx - tw3 / 2, timeY);

    // Date row
    char dateBuf[24];
    if (validTime)
        snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    else
        strncpy(dateBuf, "No NTP sync", sizeof(dateBuf));

    disp.setTextSize(1);
    disp.setTextColor(disp.color565(150, 150, 150), TFT_BLACK);
    int tw1 = disp.textWidth(dateBuf);
    disp.drawString(dateBuf, cx - tw1 / 2, timeY + 28);

    // Bottom hint
    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
    disp.setTextColor(disp.color565(80, 80, 80), disp.color565(16, 16, 16));
    disp.drawString("up/dn timezone  ESC menu", 4, disp.height() - 12);

    _lastDraw = millis();
}

void AppClock::onUpdate() {
    if (millis() - _lastDraw >= 1000) _drawClock();

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    if (ki.arrowUp || ki.arrowLeft) {
        _tzIdx = (_tzIdx - 1 + _tzCount) % _tzCount;
        _applyTZ();
        _drawClock();
        return;
    }
    if (ki.arrowDown || ki.arrowRight) {
        _tzIdx = (_tzIdx + 1) % _tzCount;
        _applyTZ();
        _drawClock();
        return;
    }
}

} // namespace Cardputer
#endif
