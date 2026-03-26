#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_qrprox.h"
#include "../hid.h"
#include "../connection.h"
#include "../led.h"
#include <WiFi.h>
#include <qrcode.h>

namespace Cardputer {

static constexpr uint16_t QR_BG  = TFT_WHITE;
static constexpr uint16_t QR_FG  = TFT_BLACK;
static constexpr uint16_t APP_BG = 0x0841;

void AppQRProx::onEnter() {
    _needsRedraw  = true;
    _lastPollMs   = 0;
    _dotCount     = 0;
}

void AppQRProx::requestRedraw() { _needsRedraw = true; }

// ── Connecting screen ─────────────────────────────────────────────────────────

void AppQRProx::_drawConnecting() {
    auto& d = M5Cardputer.Display;
    const uint16_t BG   = d.color565(18, 18, 28);
    const uint16_t BAR  = d.color565(20, 60, 130);
    const uint16_t WARN = d.color565(220, 160, 0);
    const uint16_t DIM  = d.color565(130, 130, 130);
    const uint16_t VAL  = d.color565(100, 200, 255);
    const uint16_t DOT  = d.color565(80, 140, 220);

    d.fillScreen(BG);
    d.fillRect(0, 0, d.width(), 18, BAR);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, BAR);
    d.drawString("QRProx", 4, 4);
    d.setTextColor(d.color565(160, 200, 255), BAR);
    d.drawString("Connecting...", d.width() - d.textWidth("Connecting...") - 4, 4);

    int y = 24;
    d.setTextColor(DIM, BG);
    d.drawString("SSID ", 4, y);
    d.setTextColor(VAL, BG);
    String ssidDisp = wifiSSID;
    if ((int)ssidDisp.length() > 26) ssidDisp = ssidDisp.substring(0, 23) + "...";
    d.drawString(ssidDisp, 4 + d.textWidth("SSID "), y);
    y += 14;

    if (wifiPassword == DEFAULT_WIFI_PASSWORD) {
        d.setTextColor(WARN, BG);
        d.drawString("! Default pw: " + wifiPassword, 4, y);
        y += 12;
        d.drawString("  Change in Settings app", 4, y);
        y += 14;
    }

    // Animated dots
    y += 6;
    d.setTextColor(DIM, BG);
    d.drawString("Connecting", 4, y);
    int dx = 4 + d.textWidth("Connecting") + 4;
    for (int i = 0; i < _dotCount && i < 20; i++) {
        int px = dx + i * 7;
        int py = y;
        if (px + 5 > d.width()) { px = dx; py += 14; }
        d.fillCircle(px + 2, py + 5, 2, DOT);
    }

    d.fillRect(0, d.height() - 14, d.width(), 14, d.color565(16, 16, 16));
    d.setTextColor(d.color565(110, 110, 110), d.color565(16, 16, 16));
    d.drawString("ESC back  Settings to change WiFi", 2, d.height() - 13);
}

// ── QR screen ─────────────────────────────────────────────────────────────────

void AppQRProx::_drawQR() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(APP_BG);

    uint16_t barBg = d.color565(0, 80, 100);
    d.fillRect(0, 0, d.width(), 16, barBg);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, barBg);
    d.drawString("QRProx", 4, 4);
    drawTabHint(4 + d.textWidth("QRProx") + 3);

    String ip  = WiFi.localIP().toString();
    String url = "http://" + ip;

    QRCode qr;
    uint8_t qrData[qrcode_getBufferSize(4)];
    qrcode_initText(&qr, qrData, 4, ECC_LOW, url.c_str());

    int qrAreaW = 116, qrAreaH = 101, startY = 18;
    int px = min(qrAreaW / qr.size, qrAreaH / qr.size);
    if (px < 1) px = 1;
    int qrPx = qr.size * px;
    int qrX  = 2 + (qrAreaW - qrPx) / 2;
    int qrY  = startY + (qrAreaH - qrPx) / 2;

    d.fillRect(qrX - 2, qrY - 2, qrPx + 4, qrPx + 4, QR_BG);
    for (int row = 0; row < qr.size; row++)
        for (int col = 0; col < qr.size; col++)
            if (qrcode_getModule(&qr, col, row))
                d.fillRect(qrX + col*px, qrY + row*px, px, px, QR_FG);

    int tx = 122;
    int maxChars = (d.width() - tx - 2) / 6;
    d.setTextSize(1);
    d.setTextColor(d.color565(140, 200, 255), APP_BG);
    d.drawString("Web UI:", tx, startY);
    d.setTextColor(TFT_WHITE, APP_BG);
    String urlDisp = url;
    if ((int)urlDisp.length() > maxChars) urlDisp = urlDisp.substring(0, maxChars);
    d.drawString(urlDisp, tx, startY + 12);
    d.setTextColor(TFT_YELLOW, APP_BG);
    d.drawString(ip, tx, startY + 26);
    d.setTextColor(d.color565(140, 200, 140), APP_BG);
    String hn = String(hostname) + ".local";
    d.drawString(hn.substring(0, maxChars), tx, startY + 42);
    d.setTextColor(d.color565(160, 160, 160), APP_BG);
    d.drawString("SSID:", tx, startY + 58);
    d.setTextColor(d.color565(200, 200, 200), APP_BG);
    d.drawString(wifiSSID.substring(0, maxChars), tx, startY + 70);

    d.fillRect(0, d.height() - 14, d.width(), 14, d.color565(16, 16, 16));
    d.setTextColor(d.color565(110, 110, 110), d.color565(16, 16, 16));
    d.drawString("Scan to open  BtnG0=type URL  ESC", 2, d.height() - 13);
}

// ── Update ────────────────────────────────────────────────────────────────────

void AppQRProx::onUpdate() {
    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    // Re-draw if connection state changed
    if (wifiOk != _wasConnected) {
        _wasConnected = wifiOk;
        _needsRedraw  = true;
        if (wifiOk) {
            // WiFi just connected — start NTP and LED
            initNTP();
            if (ledEnabled) blinkLED(3, LED_COLOR_WIFI_CONNECTED, LED_COLOR_WIFI_CONNECTED_DUTY_CYCLE);
        }
    }

    if (!wifiOk) {
        // Animate connecting dots every 500ms
        unsigned long now = millis();
        if (now - _lastPollMs >= 500) {
            _lastPollMs = now;
            _dotCount++;
            _drawConnecting();
            _needsRedraw = false;
        } else if (_needsRedraw) {
            _drawConnecting();
            _needsRedraw = false;
        }
    } else if (_needsRedraw) {
        _drawQR();
        _needsRedraw = false;
    }

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (wifiOk) sendPlainText("http://" + WiFi.localIP().toString());
        return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) {
        uiManager.returnToLauncher();
    } else {
        _needsRedraw = true;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
