#ifndef STATION_UI_H
#define STATION_UI_H

#include "JbdBms.h"
#include "config.h"
#include <TFT_eSPI.h>

class StationUI {
public:
  StationUI();
  void begin();
  void updateBattery(uint8_t index, const BmsData &data, bool faultActive);
  void updateButtonState(uint8_t index, bool isOn);
  void showInputDebug(bool e1, bool e2, bool t1, bool t2);
  void showMessage(const char *msg);
  void showEmergency();
  void resetState();
  void requestFullRedraw();

private:
  TFT_eSPI tft;
  bool needsFullRedraw;
  bool emergencyBlinkState;
  unsigned long lastEmergencyBlink;
  BmsData lastData[2];
  BmsData lastRenderedData[2];
  bool renderedFaultStates[2];
  bool buttonStates[2];
  bool hasRenderedData[2];
  unsigned long lastCardDrawMs[2];

  void drawFrame();
  void drawBatteryCard(uint8_t index, const BmsData &data, bool faultActive);
  void drawBatteryGauge(int x, int y, uint8_t soc, bool buttonOn, bool charging,
                        bool connected);
  void printCentered(const char *text, int y, uint16_t color, uint8_t textSize);
  uint16_t getSocColor(uint8_t soc);
  bool shouldRedrawCard(uint8_t index, const BmsData &data, bool faultActive);
};

#endif
