#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

struct CSRawKey {
    char ch    = 0;
    bool del   = false;
    bool enter = false;
    bool esc   = false;
    bool tab   = false;
    bool any   = false;
};

class AppCredStore : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "CredStore"; }
    uint16_t iconColor() const override  { return 0xF800; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    static constexpr int NUM_PAGES = 3;
    static constexpr unsigned long POLL_INTERVAL_MS = 500;

    int           _page         = 0;
    bool          _needsRedraw  = false;
    unsigned long _lastPollMs   = 0;
    bool          _snapLocked   = true;
    int           _snapCount    = -1;

    String _keyBuf;
    bool   _unlockFailed   = false;
    bool   _confirmingLock = false;

    enum RekeyField { RK_OLD = 0, RK_NEW = 1, RK_CONFIRM = 2 };
    RekeyField _rkField    = RK_OLD;
    String     _rkOld;
    String     _rkNew;
    String     _rkConfirm;
    String     _rkStatus;
    bool       _rkStatusOk = false;

    bool   _wipePrompted = false;
    String _wipeStatus;
    bool   _wipeStatusOk = false;

    void _drawTopBar(int page);
    void _drawPage0();
    void _drawPage1();
    void _drawPage2();
    void _drawConfirmLock();
    void _drawInputField(int x, int y, int w, const String& buf, bool active, bool masked = false);
    void _handlePage0(CSRawKey rk);
    void _handlePage1(CSRawKey rk);
    void _handlePage2(CSRawKey rk);
    void _pollState();
};

} // namespace Cardputer
#endif
