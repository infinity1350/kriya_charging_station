#include "LedStrip.h"

// Colors (GRB format - parameters are swapped for WS2812)
#define COLOR_OFF strip.Color(0, 0, 0)
#define COLOR_GREEN strip.Color(0, 255, 0)
#define COLOR_YELLOW strip.Color(255, 180, 0)
#define COLOR_ORANGE strip.Color(255, 100, 0)
#define COLOR_RED strip.Color(255, 0, 0)
#define COLOR_CYAN strip.Color(0, 255, 255)
#define COLOR_BLUE strip.Color(0, 0, 255)
#define COLOR_WHITE strip.Color(255, 255, 255)

LedStrip::LedStrip() : strip(NUM_LEDS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800) {
  chargingOffset[0] = 0;
  chargingOffset[1] = 0;
  lastUpdate = 0;
}

void LedStrip::begin() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();
}

uint32_t LedStrip::getSocColor(uint8_t soc) {
  if (soc < 20)
    return COLOR_RED;
  if (soc < 40)
    return COLOR_ORANGE;
  if (soc < 60)
    return COLOR_YELLOW;
  return COLOR_GREEN;
}

void LedStrip::setStaticLevel(uint8_t batteryIndex, uint8_t soc) {
  // Calculate LED range for this battery
  uint8_t startLed = batteryIndex * LEDS_PER_BATTERY;
  uint8_t endLed = startLed + LEDS_PER_BATTERY;

  // Solid blue when connected and idle.
  for (uint8_t i = startLed; i < endLed; i++) {
    strip.setPixelColor(i, COLOR_BLUE);
  }
}

void LedStrip::setChargingAnimation(uint8_t batteryIndex, uint8_t soc) {
  uint8_t startLed = batteryIndex * LEDS_PER_BATTERY;
  uint8_t endLed = startLed + LEDS_PER_BATTERY;

  // Blink all LEDs green — toggles every 500ms
  bool on = (millis() / 500) % 2 == 0;
  uint32_t color = on ? COLOR_YELLOW : COLOR_OFF;

  for (uint8_t i = startLed; i < endLed; i++) {
    strip.setPixelColor(i, color);
  }
}


void LedStrip::setDisconnected(uint8_t batteryIndex) {
  uint8_t startLed = batteryIndex * LEDS_PER_BATTERY;
  uint8_t endLed = startLed + LEDS_PER_BATTERY;

  for (uint8_t i = startLed; i < endLed; i++) {
    strip.setPixelColor(i, COLOR_BLUE);
  }
}

void LedStrip::setError(uint8_t batteryIndex) {
  uint8_t startLed = batteryIndex * LEDS_PER_BATTERY;
  uint8_t endLed = startLed + LEDS_PER_BATTERY;

  // Flash red pattern for protection errors
  bool on = (millis() / 500) % 2 == 0;
  uint32_t color = on ? COLOR_RED : COLOR_OFF;

  for (uint8_t i = startLed; i < endLed; i++) {
    strip.setPixelColor(i, color);
  }
}

void LedStrip::setFull(uint8_t batteryIndex) {
  uint8_t startLed = batteryIndex * LEDS_PER_BATTERY;
  uint8_t endLed = startLed + LEDS_PER_BATTERY;

  // Rainbow pulse or solid green
  for (uint8_t i = startLed; i < endLed; i++) {
    strip.setPixelColor(i, COLOR_GREEN);
  }
}

void LedStrip::showEmergency() {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, COLOR_RED);
  }
}

void LedStrip::update(uint8_t batteryIndex, const BmsData &data, bool buttonOn, bool faultActive) {
  if (batteryIndex >= 2)
    return;

  bool chargingActive =
      buttonOn && data.isConnected && data.chargeMosfetOn &&
      data.soc < MAX_STOP_SOC && data.voltage < MAX_STOP_VOLTAGE;

  // Update animation offset every 100ms
  if (millis() - lastUpdate > 100) {
    chargingOffset[0]++;
    chargingOffset[1]++;
    lastUpdate = millis();
  }

  if (!buttonOn || !data.isConnected) {
    setDisconnected(batteryIndex);        // Solid blue
  } else if (data.soc >= MAX_STOP_SOC || data.voltage >= MAX_STOP_VOLTAGE) {
    setFull(batteryIndex);                // Solid green when the pack is full
  } else if (faultActive) {
    setError(batteryIndex);               // Blinking red for protection faults
  } else if (chargingActive) {
    setChargingAnimation(batteryIndex, data.soc); // Blinking yellow while charging
  } else {
    setStaticLevel(batteryIndex, data.soc);       // Idle bar
  }
}

void LedStrip::show() { strip.show(); }
