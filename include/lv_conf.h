#ifndef LV_CONF_H
#define LV_CONF_H

// Minimal LVGL 9 config; anything not set here falls back to LVGL's defaults (lv_conf_internal.h).

#define LV_COLOR_DEPTH 16
// Not LVGL's fixed 64KB pool: thread-safe allocators (lodepng decodes on job threads). The board's
// puts everything in PSRAM (platform/esp32/lv_mem_psram.cpp); the sim uses the C library's.
#ifdef ARDUINO
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#else
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#endif
#define LV_DEF_REFR_PERIOD 16
// Remembered between redraws: soft shadows up to this size (shadow width + radius; costs size^2
// bytes) and rounded-corner masks (the tiles, keys and pop-ups use a handful of radii).
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 32
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 16

#define LV_USE_LOG 0
// The FPS / CPU overlay (bottom right), off until Demos > PERF OVERLAY turns it on.
#define LV_USE_SYSMON 1
#define LV_USE_PERF_MONITOR 1

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_QRCODE 1
#define LV_USE_ANIMIMG 1
#define LV_USE_LODEPNG 1  // item icons are PNGs (media/image.cpp)

#ifndef ARDUINO  // the sim
#define LV_USE_SNAPSHOT 1  // BB_SCREENSHOT, see platform/sim/screenshot.cpp
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#endif

#endif  // LV_CONF_H
