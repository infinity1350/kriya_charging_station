#ifndef USER_SETUP_H
#define USER_SETUP_H

#define STM32
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_RGB

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Pinout adapted from user request
#define TFT_CS PA4
#define TFT_DC PB4
#define TFT_RST PB5

// SPI1 Pins (Standard for STM32F407)
#define TFT_MOSI PA7
#define TFT_SCLK PA5
// #define TFT_MISO PA6 // Not needed for display only

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_GFXFF
#define SMOOTH_FONT

#define TFT_INVERSION_OFF

#define SPI_FREQUENCY 36000000

#endif
