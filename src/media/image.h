#pragma once

#include <lvgl.h>
#include <stdint.h>

#include <string>
#include <vector>

// An LVGL image whose pixels are owned at runtime (e.g. a downloaded avatar), unlike assets/.
// Movable, not copyable; dsc() stays valid as long as the image does.
class OwnedImage {
 public:
  OwnedImage() = default;
  OwnedImage(OwnedImage &&) = default;
  OwnedImage &operator=(OwnedImage &&) = default;
  OwnedImage(const OwnedImage &) = delete;
  OwnedImage &operator=(const OwnedImage &) = delete;

  bool empty() const { return pixels_.empty(); }
  const lv_image_dsc_t *dsc();

  // Decodes a WebP, center-crops it to a square and box-downscales it to size x size (RGB565).
  static bool fromWebp(const std::string &bytes, int size, OwnedImage &out);

  // A box-downscaled copy that fits in max_size x max_size (RGB565A8 stays RGB565A8): drawn 1:1,
  // it costs far less than LVGL scaling the original on every redraw. False if it's not smaller.
  bool shrunk(int max_size, OwnedImage &out) const;

  // Raw form for AssetCache: the pixels as drawn, no decoding needed to load them back.
  std::string serialize() const;
  static bool deserialize(const std::string &bytes, OwnedImage &out);

  // Decodes a PNG and fits it in max_size x max_size, keeping its shape and transparency (RGB565A8).
  // Small images grow by a whole factor (pixel art stays sharp); big ones are box-downscaled.
  // Safe on any thread.
  static bool fromPng(const std::string &bytes, int max_size, OwnedImage &out);

 private:
  void setHeader(lv_color_format_t cf, uint32_t w, uint32_t h);  // a fresh dsc_ for pixels_ of that shape

  std::vector<uint8_t> pixels_;
  lv_image_dsc_t dsc_ = {};
};
