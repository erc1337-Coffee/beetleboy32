#include "media/image.h"

#include <src/webp/decode.h>

// LVGL's bundled lodepng (LV_USE_LODEPNG), compiled as C; its header's C++ API clashes when
// included from C++, so declare the one function used. LVGL modified it: `out` receives an
// lv_draw_buf_t * (RGBA bytes in ->data, rows ->header.stride apart), freed with lv_draw_buf_destroy.
extern "C" unsigned lodepng_decode32(unsigned char **out, unsigned *w, unsigned *h, const unsigned char *in,
                                     size_t insize);

// "BBI2", color format (1 byte), width, height (2 bytes each, little endian), then the pixels.
// Bump the magic when the format changes; old files are refetched.
static constexpr char SERIAL_MAGIC[4] = {'B', 'B', 'I', '2'};
static constexpr size_t SERIAL_HEADER = 9;

// The source pixels [s0, s1) that output pixel i of n covers, along a source of `size` pixels.
static void src_span(uint32_t i, uint32_t n, uint32_t size, uint32_t &s0, uint32_t &s1) {
  s0 = i * size / n;
  s1 = (i + 1) * size / n;
  if (s1 <= s0) s1 = s0 + 1;  // smaller than the output: pixels repeat
}

static uint16_t rgb565(uint32_t r, uint32_t g, uint32_t b) {  // from 8 bits per channel
  return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
}

static void store565(std::vector<uint8_t> &pixels, size_t i, uint16_t c) {  // little endian, as LVGL draws it
  pixels[i * 2] = c & 0xff;
  pixels[i * 2 + 1] = c >> 8;
}

// data and data_size come from pixels_ when asked for, so they always match it: the functions that
// fill pixels_ set only the header.
const lv_image_dsc_t *OwnedImage::dsc() {
  dsc_.data = pixels_.data();
  dsc_.data_size = pixels_.size();
  return &dsc_;
}

void OwnedImage::setHeader(lv_color_format_t cf, uint32_t w, uint32_t h) {
  dsc_ = {};
  dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc_.header.cf = cf;
  dsc_.header.w = w;
  dsc_.header.h = h;
  dsc_.header.stride = w * 2;  // the RGB565 plane's (RGB565A8's alpha plane follows it)
}

bool OwnedImage::fromWebp(const std::string &bytes, int size, OwnedImage &out) {
  int w = 0, h = 0;
  uint8_t *rgba = WebPDecodeRGBA((const uint8_t *)bytes.data(), bytes.size(), &w, &h);  // ~w*h*4 bytes (PSRAM on the board)
  if (!rgba) return false;

  const uint32_t side = w < h ? w : h;
  const uint32_t x0 = (w - side) / 2, y0 = (h - side) / 2;
  out.pixels_.assign((size_t)size * size * 2, 0);
  for (uint32_t y = 0; y < (uint32_t)size; y++) {
    uint32_t sy0, sy1;
    src_span(y, size, side, sy0, sy1);
    for (uint32_t x = 0; x < (uint32_t)size; x++) {
      uint32_t sx0, sx1;
      src_span(x, size, side, sx0, sx1);
      uint32_t r = 0, g = 0, b = 0, n = 0;
      for (uint32_t sy = y0 + sy0; sy < y0 + sy1; sy++) {
        for (uint32_t sx = x0 + sx0; sx < x0 + sx1; sx++) {
          const uint8_t *p = rgba + (sy * w + sx) * 4;
          // Transparent pixels composite onto white, like the website's card.
          r += (p[0] * p[3] + 255 * (255 - p[3])) / 255;
          g += (p[1] * p[3] + 255 * (255 - p[3])) / 255;
          b += (p[2] * p[3] + 255 * (255 - p[3])) / 255;
          n++;
        }
      }
      store565(out.pixels_, (size_t)y * size + x, rgb565(r / n, g / n, b / n));
    }
  }
  WebPFree(rgba);
  out.setHeader(LV_COLOR_FORMAT_RGB565, size, size);
  return true;
}

bool OwnedImage::fromPng(const std::string &bytes, int max_size, OwnedImage &out) {
  unsigned char *out_ptr = nullptr;
  unsigned w = 0, h = 0;
  unsigned error = lodepng_decode32(&out_ptr, &w, &h, (const unsigned char *)bytes.data(), bytes.size());
  auto *decoded = reinterpret_cast<lv_draw_buf_t *>(out_ptr);
  if (error || !decoded || !w || !h) {
    if (decoded) lv_draw_buf_destroy(decoded);
    return false;
  }
  const uint8_t *rgba = decoded->data;
  const uint32_t stride = decoded->header.stride;

  unsigned longest = w > h ? w : h;
  unsigned ow, oh;
  if (longest <= (unsigned)max_size) {
    unsigned k = max_size / longest;
    ow = w * k, oh = h * k;
  } else {
    ow = w * max_size / longest, oh = h * max_size / longest;
  }
  if (!ow) ow = 1;
  if (!oh) oh = 1;

  out.pixels_.assign((size_t)ow * oh * 3, 0);  // RGB565 plane, then the alpha plane
  for (uint32_t y = 0; y < oh; y++) {
    uint32_t sy0, sy1;
    src_span(y, oh, h, sy0, sy1);
    for (uint32_t x = 0; x < ow; x++) {
      uint32_t sx0, sx1;
      src_span(x, ow, w, sx0, sx1);
      uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
      for (uint32_t sy = sy0; sy < sy1; sy++) {
        for (uint32_t sx = sx0; sx < sx1; sx++) {
          const uint8_t *p = rgba + sy * stride + sx * 4;
          r += p[0] * p[3], g += p[1] * p[3], b += p[2] * p[3];  // alpha-weighted: no dark fringes
          a += p[3];
          n++;
        }
      }
      size_t i = (size_t)y * ow + x;
      store565(out.pixels_, i, a ? rgb565(r / a, g / a, b / a) : 0);
      out.pixels_[(size_t)ow * oh * 2 + i] = (uint8_t)(a / n);
    }
  }
  lv_draw_buf_destroy(decoded);
  out.setHeader(LV_COLOR_FORMAT_RGB565A8, ow, oh);
  return true;
}

bool OwnedImage::shrunk(int max_size, OwnedImage &out) const {
  const uint32_t w = dsc_.header.w, h = dsc_.header.h;
  const bool alpha = dsc_.header.cf == LV_COLOR_FORMAT_RGB565A8;
  const uint32_t longest = w > h ? w : h;
  if (pixels_.empty() || longest <= (uint32_t)max_size) return false;
  uint32_t ow = w * max_size / longest, oh = h * max_size / longest;
  if (!ow) ow = 1;
  if (!oh) oh = 1;
  const uint8_t *alphas = pixels_.data() + (size_t)w * h * 2;

  out.pixels_.assign((size_t)ow * oh * (alpha ? 3 : 2), 0);
  for (uint32_t y = 0; y < oh; y++) {
    uint32_t sy0, sy1;
    src_span(y, oh, h, sy0, sy1);
    for (uint32_t x = 0; x < ow; x++) {
      uint32_t sx0, sx1;
      src_span(x, ow, w, sx0, sx1);
      uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
      for (uint32_t sy = sy0; sy < sy1; sy++) {
        for (uint32_t sx = sx0; sx < sx1; sx++) {
          size_t i = (size_t)sy * w + sx;
          uint16_t c = pixels_[i * 2] | pixels_[i * 2 + 1] << 8;
          uint32_t pa = alpha ? alphas[i] : 255;
          r += ((c >> 11) & 0x1f) * pa, g += ((c >> 5) & 0x3f) * pa, b += (c & 0x1f) * pa;  // no dark fringes
          a += pa;
          n++;
        }
      }
      size_t o = (size_t)y * ow + x;
      store565(out.pixels_, o, a ? (uint16_t)((r / a) << 11 | (g / a) << 5 | (b / a)) : 0);
      if (alpha) out.pixels_[(size_t)ow * oh * 2 + o] = (uint8_t)(a / n);
    }
  }
  out.setHeader((lv_color_format_t)dsc_.header.cf, ow, oh);
  return true;
}

std::string OwnedImage::serialize() const {
  std::string out(SERIAL_MAGIC, 4);
  out += (char)dsc_.header.cf;
  for (uint32_t v : {(uint32_t)dsc_.header.w, (uint32_t)dsc_.header.h}) {
    out += (char)(v & 0xff);
    out += (char)(v >> 8);
  }
  out.append((const char *)pixels_.data(), pixels_.size());
  return out;
}

bool OwnedImage::deserialize(const std::string &bytes, OwnedImage &out) {
  if (bytes.size() < SERIAL_HEADER || bytes.compare(0, 4, SERIAL_MAGIC, 4) != 0) return false;
  const uint8_t *b = (const uint8_t *)bytes.data();
  lv_color_format_t cf = (lv_color_format_t)b[4];
  uint32_t w = b[5] | b[6] << 8, h = b[7] | b[8] << 8;
  size_t per_pixel = cf == LV_COLOR_FORMAT_RGB565A8 ? 3 : cf == LV_COLOR_FORMAT_RGB565 ? 2 : 0;
  if (!per_pixel || !w || !h || bytes.size() != SERIAL_HEADER + (size_t)w * h * per_pixel) return false;

  out.pixels_.assign(bytes.begin() + SERIAL_HEADER, bytes.end());
  out.setHeader(cf, w, h);
  return true;
}
