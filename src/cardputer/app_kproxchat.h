#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"
#include "../nostr_client.h"

extern const uint16_t fa_connectdevelop_48[2304];

namespace Cardputer {

// Fixed community relay and channel — users cannot change these
static constexpr const char* KPROXCHAT_RELAY   = "wss://relay.damus.io";
static constexpr const char* KPROXCHAT_CHANNEL = "#kprox";
static constexpr const char* NVS_KPROXCHAT     = "kprox_chat";

class AppKProxChat : public AppBase {
public:
    void onEnter()  override;
    void onExit()   override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }
    const char* appName() const override { return "KProxChat"; }
    const char* appHelp() const override {
        return "KProx community chat.\n"
               "Open channel on Nostr — no account needed.\n"
               "Page 1: Feed + compose (ENTER to write)\n"
               "Page 2: Status (relay + key info)";
    }
    const uint16_t* appIcon() const override { return fa_connectdevelop_48; }

    static constexpr int BAR_H = 16;
    static constexpr int BOT_H = 14;

private:
    static constexpr int NUM_PAGES  = 2;
    static constexpr uint16_t KC_BG = 0x0009; // dark blue-black

    int  _page        = 0;
    bool _needsRedraw = true;

    // Keys — auto-generated, stored in plain NVS (no CredStore)
    String _privkey;
    String _pubkey;
    String _name;
    bool   _keysReady = false;

    void _initIdentity();   // generate + persist keys and name on first run

    // Page 0 — Feed + Compose
    static constexpr int FEED_Y     = BAR_H + 1;
    static constexpr int INPUT_H    = 15;
    static constexpr int INPUT_Y    = 135 - BOT_H - INPUT_H;
    static constexpr int FEED_BOTTOM = INPUT_Y - 2;
    static constexpr int FEED_ROW_H  = 12;

    int    _feedScroll       = 0;
    bool   _feedScrollPinned = true;
    String _compBuf;
    bool   _compEditing      = false;
    String _compStatus;
    bool   _compStatusOk     = false;
    bool   _pendingRelayCheck = false;

    // Page 1 — Status (read-only) + nick edit
    String _statusMsg;
    bool   _statusOk  = false;
    bool   _nickEditing = false;
    String _nickBuf;

    unsigned long _lastPollMs    = 0;
    unsigned long _lastRedrawMs  = 0;
    unsigned long _lastRefreshMs = 0;
    bool          _autoConnect   = false;
    bool          _fetchPendingMetadata = false;

    NostrClient _client;   // own client instance — independent of nostrClient global

    void _drawTopBar(int page);
    void _drawBottomBar(const char* hint);
    void _drawPage0();
    void _drawPage1();
    void _handlePage0(KeyInput ki);
    void _handlePage1(KeyInput ki);
};

} // namespace Cardputer
#endif
