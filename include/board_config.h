#pragma once

// Pin map for the Waveshare ESP32-S3-Touch-LCD-1.69.
// Source: https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69

// LCD (SPI): ST7789V2, 240x280.
#define PIN_LCD_DC   4
#define PIN_LCD_CS   5
#define PIN_LCD_SCK  6
#define PIN_LCD_MOSI 7
#define PIN_LCD_RST  8
#define PIN_LCD_BL   15

// Shared I2C bus: touch (CST816T @0x15), IMU (QMI8658 @0x6B), RTC (PCF85063 @0x51)
#define PIN_I2C_SDA  11
#define PIN_I2C_SCL  10
#define PIN_TP_RST   13
#define PIN_TP_INT   14

#define PIN_BUZZER   42  // GPIO33 on the older board revision
#define PIN_IMU_INT  38  // the QMI8658's interrupt line
#define PIN_BAT_ADC  1   // VBAT / 3
// Power key circuit: on battery, the key powers the board up, and SYS_EN must then be held high or
// the power drops when the key is let go. SYS_OUT reads the key (low while pressed).
#define PIN_SYS_EN   41
#define PIN_SYS_OUT  40

// External buttons: wire each one between the GPIO and GND (internal pull-up, active low).
// Free header pins are 2, 3, 17, 18 (+43/44 UART). GPIO3 is a strapping pin, so it's left spare.
#define PIN_BTN_CANCEL 2
#define PIN_BTN_OK     17
// No Next button: GPIO18 drives the LEDs. Define PIN_BTN_NEXT on a free pin to add one.

// RGB LEDs (WS2812B), powered from 3V3: data on this pin, chained.
#define PIN_LEDS  18
#define LED_COUNT 4

// The panel is natively portrait (240x280); the app runs it landscape.
#define PANEL_W 240
#define PANEL_H 280
#define DISPLAY_ROTATION 3  // LovyanGFX rotation: 1 or 3 (flip if the image is upside down)
#define SCREEN_W PANEL_H    // logical size the UI is laid out for
#define SCREEN_H PANEL_W
