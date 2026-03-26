#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppQRProx : public AppBase {
public:
    void onEnter()      override;
    void onUpdate()     override;
    void requestRedraw() override;

    const char* appName() const override { return "QRProx"; }
    const char* appHelp() const override {
        return "Displays a QR code for the web interface.\n"
               "Shows WiFi connecting screen until connected.\n"
               "Scan with phone to open the web UI.\n"
               "BtnG0 types the device URL to the host.";
    }
    const uint16_t* appIcon() const override { return fa_qrcode_48; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    bool          _needsRedraw  = true;
    bool          _wasConnected = false;
    unsigned long _lastPollMs   = 0;
    int           _dotCount     = 0;

    void _drawConnecting();
    void _drawQR();
};

} // namespace Cardputer
#endif
