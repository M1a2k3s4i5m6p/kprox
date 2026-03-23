#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppQRProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "QRProx"; }
    const char* appHelp()  const override { return "Displays a QR code for the web interface.\nScan with phone to open the web UI.\nBtnG0 types the device URL to the host."; }
    const uint16_t* appIcon() const override { return fa_qrcode_48; } // dark teal
    bool handlesGlobalBtnA() const override { return true; }

private:
    bool _needsRedraw = true;
    void _draw();
};

} // namespace Cardputer
#endif
