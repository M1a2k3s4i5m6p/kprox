#pragma once

#include "globals.h"

void setLED(uint8_t r, uint8_t g, uint8_t b, int duration = 0);
void setLED(LEDColor color, int duration = 0);
void blinkLED(int count);
void blinkLED(int count, LEDColor color);
void blinkLED(int count, LEDColor color, int dutyCycle);
void blinkLED(int count, uint8_t r, uint8_t g, uint8_t b, int dutyCycle);
void flashTxIndicator();
