#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_qrprox.h"
#include "../hid.h"
#include <WiFi.h>
#include <qrcode.h>

namespace Cardputer {

static constexpr uint16_t QR_BG     = TFT_WHITE;
static constexpr uint16_t QR_FG     = TFT_BLACK;
static constexpr uint16_t APP_BG    = 0x0841;

void AppQRProx::onEnter() {
    _needsRedraw = true;
}

void AppQRProx::_draw() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(APP_BG);

    // Top bar
    uint16_t barBg = disp.color565(0, 80, 100);
    disp.fillRect(0, 0, disp.width(), 16, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("QRProx", 4, 4);

    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    if (!wifiOk) {
        disp.setTextColor(disp.color565(220, 160, 0), APP_BG);
        disp.setTextSize(1);
        disp.drawString("No WiFi connection.", 4, 30);
        disp.setTextColor(disp.color565(140, 140, 140), APP_BG);
        disp.drawString("Connect in Settings.", 4, 46);
        disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
        disp.setTextColor(disp.color565(110, 110, 110), disp.color565(16, 16, 16));
        disp.drawString("ESC back", 2, disp.height() - 13);
        return;
    }

    String ip   = WiFi.localIP().toString();
    String url  = "http://" + ip;

    // Generate QR code (version 3 = up to 61 alphanumeric chars)
    QRCode qr;
    uint8_t qrData[qrcode_getBufferSize(4)];
    qrcode_initText(&qr, qrData, 4, ECC_LOW, url.c_str());

    // Calculate pixel size to fit in the available area
    // Available height: 135 - 16 (bar) - 14 (bottom bar) - 4 (margins) = 101px
    // Available width: 240 - 4 - 120 (text panel) = 116px  → leave right side for text
    int qrAreaW = 116;
    int qrAreaH = 101;
    int startY  = 18;

    int px = min(qrAreaW / qr.size, qrAreaH / qr.size);
    if (px < 1) px = 1;
    int qrPx    = qr.size * px;
    int qrX     = 2 + (qrAreaW - qrPx) / 2;
    int qrY     = startY + (qrAreaH - qrPx) / 2;

    // White background behind QR
    disp.fillRect(qrX - 2, qrY - 2, qrPx + 4, qrPx + 4, QR_BG);

    for (int row = 0; row < qr.size; row++) {
        for (int col = 0; col < qr.size; col++) {
            if (qrcode_getModule(&qr, col, row)) {
                disp.fillRect(qrX + col * px, qrY + row * px, px, px, QR_FG);
            }
        }
    }

    // Right panel — URL text and instructions
    int tx = 122;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(140, 200, 255), APP_BG);
    disp.drawString("Web UI:", tx, startY);

    disp.setTextColor(TFT_WHITE, APP_BG);
    int maxChars = (disp.width() - tx - 2) / 6;
    String urlDisp = url;
    if ((int)urlDisp.length() > maxChars) urlDisp = urlDisp.substring(0, maxChars);
    disp.drawString(urlDisp, tx, startY + 12);

    // IP separately for readability
    disp.setTextColor(TFT_YELLOW, APP_BG);
    disp.drawString(ip, tx, startY + 26);

    // Hostname
    disp.setTextColor(disp.color565(140, 200, 140), APP_BG);
    String hn = String(hostname) + ".local";
    disp.drawString(hn.substring(0, maxChars), tx, startY + 42);

    // SSID
    disp.setTextColor(disp.color565(160, 160, 160), APP_BG);
    disp.drawString("SSID:", tx, startY + 58);
    disp.setTextColor(disp.color565(200, 200, 200), APP_BG);
    String ssidDisp = wifiSSID.substring(0, maxChars);
    disp.drawString(ssidDisp, tx, startY + 70);

    // Bottom bar
    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
    disp.setTextColor(disp.color565(110, 110, 110), disp.color565(16, 16, 16));
    disp.drawString("Scan to open  BtnG0=type URL  ESC", 2, disp.height() - 13);
}

void AppQRProx::onUpdate() {
    if (_needsRedraw) {
        _draw();
        _needsRedraw = false;
    }

    // Physical G0 button — must be checked before the anyKey gate since no
    // keyboard key needs to be pressed alongside it.
    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (WiFi.status() == WL_CONNECTED) {
            sendPlainText("http://" + WiFi.localIP().toString());
        }
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
