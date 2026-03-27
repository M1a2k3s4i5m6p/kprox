#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"
#include <WiFiClientSecure.h>
#include <WiFiClient.h>

namespace Cardputer {

#define IRC_MSG_MAX     75
#define IRC_CONTENT_MAX 160
#define IRC_NICK_MAX    32

struct IrcMsg {
    char nick[17];
    char content[IRC_CONTENT_MAX + 1];
};

enum class IrcState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    REGISTERING,
    CONNECTED,
    ERROR
};

class AppIRCProx : public AppBase {
public:
    void onEnter()  override;
    void onExit()   override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }
    const char* appName() const override { return "IRCProx"; }
    const char* appHelp() const override {
        return "IRC chat client.\n"
               "Page 1: Feed + compose (ENTER to write)\n"
               "Page 2: Status + connect\n"
               "Page 3: Config (server, channel, nick)";
    }
    const uint16_t* appIcon() const override { return fa_circle_nodes_48; }

    static constexpr int BAR_H = 16;
    static constexpr int BOT_H = 14;

private:
    static constexpr int NUM_PAGES  = 3;
    static constexpr uint16_t IP_BG = 0x000E; // dark navy

    int  _page        = 0;
    bool _needsRedraw = true;

    // Connection config
    String _server;
    String _channel;
    String _nick;
    String _password;          // stored in CredStore; empty = unregistered
    bool   _saslPending = false;
    void _loadConfig();
    void _saveConfig();
    void _savePassword();
    bool _hasPassword() const { return !credStoreLocked && !_password.isEmpty(); }

    // IRC TCP client — heap-allocated so each connect gets a fresh TLS context
    WiFiClientSecure* _secureCli = nullptr;
    WiFiClient*       _plainCli  = nullptr;
    Client*           _client    = nullptr;
    IrcState         _state    = IrcState::DISCONNECTED;
    String           _rxBuf;
    String           _lastError;
    String           _lastSysMsg;

    bool _connect();
    void _disconnect();
    void _sendRaw(const String& line);
    void _poll();
    void _processLine(const String& line);
    bool _publish(const String& msg);

    // Message ring buffer (newest first)
    IrcMsg _msgs[IRC_MSG_MAX];
    int    _msgCount = 0;
    void   _pushMsg(const char* nick, const char* content);

    // Page 0 — Feed + Compose
    int    _feedScroll       = 0;
    bool   _feedScrollPinned = true;  // auto-follow newest message
    String _compBuf;
    bool   _compEditing = false;
    String _compStatus;
    bool   _compStatusOk = false;

    // Page 1 — Status
    String _p1Status;
    bool   _p1StatusOk = false;

    // Page 2 — Config
    enum CfgField { CF_SERVER = 0, CF_CHANNEL = 1, CF_NICK = 2, CF_PASSWORD = 3, CF_COUNT = 4 };
    CfgField _cfgSel     = CF_SERVER;
    bool     _cfgEditing = false;
    String   _cfgBuf;
    String   _cfgStatus;
    bool     _cfgStatusOk = false;

    unsigned long _lastPollMs  = 0;
    unsigned long _lastPingMs  = 0;
    unsigned long _lastRxMs    = 0;
    bool          _autoConnect      = false;
    bool          _capAcked         = false;
    bool          _historyRequested = false;
    bool          _joinedChannel    = false;
    String        _capLsAccum;
    String        _histStatus;   // diagnostic: shown on status page

    // State-change tracking to prevent spurious redraws
    IrcState      _prevState    = IrcState::DISCONNECTED;
    int           _prevMsgCount = 0;
    String        _prevSysMsg;

    void _drawTopBar(int page, const char* extra = nullptr);
    void _drawBottomBar(const char* hint);

    void _drawPage0();
    void _drawPage1();
    void _drawPage2();

    void _handlePage0(KeyInput ki);
    void _handlePage1(KeyInput ki);
    void _handlePage2(KeyInput ki);
};

} // namespace Cardputer
#endif
