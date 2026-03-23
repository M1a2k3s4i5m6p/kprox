#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include <WiFi.h>

namespace Cardputer {

class AppLauncher : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* appName() const override { return "Launcher"; }
    const char* appHelp()  const override {
        return "KProx turns this Cardputer into a one-button programmable HID device.\n\nLAUNCHER: arrow keys scroll through apps, ENTER opens one.\n\nBUTTON (BtnG0): single press plays the active register on the paired host. Two presses cycles to the next register.\n\nFIRST STEPS:\n1. Connect to WiFi in Settings.\n2. Open http://kprox.local in a browser.\n3. Change the API key (default: kprox1337).\n4. Edit Register 1 in the web Registers tab and press Play.\n\nCREDENTIALS: unlock the CredStore app before using {CREDSTORE} tokens.\n\nTap TAB on any app for its help page."; }


private:
    int           _selected          = 0;
    bool          _needsRedraw       = true;
    unsigned long _lastStatusUpdate  = 0;

    void _drawMenu();
    void _drawStatusBar();
    void _drawIcon(int appIndex, int screenX, bool selected);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
