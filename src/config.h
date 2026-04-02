#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===================================
// BMS UART Configuration
// ===================================

// Battery #1 (JBD BMS) - Serial1
#define BAT1_TX_PIN PA9
#define BAT1_RX_PIN PA10
#define BAT1_SERIAL Serial1

// Battery #2 (JBD BMS) - Serial2
#define BAT2_TX_PIN PA2
#define BAT2_RX_PIN PA3
#define BAT2_SERIAL Serial2

#define BMS_BAUD_RATE 9600

// ===================================
// TFT Configuration
// ===================================
// Pins are defined in User_Setup.h for TFT_eSPI
// Orientation: 0=Portrait, 1=Landscape, 2=Portrait Inv, 3=Landscape Inv
#define TFT_ORIENTATION 1

// ============================================================================
// WS2812B LED STRIP
// ============================================================================
#define LED_STRIP_PIN PA1
#define NUM_LEDS 48
#define LED_BRIGHTNESS 255

// ===================================
// Emergency Stop Buttons
// Current hardware with NO feedback and INPUT_PULLUP:
// released = HIGH, pressed = LOW
// ===================================
#define ESTOP1_PIN PB12
#define ESTOP2_PIN PB15
#define ESTOP_ACTIVE_STATE LOW

// ===================================
// Toggle Buttons for Charging ON/OFF
// Current hardware behavior:
// OFF = LOW, ON = HIGH
// ===================================
#define TOGGLE_BTN1_PIN PB14
#define TOGGLE_BTN2_PIN PB13
#define TOGGLE_ACTIVE_STATE HIGH

#define BUTTON_DEBOUNCE_MS 50

// ===================================
// Charging Logic Thresholds
// ===================================
#define MIN_START_SOC 98
#define MAX_STOP_SOC 100
#define MAX_STOP_VOLTAGE 54.6f
#define RECHARGE_THRESHOLD_V 53.0f

// ===================================
// UI Colors
// ===================================
#define C_BG 0x0000
#define C_TEXT 0xFFFF
#define C_ACCENT 0x07FF
#define C_GOOD 0x07E0
#define C_WARN 0xFD20
#define C_ERROR 0xF800

#endif
