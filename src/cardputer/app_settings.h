#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppSettings : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "Settings"; }
    uint16_t iconColor() const override { return 0x07E0; }  // green

private:
    // 0=Connectivity, 1=Set WiFi, 2=Identifiers
    int  _page        = 0;
    bool _needsRedraw = true;

    // Page 0: toggle selection
    int  _toggleSel     = 0;
    bool _rebootNote    = false;

    // Page 1: WiFi setup
    enum WifiInputState { WS_SSID, WS_PASS, WS_CONNECTING, WS_DONE };
    WifiInputState _wifiState = WS_SSID;
    String         _wifiInputBuf;
    String         _newSSID;
    String         _wifiStatusMsg;
    bool           _wifiSuccess = false;

    // Page 2: identifier fields
    int    _idSel    = 0;
    bool   _editing  = false;
    String _editBuf;
    bool   _idSaved  = false;

    void _drawTopBar(int pageNum);
    void _drawBottomBar(const char* hint);
    void _drawPage0();
    void _drawPage1();
    void _drawPage2();
    void _handlePage0(KeyInput ki);
    void _handlePage1(KeyInput ki);
    void _handlePage2(KeyInput ki);
    void _connectWifi();
    void _drawInputField(int x, int y, int w, const String& text, bool active, bool masked = false);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
