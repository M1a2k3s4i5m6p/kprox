#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"
#include "../nostr_client.h"

extern const uint16_t fa_comments_48[2304];

namespace Cardputer {

class AppNostrProx : public AppBase {
public:
    void onEnter()  override;
    void onExit()   override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }
    const char* appName() const override { return "NostrProx"; }
    const char* appHelp() const override {
        return "Nostr decentralised chat.\n"
               "Keys stored in CredStore (unlock first).\n"
               "Page 1: Feed + compose (ENTER to write)\n"
               "Page 2: Status + connect/keygen\n"
               "Page 3: Config (relay + channel)";
    }
    const uint16_t* appIcon() const override { return fa_comments_48; }

    static constexpr int BAR_H = 16;
    static constexpr int BOT_H = 14;

private:
    static constexpr int NUM_PAGES     = 3;
    static constexpr uint16_t NP_BG    = 0x180E;

    int  _page        = 1;
    bool _needsRedraw = true;

    // Keys / config
    String _privkey;
    String _pubkey;
    String _relay;
    String _channel;
    bool   _keysLoaded  = false;
    bool _reloadKeys();
    void _saveKeys();
    void _saveConfig();

    // Page 0 — Feed + Compose
    int    _feedScroll       = 0;
    bool   _feedScrollPinned = true;
    String _compBuf;
    bool   _compEditing   = false;
    String _compStatus;
    bool   _compStatusOk  = false;
    bool   _pendingRelayCheck = false;

    // Page 1 — Status
    String _p0Status;
    bool   _p0StatusOk   = false;
    bool   _p0GenConfirm = false;

    // Page 2 — Config
    enum CfgField { CF_RELAY = 0, CF_CHANNEL = 1, CF_COUNT = 2 };
    CfgField _cfgSel     = CF_RELAY;
    bool     _cfgEditing = false;
    String   _cfgBuf;
    String   _cfgStatus;
    bool     _cfgStatusOk = false;

    unsigned long _lastPollMs    = 0;
    unsigned long _lastRedrawMs  = 0;
    unsigned long _lastRefreshMs = 0;
    bool          _autoConnect   = false;

    void _drawTopBar(int page, const char* extra = nullptr);
    void _drawBottomBar(const char* hint);

    void _drawPage0();   // Feed + Compose
    void _drawPage1();   // Status
    void _drawPage2();   // Config

    void _handlePage0(KeyInput ki);
    void _handlePage1(KeyInput ki);
    void _handlePage2(KeyInput ki);
};

} // namespace Cardputer
#endif
