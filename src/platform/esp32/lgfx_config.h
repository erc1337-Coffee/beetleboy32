#pragma once

// LovyanGFX device for the display + touch.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board_config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Touch_CST816S _touch;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.pin_sclk = PIN_LCD_SCK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_LCD_DC;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      // Native (portrait) geometry; rotation is applied at runtime via setRotation().
      // The controller has 240x320 of RAM; the panel shows a 240x280 window of it, 20 rows down.
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = PIN_LCD_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = PANEL_W;
      cfg.panel_height = PANEL_H;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 20;
      cfg.readable = false;
      cfg.invert = true;  // IPS panel
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    {
      auto cfg = _touch.config();
      // Raw touch coordinates, native orientation; LovyanGFX rotates them with the display.
      cfg.x_min = 0;
      cfg.x_max = PANEL_W - 1;
      cfg.y_min = 0;
      cfg.y_max = PANEL_H - 1;
      cfg.i2c_addr = 0x15;
      cfg.pin_int = PIN_TP_INT;
      cfg.pin_rst = PIN_TP_RST;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 0;
      cfg.pin_sda = PIN_I2C_SDA;
      cfg.pin_scl = PIN_I2C_SCL;
      cfg.freq = 400000;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
