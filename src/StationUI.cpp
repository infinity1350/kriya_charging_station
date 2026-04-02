#include "StationUI.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Minimalistic black UI palette
#define BG_COLOR 0x0000
#define TEXT_PRIMARY 0xFFFF
#define TEXT_SECONDARY 0x7BEF
#define TEXT_MUTED 0x4A69
#define ACCENT_CYAN 0x07FF
#define ACCENT_GREEN 0x07E0
#define ACCENT_ORANGE 0xFD20
#define ACCENT_RED 0xF800
#define BORDER_LIGHT 0x3186
#define BORDER_ACTIVE 0xFFE0
#define CARD_DIM 0x1082

// Layout
#define SCREEN_W 320
#define SCREEN_H 240
#define HEADER_Y 24
#define CARD_Y 35
#define CARD_W 145
#define CARD_H 195
#define BAT1_X 10
#define BAT2_X 165

StationUI::StationUI()
    : tft(TFT_eSPI()), needsFullRedraw(true), emergencyBlinkState(true),
      lastEmergencyBlink(0) {
  memset(lastData, 0, sizeof(lastData));
  memset(lastRenderedData, 0, sizeof(lastRenderedData));
  renderedFaultStates[0] = false;
  renderedFaultStates[1] = false;
  buttonStates[0] = false;
  buttonStates[1] = false;
  hasRenderedData[0] = false;
  hasRenderedData[1] = false;
  lastCardDrawMs[0] = 0;
  lastCardDrawMs[1] = 0;
}

void StationUI::begin() {
  tft.init();
  tft.setRotation(TFT_ORIENTATION);
  drawFrame();
}

void StationUI::drawFrame() {
  tft.setRotation(TFT_ORIENTATION);
  tft.fillScreen(BG_COLOR);
  tft.drawFastHLine(0, HEADER_Y, SCREEN_W, BORDER_LIGHT);

  tft.setTextSize(2);
  printCentered("CHARGING STATION", 6, TEXT_PRIMARY, 2);

  needsFullRedraw = false;
}

void StationUI::updateBattery(uint8_t index, const BmsData &data, bool faultActive) {
  tft.setRotation(TFT_ORIENTATION);

  bool forceCardRedraw = needsFullRedraw;
  if (needsFullRedraw) {
    drawFrame();
  }

  lastData[index] = data;
  if (!forceCardRedraw && !shouldRedrawCard(index, data, faultActive)) {
    return;
  }

  drawBatteryCard(index, data, faultActive);
  lastRenderedData[index] = data;
  renderedFaultStates[index] = faultActive;
  hasRenderedData[index] = true;
  lastCardDrawMs[index] = millis();
}

void StationUI::updateButtonState(uint8_t index, bool isOn) {
  if (index >= 2) {
    return;
  }
  buttonStates[index] = isOn;
}

void StationUI::showInputDebug(bool e1, bool e2, bool t1, bool t2) {
  char buf[32];
  snprintf(buf, sizeof(buf), "E1:%d E2:%d T1:%d T2:%d", e1 ? 1 : 0, e2 ? 1 : 0,
           t1 ? 1 : 0, t2 ? 1 : 0);
  tft.setRotation(TFT_ORIENTATION);
  tft.fillRect(4, 26, 150, 10, BG_COLOR);
  tft.setTextSize(1);
  tft.setTextColor(TEXT_MUTED, BG_COLOR);
  tft.setCursor(4, 28);
  tft.print(buf);
}

void StationUI::drawBatteryCard(uint8_t index, const BmsData &data, bool faultActive) {
  const int x = (index == 0) ? BAT1_X : BAT2_X;
  const int y = CARD_Y;
  const bool connected = data.isConnected;
  const bool charging = buttonStates[index] && connected && data.chargeMosfetOn;
  const bool full = connected &&
                    (data.soc >= MAX_STOP_SOC || data.voltage >= MAX_STOP_VOLTAGE);
  const bool fault = connected && buttonStates[index] && faultActive;
  const bool showCharging = charging && !fault;
  const bool isActive = connected;
  const uint16_t borderColor = isActive ? BORDER_ACTIVE : BORDER_LIGHT;
  const uint16_t titleColor = isActive ? BORDER_ACTIVE : TEXT_MUTED;

  tft.fillRect(x, y, CARD_W, CARD_H, BG_COLOR);
  tft.fillRoundRect(x, y, CARD_W, CARD_H, 6, CARD_DIM);
  tft.drawRoundRect(x, y, CARD_W, CARD_H, 6, borderColor);

  tft.setTextSize(1);
  tft.setTextColor(titleColor, CARD_DIM);
  tft.setCursor(x + 35, y + 10);
  tft.print(index == 0 ? "BATTERY 2" : "BATTERY 1");

  if (isActive && connected) {
    tft.fillCircle(x + CARD_W - 15, y + 13, 4, ACCENT_GREEN);
  } else {
    tft.fillCircle(x + CARD_W - 15, y + 13, 4, CARD_DIM);
  }

  drawBatteryGauge(x + 22, y + 30, connected ? data.soc : 0, buttonStates[index],
                   charging, connected);

  tft.drawFastHLine(x + 10, y + 120, CARD_W - 20, borderColor);

  tft.setTextColor(TEXT_MUTED, CARD_DIM);
  tft.setTextSize(1);
  tft.setCursor(x + 15, y + 132);
  tft.print("VOLTAGE");

  tft.setTextColor(TEXT_PRIMARY, CARD_DIM);
  tft.setTextSize(2);
  char voltStr[12];
  if (connected) {
    sprintf(voltStr, "%.1fV", data.voltage);
  } else {
    strcpy(voltStr, "--.-V");
  }
  tft.fillRect(x + 15, y + 145, 58, 16, CARD_DIM);
  tft.setCursor(x + 15, y + 145);
  tft.print(voltStr);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_MUTED, CARD_DIM);
  tft.setCursor(x + 85, y + 132);
  tft.print("CURRENT");

  uint16_t currentColor = TEXT_PRIMARY;
  if (connected) {
    if (charging) {
      currentColor = ACCENT_ORANGE;
    } else if (data.current > 0.5f) {
      currentColor = ACCENT_GREEN;
    }
  }

  tft.setTextColor(currentColor, CARD_DIM);
  tft.setTextSize(2);
  char currentStr[12];
  if (connected) {
    sprintf(currentStr, "%.1fA", data.current);
  } else {
    strcpy(currentStr, "--.-A");
  }
  tft.fillRect(x + 85, y + 145, 52, 16, CARD_DIM);
  tft.setCursor(x + 85, y + 145);
  tft.print(currentStr);

  tft.setTextSize(1);
  tft.fillRect(x + 20, y + 172, 105, 12, CARD_DIM);
  if (!buttonStates[index] || !connected) {
  } else if (full) {
    tft.setTextColor(ACCENT_GREEN, CARD_DIM);
    tft.setCursor(x + 47, y + 175);
    tft.print("FULL");
  } else if (fault) {
    tft.setTextColor(ACCENT_RED, CARD_DIM);
    tft.setCursor(x + 47, y + 175);
    tft.print("FAULT");
    


  } else if (showCharging) {
    tft.setTextColor(ACCENT_GREEN, CARD_DIM);
    tft.setCursor(x + 30, y + 175);
    tft.print("CHARGING");
  }

}

void StationUI::drawBatteryGauge(int x, int y, uint8_t soc, bool buttonOn, bool charging,
                                 bool connected) {
  const int gaugeW = 100;
  const int gaugeH = 80;
  const uint16_t gaugeBorder = connected ? BORDER_LIGHT : TEXT_MUTED;

  tft.drawRoundRect(x, y, gaugeW, gaugeH, 4, gaugeBorder);
  tft.fillRoundRect(x + 35, y - 5, 30, 6, 2, gaugeBorder);
  tft.fillRect(x + 4, y + 4, gaugeW - 8, gaugeH - 8, BG_COLOR);

  if (soc > 100) {
    soc = 100;
  }

  if (connected && soc > 0) {
    uint16_t fillColor = charging ? ACCENT_ORANGE : getSocColor(soc);
    const uint16_t fillH = ((gaugeH - 8) * soc) / 100;
    const uint16_t fillY = y + gaugeH - 4 - fillH;
    tft.fillRoundRect(x + 4, fillY, gaugeW - 8, fillH, 2, fillColor);
  }

  char socStr[8];
  if (!buttonOn) {
    strcpy(socStr, "OFF");
  } else if (!connected) {
    strcpy(socStr, "");
  } else {
    sprintf(socStr, "%d%%", soc);
  }
  tft.setTextColor(TEXT_PRIMARY);
  tft.setTextSize(3);
  const int textWidth = strlen(socStr) * 18;
  int textX = x + (gaugeW - textWidth) / 2;
  if (textX < x + 8) {
    textX = x + 8;
  }
  const int textY = y + 28;
  tft.setCursor(textX, textY);
  tft.print(socStr);
}

void StationUI::showEmergency() {
  tft.setRotation(TFT_ORIENTATION);

  unsigned long now = millis();
  if (needsFullRedraw || now - lastEmergencyBlink >= 500) {
    lastEmergencyBlink = now;
    if (needsFullRedraw) {
      emergencyBlinkState = true;
    } else {
      emergencyBlinkState = !emergencyBlinkState;
    }

    if (emergencyBlinkState) {
      tft.fillScreen(ACCENT_RED);
      tft.setTextColor(TEXT_PRIMARY, ACCENT_RED);
    } else {
      tft.fillScreen(BG_COLOR);
      tft.setTextColor(ACCENT_RED, BG_COLOR);
    }

    uint16_t titleColor = emergencyBlinkState ? TEXT_PRIMARY : ACCENT_RED;
    printCentered("EMERGENCY", 90, titleColor, 3);
    printCentered("STOP", 130, titleColor, 3);
    printCentered("System Disabled", 190, emergencyBlinkState ? TEXT_PRIMARY : TEXT_MUTED, 1);
  }

  needsFullRedraw = true;
}

void StationUI::showMessage(const char *msg) {
  tft.setRotation(TFT_ORIENTATION);
  tft.fillScreen(BG_COLOR);
  printCentered("KRIYA", 80, ACCENT_CYAN, 3);
  printCentered(msg, 140, TEXT_PRIMARY, 2);
  needsFullRedraw = true;
}

void StationUI::resetState() {
  needsFullRedraw = true;
  hasRenderedData[0] = false;
  hasRenderedData[1] = false;
}

void StationUI::requestFullRedraw() {
  needsFullRedraw = true;
  hasRenderedData[0] = false;
  hasRenderedData[1] = false;
}

void StationUI::printCentered(const char *text, int y, uint16_t color,
                              uint8_t textSize) {
  tft.setTextSize(textSize);
  tft.setTextColor(color, BG_COLOR);

  int textWidth = strlen(text) * 6 * textSize;
  int x = (SCREEN_W - textWidth) / 2;
  if (x < 0) {
    x = 0;
  }

  tft.setCursor(x, y);
  tft.print(text);
}

uint16_t StationUI::getSocColor(uint8_t soc) {
  if (soc > 50) {
    return ACCENT_GREEN;
  }
  if (soc > 20) {
    return ACCENT_ORANGE;
  }
  return ACCENT_RED;
}

bool StationUI::shouldRedrawCard(uint8_t index, const BmsData &data, bool faultActive) {
  if (needsFullRedraw || !hasRenderedData[index]) {
    return true;
  }

  const BmsData &last = lastRenderedData[index];
  if (data.isConnected != last.isConnected) {
    return true;
  }
  if (data.soc != last.soc) {
    return true;
  }
  if (fabs(data.voltage - last.voltage) >= 0.3f) {
    return true;
  }
  if (fabs(data.current - last.current) >= 0.4f) {
    return true;
  }
  if (data.chargeMosfetOn != last.chargeMosfetOn ||
      data.dischargeMosfetOn != last.dischargeMosfetOn) {
    return true;
  }
  if (data.protectionStatus != last.protectionStatus) {
    return true;
  }
  if (faultActive != renderedFaultStates[index]) {
    return true;
  }

  return false;
}


