#include "led.h"

void setLED(uint8_t r, uint8_t g, uint8_t b, int duration) {
    if (!ledEnabled) r = g = b = 0;
    leds[0] = CRGB(r, g, b);
    FastLED.show();
    if (duration > 0) {
        delay(duration);
        leds[0] = CRGB(0, 0, 0);
        FastLED.show();
    }
}

void setLED(LEDColor color, int duration) {
    setLED(color.r, color.g, color.b, duration);
}

void blinkLED(int count) {
    blinkLED(count, LED_COLOR_BLINK);
}

void blinkLED(int count, LEDColor color) {
    blinkLED(count, color.r, color.g, color.b, LED_DEFAULT_DUTY_CYCLE);
}

void blinkLED(int count, LEDColor color, int dutyCycle) {
    blinkLED(count, color.r, color.g, color.b, dutyCycle);
}

void blinkLED(int count, uint8_t r, uint8_t g, uint8_t b, int dutyCycle) {
    if (!ledEnabled) return;
    for (int i = 0; i < count; i++) {
        setLED(r, g, b, dutyCycle);
        delay(dutyCycle);
    }
}

void flashTxIndicator() {
    if (!ledEnabled) return;

    bool bleConn = bluetoothEnabled && bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
#ifdef BOARD_HAS_USB_HID
    bool usbConn = usbEnabled && usbInitialized && usbKeyboardReady;
#else
    bool usbConn = false;
#endif

    LEDColor txColor;
    if      (bleConn && usbConn) txColor = LED_COLOR_TX_BOTH;
    else if (bleConn)            txColor = LED_COLOR_TX_BLE;
    else if (usbConn)            txColor = LED_COLOR_TX_USB;
    else                         txColor = LED_COLOR_TX_NO_CONNECTION;

    setLED(txColor, 50);
}
