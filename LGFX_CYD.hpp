#pragma once
#include <LovyanGFX.hpp>

/*
  Configuracion para ESP32-2432S028 / ESP32-2432S028R.
  Pantalla + touch ya vienen conectados internamente.
*/

#define CYD_TFT_SCK   14
#define CYD_TFT_MOSI  13
#define CYD_TFT_MISO  12
#define CYD_TFT_CS    15
#define CYD_TFT_DC     2
#define CYD_TFT_BL    21

#define CYD_TP_CLK    25
#define CYD_TP_MOSI   32
#define CYD_TP_MISO   39
#define CYD_TP_CS     33
#define CYD_TP_IRQ    36

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;

#if DISPLAY_CYD_2USB
  lgfx::Panel_ST7789 _panel_instance;
#else
  lgfx::Panel_ILI9341 _panel_instance;
#endif

  lgfx::Light_PWM _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
#if DISPLAY_CYD_2USB
      cfg.freq_write = 80000000;
#else
      cfg.freq_write = 40000000;
#endif
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = CYD_TFT_SCK;
      cfg.pin_mosi = CYD_TFT_MOSI;
      cfg.pin_miso = CYD_TFT_MISO;
      cfg.pin_dc = CYD_TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = CYD_TFT_CS;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
#if DISPLAY_CYD_2USB
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 16;
#else
      cfg.offset_rotation = 2;
      cfg.dummy_read_pixel = 8;
#endif
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = CYD_TFT_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 240;
      cfg.x_max = 3800;
      cfg.y_min = 3700;
      cfg.y_max = 200;
      cfg.pin_int = CYD_TP_IRQ;
      cfg.bus_shared = false;
#if DISPLAY_CYD_2USB
      cfg.offset_rotation = 2;
#else
      cfg.offset_rotation = 0;
#endif
      cfg.spi_host = -1;
      cfg.freq = 1000000;
      cfg.pin_sclk = CYD_TP_CLK;
      cfg.pin_mosi = CYD_TP_MOSI;
      cfg.pin_miso = CYD_TP_MISO;
      cfg.pin_cs = CYD_TP_CS;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
