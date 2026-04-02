#ifndef LED_STRIP_H
#define LED_STRIP_H

#include "JbdBms.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// Split the strip evenly between both batteries.
#define LEDS_PER_BATTERY (NUM_LEDS / 2)

class LedStrip {
public:
  LedStrip();
  void begin();
  void update(uint8_t batteryIndex, const BmsData &data, bool buttonOn, bool faultActive);
  void showEmergency();
  void show();

private:
  Adafruit_NeoPixel strip;

  // Animation state
  uint8_t chargingOffset[2];
  unsigned long lastUpdate;

  uint32_t getSocColor(uint8_t soc);
  void setChargingAnimation(uint8_t batteryIndex, uint8_t soc);
  void setStaticLevel(uint8_t batteryIndex, uint8_t soc);
  void setDisconnected(uint8_t batteryIndex);
  void setError(uint8_t batteryIndex);
  void setFull(uint8_t batteryIndex);
};

#endif
