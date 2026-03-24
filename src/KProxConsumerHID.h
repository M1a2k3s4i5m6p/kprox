#pragma once
#include "config.h"
#ifdef BOARD_HAS_USB_HID

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>

#define KPROX_CONSUMER_REPORT_ID  3
#define KPROX_SYSTEM_REPORT_ID    4
#define KPROX_EXT_REPORT_ID       5

static const uint8_t KPROX_CONSUMER_DESC[] = {
    // Consumer Control — 18 data bits + 6 padding = 3 bytes
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, KPROX_CONSUMER_REPORT_ID,
    0x05, 0x0C, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x12,
    // byte 0: NextTrack PrevTrack Stop PlayPause Mute VolUp VolDown WWWHome
    0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7, 0x09, 0xCD,
    0x09, 0xE2, 0x09, 0xE9, 0x09, 0xEA, 0x0A, 0x23, 0x02,
    // byte 1: MyComputer Calculator Bookmarks Search WWWStop WWWBack MediaSel Mail
    0x0A, 0x94, 0x01, 0x0A, 0x92, 0x01,
    0x0A, 0x2A, 0x02, 0x0A, 0x21, 0x02,
    0x0A, 0x26, 0x02, 0x0A, 0x24, 0x02,
    0x0A, 0x83, 0x01, 0x0A, 0x8A, 0x01,
    // byte 2 bits 0-1: WWWForward WWWRefresh; bits 2-7 padding
    0x0A, 0x25, 0x02, 0x0A, 0x27, 0x02,
    0x81, 0x02,
    0x95, 0x06, 0x81, 0x03,
    0xC0,
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01,
    0x85, KPROX_SYSTEM_REPORT_ID,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03,
    0x09, 0x81, 0x09, 0x82, 0x09, 0x83,
    0x81, 0x02,
    0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
    0xC0,
    // Extended keyboard keys (International/Lang) — Report ID 5, 11 bits + 5 padding
    0x05, 0x07, 0x09, 0x01, 0xA1, 0x01,
    0x85, KPROX_EXT_REPORT_ID,
    0x05, 0x07, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0B,
    // byte0: KpComma RO KatakanaHiragana Yen Henkan Muhenkan Hanguel Hanja
    0x09, 0x85, 0x09, 0x87, 0x09, 0x88, 0x09, 0x89,
    0x09, 0x8A, 0x09, 0x8B, 0x09, 0x90, 0x09, 0x91,
    // byte1 bits 0-2: Katakana Hiragana Zenkaku/Hankaku
    0x09, 0x92, 0x09, 0x93, 0x09, 0x94,
    0x81, 0x02,
    0x95, 0x05, 0x81, 0x03,
    0xC0
};

class KProxConsumerHID : public USBHIDDevice {
public:
    // addDevice() called in constructor so it runs at static init time —
    // BEFORE USB.begin() builds the config descriptor. This ensures the
    // consumer+system descriptor bytes are included in tinyusb_hid_device_descriptor_len
    // when the host requests the USB configuration descriptor.
    KProxConsumerHID() : _begun(false) {
        _hid.addDevice(this, sizeof(KPROX_CONSUMER_DESC));
    }

    // begin() creates the send semaphore (needed by SendReport)
    void begin() {
        if (_begun) return;
        _hid.begin();
        _begun = true;
    }

    bool sendConsumer(uint8_t b0, uint8_t b1, uint8_t b2 = 0) {
        if (!_begun) return false;
        uint8_t r[3] = {b0, b1, b2};
        return _hid.SendReport(KPROX_CONSUMER_REPORT_ID, r, 3);
    }

    bool sendSystem(uint8_t bits) {
        if (!_begun) return false;
        uint8_t r[1] = {bits};
        return _hid.SendReport(KPROX_SYSTEM_REPORT_ID, r, 1);
    }

    bool sendExtKey(uint8_t b0, uint8_t b1) {
        if (!_begun) return false;
        uint8_t r[2] = {b0, b1};
        if (!_hid.SendReport(KPROX_EXT_REPORT_ID, r, 2)) return false;
        uint8_t z[2] = {0, 0};
        return _hid.SendReport(KPROX_EXT_REPORT_ID, z, 2);
    }

    bool isReady() const { return _begun; }

    uint16_t _onGetDescriptor(uint8_t* buf) override {
        memcpy(buf, KPROX_CONSUMER_DESC, sizeof(KPROX_CONSUMER_DESC));
        return sizeof(KPROX_CONSUMER_DESC);
    }

private:
    USBHID _hid;
    bool   _begun;
};

extern KProxConsumerHID KProxConsumer;
#endif
