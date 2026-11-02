#include "screenshot.h"

#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>

static bool write_bmp(const char *path, const lv_draw_buf_t *buf) {
  const int w = buf->header.w, h = buf->header.h;
  const int row_size = (w * 3 + 3) & ~3;  // BMP rows are 4-byte aligned
  const uint32_t data_size = row_size * h, file_size = 54 + data_size;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int off, uint32_t v) {
    for (int i = 0; i < 4; i++) header[off + i] = (v >> (8 * i)) & 0xff;
  };
  put32(2, file_size);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  header[26] = 1;   // planes
  header[28] = 24;  // bits per pixel
  put32(34, data_size);

  FILE *f = fopen(path, "wb");
  if (!f) return false;
  fwrite(header, 1, sizeof(header), f);
  static const uint8_t pad[3] = {};
  for (int y = h - 1; y >= 0; y--) {  // bottom-up; LVGL RGB888 is already BGR like BMP
    fwrite(buf->data + y * buf->header.stride, 1, w * 3, f);
    fwrite(pad, 1, row_size - w * 3, f);
  }
  return fclose(f) == 0;
}

static void take(lv_timer_t *) {
  const char *path = getenv("BB_SCREENSHOT");
  // Own buffer: a full-screen snapshot doesn't fit the LVGL heap, which is sized like the board's.
  lv_obj_t *screen = lv_screen_active();
  const uint32_t w = lv_obj_get_width(screen), h = lv_obj_get_height(screen);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB888);
  void *data = malloc(stride * h + LV_DRAW_BUF_ALIGN);
  lv_draw_buf_t buf;
  bool ok = lv_draw_buf_init(&buf, w, h, LV_COLOR_FORMAT_RGB888, stride,
                             lv_draw_buf_align(data, LV_COLOR_FORMAT_RGB888), stride * h) == LV_RESULT_OK &&
            lv_snapshot_take_to_draw_buf(screen, LV_COLOR_FORMAT_RGB888, &buf) == LV_RESULT_OK &&
            write_bmp(path, &buf);
  free(data);
  printf("[sim] screenshot %s: %s\n", ok ? "saved" : "FAILED", path);
  exit(ok ? 0 : 1);
}

void screenshot_init() {
  if (!getenv("BB_SCREENSHOT")) return;
  const char *ms = getenv("BB_SCREENSHOT_MS");
  lv_timer_create(take, ms ? (uint32_t)atoi(ms) : 1500, nullptr);
}
