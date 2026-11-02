// LVGL's memory (lv_conf.h: LV_STDLIB_CUSTOM on the board), in PSRAM: the ~300 KB of internal RAM
// is needed by Wi-Fi, TLS and thread stacks, and a screen with a hundred tiles plus a download could
// run it dry. Internal RAM only if PSRAM is full. Thread-safe (icons are decoded into LVGL buffers
// on job threads).
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <stdlib.h>

static constexpr uint32_t PSRAM = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

extern "C" {
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}

void *lv_malloc_core(size_t size) {
  void *p = heap_caps_malloc(size, PSRAM);
  return p ? p : malloc(size);
}

void *lv_realloc_core(void *p, size_t new_size) {
  void *q = heap_caps_realloc(p, new_size, PSRAM);
  return q ? q : realloc(p, new_size);
}

void lv_free_core(void *p) { heap_caps_free(p); }
void lv_mem_monitor_core(lv_mem_monitor_t *) {}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
}
