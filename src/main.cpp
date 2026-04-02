#include "JbdBms.h"
#include "LedStrip.h"
#include "StationUI.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>

// Temporary mode:
// keep software emergency detection enabled for UI/feedback.
static const bool DISABLE_SOFTWARE_EMERGENCY_LOGIC = false;

// Hardware emergency mode:
// software reads emergency only for feedback/UI and does not control cutoff.
static const bool USE_EMERGENCY_FEEDBACK_ONLY = true;

// ---------------- PIN DEFINITIONS ----------------
// (Using pins from config.h — removed duplicate #defines to avoid conflict)

// ---------------- GLOBAL STATES ----------------
bool chargerEnabled[2] = {false, false};
bool emergencyStop = false;
bool lastEmergencyStop = false;
bool recoveryNeeded[2] = {false, false};
unsigned long currentZeroSinceMs[2] = {0, 0};
bool currentWasZero[2] = {false, false};

// ---------------- OBJECTS ----------------
JbdBms bms1(BAT1_SERIAL);
JbdBms bms2(BAT2_SERIAL);
StationUI ui;
LedStrip leds;

// ---------------- TIMING ----------------
unsigned long lastBmsUpdate = 0;
unsigned long lastLedUpdate = 0;
unsigned long lastUiUpdate = 0;

const unsigned long BMS_UPDATE_INTERVAL = 500;
const unsigned long LED_UPDATE_INTERVAL = 50;
const unsigned long UI_UPDATE_INTERVAL = 400;
const unsigned long EMERGENCY_DEBOUNCE_MS = 80;

bool emergencyRawLast = false;
bool emergencyStable = false;
unsigned long emergencyLastChangeMs = 0;

// ---------------- DATA CACHE ----------------
BmsData cachedData[2] = {};

static const uint8_t LEFT_BATTERY_INDEX = 1;
static const uint8_t RIGHT_BATTERY_INDEX = 0;

bool isFaultActive(uint8_t index) {
  if (index >= 2) return false;

  const BmsData &data = cachedData[index];

  // No fault if charger is off or battery not connected — reset timer
  if (!chargerEnabled[index] || !data.isConnected) {
    currentZeroSinceMs[index] = 0;
    currentWasZero[index] = false;
    return false;
  }

  // No fault if battery is full — reset timer
  const bool full = (data.soc >= MAX_STOP_SOC || data.voltage >= MAX_STOP_VOLTAGE);
  if (full) {
    currentZeroSinceMs[index] = 0;
    currentWasZero[index] = false;
    return false;
  }

  // Immediate fault for BMS protection flags (over-voltage, over-current, etc.)
  if (data.protectionStatus > 0) {
    return true;
  }

  // Current is flowing — reset the zero-current timer, no fault
  if (data.current > 0.0f) {
    currentZeroSinceMs[index] = 0;
    currentWasZero[index] = false;
    return false;
  }

  // Current is 0: start the timer on the first zero reading
  if (!currentWasZero[index]) {
    currentWasZero[index] = true;
    currentZeroSinceMs[index] = millis();
  }

  // Only report fault after current has been 0 for 15 seconds continuously
  return (millis() - currentZeroSinceMs[index]) >= 15000UL;
}

// ======================================================
// READ FUNCTIONS
// ======================================================
bool readCharger1() { return digitalRead(TOGGLE_BTN1_PIN) == TOGGLE_ACTIVE_STATE; }
bool readCharger2() { return digitalRead(TOGGLE_BTN2_PIN) == TOGGLE_ACTIVE_STATE; }

// FIX: Use ESTOP_ACTIVE_STATE from config.h for consistent detection
bool isEmergencyActive() {
  if (DISABLE_SOFTWARE_EMERGENCY_LOGIC) {
    return false;
  }
  bool e1Pressed = (digitalRead(ESTOP1_PIN) == ESTOP_ACTIVE_STATE);
  bool e2Pressed = (digitalRead(ESTOP2_PIN) == ESTOP_ACTIVE_STATE);
  return e1Pressed || e2Pressed;
}

// ======================================================
// FUNCTION PROTOTYPES
// ======================================================
void handleBattery(JbdBms &bms, uint8_t index);
void handleToggleButtons();

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Charging Station Starting");

  pinMode(TOGGLE_BTN1_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_BTN2_PIN, INPUT_PULLUP);
  // FIX: Use ESTOP pin names from config.h
  pinMode(ESTOP1_PIN, INPUT_PULLUP);
  pinMode(ESTOP2_PIN, INPUT_PULLUP);

  BAT1_SERIAL.begin(BMS_BAUD_RATE);
  BAT2_SERIAL.begin(BMS_BAUD_RATE);

  leds.begin();

  ui.begin();
  ui.showMessage("INITIALIZING...");
  delay(600);
  ui.begin();
  ui.requestFullRedraw();
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  // ---- Emergency debounce ----
  bool emergencyRawNow = isEmergencyActive();
  if (emergencyRawNow != emergencyRawLast) {
    emergencyRawLast = emergencyRawNow;
    emergencyLastChangeMs = millis();
  }
  if ((millis() - emergencyLastChangeMs) >= EMERGENCY_DEBOUNCE_MS) {
    emergencyStable = emergencyRawLast;
  }
  const bool anyChargerEnabled = chargerEnabled[0] || chargerEnabled[1];
  emergencyStop = emergencyStable && anyChargerEnabled;

  // ---- Debug print ----
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 500) {
    lastDebug = millis();
    Serial.printf("E1=%d E2=%d emergency=%d | C1=%d[%s] C2=%d[%s]\n",
                  digitalRead(ESTOP1_PIN), digitalRead(ESTOP2_PIN),
                  emergencyStop,
                  digitalRead(TOGGLE_BTN1_PIN), chargerEnabled[0] ? "ON" : "OFF",
                  digitalRead(TOGGLE_BTN2_PIN), chargerEnabled[1] ? "ON" : "OFF");
  }

  // ---- Handle toggle buttons ----
  // In hardware-emergency mode, toggles must still be readable even when
  // emergency feedback is active.
  handleToggleButtons();

  // ---- Detect emergency release: set recovery flags ----
  // FIX: This was correct in the original, but we moved it BEFORE BMS update
  // so recoveryNeeded is set before handleBattery() runs this cycle.
  if (lastEmergencyStop && !emergencyStop) {
    recoveryNeeded[0] = chargerEnabled[0];
    recoveryNeeded[1] = chargerEnabled[1];
    Serial.printf("Emergency released: recovery B1=%d B2=%d\n",
                  recoveryNeeded[0], recoveryNeeded[1]);
  }

  // ---- BMS Update ----
  if (millis() - lastBmsUpdate > BMS_UPDATE_INTERVAL) {
    lastBmsUpdate = millis();
    // Physical slot wiring is opposite the UART numbering:
    // right slot (Battery 1) is read on bms2, left slot (Battery 2) is read on bms1.
    handleBattery(bms2, 0);
    handleBattery(bms1, 1);
  }

  // ---- LED Update ----
  if (millis() - lastLedUpdate > LED_UPDATE_INTERVAL) {
    lastLedUpdate = millis();
    if (emergencyStop) {
      leds.showEmergency();
    } else {
      // Physical layout:
      // left side = Battery 2, right side = Battery 1
      leds.update(0, cachedData[RIGHT_BATTERY_INDEX], chargerEnabled[RIGHT_BATTERY_INDEX],
                  isFaultActive(RIGHT_BATTERY_INDEX));
      leds.update(1, cachedData[LEFT_BATTERY_INDEX], chargerEnabled[LEFT_BATTERY_INDEX],
                  isFaultActive(LEFT_BATTERY_INDEX));
    }
    leds.show();
  }

  // ---- UI Update ----
  if (emergencyStop) {
    ui.showEmergency();
  } else {
    if (lastEmergencyStop) {
      ui.resetState();
      ui.requestFullRedraw();
      ui.updateBattery(0, cachedData[LEFT_BATTERY_INDEX], isFaultActive(LEFT_BATTERY_INDEX));
      ui.updateBattery(1, cachedData[RIGHT_BATTERY_INDEX], isFaultActive(RIGHT_BATTERY_INDEX));
    }

    if (millis() - lastUiUpdate > UI_UPDATE_INTERVAL) {
      lastUiUpdate = millis();
      ui.updateBattery(0, cachedData[LEFT_BATTERY_INDEX], isFaultActive(LEFT_BATTERY_INDEX));
      ui.updateBattery(1, cachedData[RIGHT_BATTERY_INDEX], isFaultActive(RIGHT_BATTERY_INDEX));
    }
  }
  lastEmergencyStop = emergencyStop;
}

// ======================================================
// TOGGLE HANDLER
// ======================================================
void handleToggleButtons() {
  static bool charger1RawLast = false;
  static bool charger2RawLast = false;
  static bool charger1Stable = false;
  static bool charger2Stable = false;
  static unsigned long charger1LastChangeMs = 0;
  static unsigned long charger2LastChangeMs = 0;

  bool charger1Raw = readCharger1();
  bool charger2Raw = readCharger2();

  if (charger1Raw != charger1RawLast) {
    charger1RawLast = charger1Raw;
    charger1LastChangeMs = millis();
  }
  if (charger2Raw != charger2RawLast) {
    charger2RawLast = charger2Raw;
    charger2LastChangeMs = millis();
  }

  if ((millis() - charger1LastChangeMs) >= BUTTON_DEBOUNCE_MS) {
    charger1Stable = charger1RawLast;
  }
  if ((millis() - charger2LastChangeMs) >= BUTTON_DEBOUNCE_MS) {
    charger2Stable = charger2RawLast;
  }

  bool buttonStateChanged = false;

  if (chargerEnabled[0] != charger1Stable) {
    chargerEnabled[0] = charger1Stable;
    buttonStateChanged = true;
    Serial.printf("Charger 1 %s\n", chargerEnabled[0] ? "ENABLED" : "DISABLED");
    if (!chargerEnabled[0]) {
      recoveryNeeded[0] = false;
    }
  }

  if (chargerEnabled[1] != charger2Stable) {
    chargerEnabled[1] = charger2Stable;
    buttonStateChanged = true;
    Serial.printf("Charger 2 %s\n", chargerEnabled[1] ? "ENABLED" : "DISABLED");
    if (!chargerEnabled[1]) {
      recoveryNeeded[1] = false;
    }
  }

  // Card index 0 is the left card and index 1 is the right card.
  // Physical layout:
  // left side = Battery 2 / Toggle 2, right side = Battery 1 / Toggle 1
  ui.updateButtonState(0, chargerEnabled[LEFT_BATTERY_INDEX]);
  ui.updateButtonState(1, chargerEnabled[RIGHT_BATTERY_INDEX]);

  if (buttonStateChanged) {
    ui.requestFullRedraw();
    ui.updateBattery(0, cachedData[LEFT_BATTERY_INDEX], isFaultActive(LEFT_BATTERY_INDEX));
    ui.updateBattery(1, cachedData[RIGHT_BATTERY_INDEX], isFaultActive(RIGHT_BATTERY_INDEX));
  }
}

// ======================================================
// BATTERY HANDLER
// ======================================================
void handleBattery(JbdBms &bms, uint8_t index) {
  bool success = bms.update();
  const BmsData &data = bms.getData();

  cachedData[index] = data;

  if (!success || !data.isConnected) {
    Serial.printf("Bat %d: Comm Error\n", index + 1);
    return;
  }

  // FIX: Emergency stop — disable charging but DO NOT clear recoveryNeeded here.
  // recoveryNeeded is set when emergency is RELEASED (handled in loop()),
  // so clearing it here would prevent recovery from working.
  if (emergencyStop) {
    if (USE_EMERGENCY_FEEDBACK_ONLY) {
      // In hardware-emergency mode, software should not block per-battery
      // toggle control. Hardware handles the actual cutoff.
    } else if (data.chargeMosfetOn) {
      bms.disableCharging();
      Serial.printf("Bat %d: EMERGENCY STOP — charging cut\n", index + 1);
      return;
    }
  }

  if (!chargerEnabled[index]) {
    recoveryNeeded[index] = false;
    if (data.chargeMosfetOn) {
      bms.disableCharging();
      Serial.printf("Bat %d: Disabled by switch\n", index + 1);
    }
    return;
  }

  if (data.soc >= MAX_STOP_SOC || data.voltage >= MAX_STOP_VOLTAGE) {
    recoveryNeeded[index] = false;
    if (data.chargeMosfetOn) {
      bms.disableCharging();
      Serial.printf("Bat %d: Full\n", index + 1);
    }
    return;
  }

  if (recoveryNeeded[index]) {
    if (!data.chargeMosfetOn) {
      bms.enableCharging();
      Serial.printf("Bat %d: Recovery charging command\n", index + 1);
    } else {
      recoveryNeeded[index] = false;
      Serial.printf("Bat %d: Recovery complete\n", index + 1);
    }
    return;
  }

  if (!data.chargeMosfetOn) {
    bms.enableCharging();
    Serial.printf("Bat %d: Charging\n", index + 1);
  }
}
