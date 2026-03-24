#pragma once

#include "globals.h"
#include <vector>

// Per-token HID output routing. Default is BOTH for all functions.
// USB_ONLY is silently ignored when BOARD_HAS_USB_HID is not defined.
enum class HIDRoute : uint8_t { BOTH = 0, BLE_ONLY = 1, USB_ONLY = 2 };

bool isBLEConnected();
bool isUSBConnected();
bool hasAnyConnection();

void hidReleaseAll();
void hidReleaseAllBLE();
void hidReleaseAllUSB();
void hidPressRaw(uint8_t hidUsage, uint8_t modifiers, HIDRoute route = HIDRoute::BOTH);
void hidReleaseRaw(HIDRoute route = HIDRoute::BOTH);

void sendPlainText(const String& text);
void sendSpecialKey(uint8_t keycode, HIDRoute route = HIDRoute::BOTH);
void sendSpecialKeyRaw(uint8_t hidUsage, HIDRoute route = HIDRoute::BOTH);
void sendSpecialKeyTimed(uint8_t keycode, int holdMs, HIDRoute route = HIDRoute::BOTH);
void pressKey(uint8_t keycode, HIDRoute route = HIDRoute::BOTH);
void pressKeyRaw(uint8_t hidUsage, HIDRoute route = HIDRoute::BOTH);
void releaseKey(uint8_t keycode, HIDRoute route = HIDRoute::BOTH);
void releaseKeyRaw(HIDRoute route = HIDRoute::BOTH);
void sendKeyChord(const std::vector<uint8_t>& keycodes, uint8_t modifiers = 0, HIDRoute route = HIDRoute::BOTH);
void processChord(const String& chordStr, HIDRoute route = HIDRoute::BOTH);

void sendConsumerKey(const MediaKeyReport key, HIDRoute route = HIDRoute::BOTH);
void sendSystemKey(SystemKeyReport key, HIDRoute route = HIDRoute::BOTH);

void setMousePosition(int x, int y, HIDRoute route = HIDRoute::BOTH);
void sendMouseMovement(int deltaX, int deltaY, HIDRoute route = HIDRoute::BOTH);
void sendMouseClick(int button, HIDRoute route = HIDRoute::BOTH);
void sendMousePress(int button, HIDRoute route = HIDRoute::BOTH);
void sendMouseRelease(int button, HIDRoute route = HIDRoute::BOTH);
void sendMouseDoubleClick(int button, HIDRoute route = HIDRoute::BOTH);
void sendMouseScroll(int wheel, HIDRoute route = HIDRoute::BOTH);
void sendMouseHScroll(int hWheel, HIDRoute route = HIDRoute::BOTH);
void sendExtKey(uint8_t b0, uint8_t b1, HIDRoute route = HIDRoute::BOTH);

void sendBatchedMouseMovement();
void accumulateMouseMovement(int16_t deltaX, int16_t deltaY);

void haltAllOperations();
void resumeOperations();
