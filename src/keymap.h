#pragma once
#include "globals.h"
#include <map>
#include <vector>

struct KeyEntry {
    uint8_t hidUsage;   // raw USB HID Usage ID
    uint8_t modifiers;  // HID modifier bitmask: 0x01=LCtrl 0x02=LShift 0x04=LAlt 0x08=LGUI 0x40=RAlt(AltGr)
};

// Modifier bitmask constants matching USB HID spec (NOT the BLE library's translated values)
#define KM_MOD_LSHIFT  0x02
#define KM_MOD_LALT    0x04
#define KM_MOD_RALT    0x40  // AltGr

extern String activeKeymap;

void keymapInit();
bool keymapLoad(const String& id);
bool keymapExists(const String& id);
bool keymapLookup(uint16_t codepoint, KeyEntry& out);
void keymapSaveActive();
String keymapActive();
std::vector<String> keymapListAvailable();
bool keymapDelete(const String& id);
bool keymapUpload(const String& id, const String& json);
