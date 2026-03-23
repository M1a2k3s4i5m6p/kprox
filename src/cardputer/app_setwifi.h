#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppSetWifi : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "SetWiFi"; }
    const char* appHelp()  const override { return "Configure WiFi credentials.\nType SSID and password then save.\nDevice reconnects automatically."; }
    uint16_t iconColor() const override { return 0x065F; }  // ~rgb(0,100,200)

private:
    enum State { S_SSID, S_PASS, S_CONNECTING, S_DONE };
    State   _state   = S_SSID;
    String  _ssid;
    String  _pass;
    String  _inputBuf;
    bool    _dirty   = true;

    void _render();
    void _connect();
    void _appendLine(const char* text, uint16_t color);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
