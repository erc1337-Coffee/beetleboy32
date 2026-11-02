#pragma once

#include <functional>
#include <string>

#include "ui/screen.h"

// Text entry sized for a fingertip on the small screen, like an old phone's keypad: six big keys,
// each holding up to five characters. Tapping a key types its first one; tapping it again quickly
// cycles to the next (shown highlighted until it's settled: a pause or another key). A row below
// switches layers (abc, ABC, 123, #+=), types a space, deletes, and finishes with OK, which hands the
// text to `done`.
class KeyboardScreen : public Screen {
 public:
  using Done = std::function<void(const std::string &text)>;
  // masked: the text shows as *, but for the character still cycling.
  KeyboardScreen(App &app, std::string prompt, Done done, bool masked = false)
      : Screen(app), prompt_(std::move(prompt)), done_(std::move(done)), masked_(masked) {}
  ~KeyboardScreen() override;
  const char *name() const override { return "keyboard"; }
  void update() override;  // settles the cycling character after a pause

 protected:
  void build(lv_obj_t *root) override;

 private:
  static constexpr int CELLS = 6;
  void showKeys();   // the cells and the layer key, for layer_
  // The text's end, with the cycling character highlighted (or else a cursor); the prompt while empty.
  void showField();
  void settle();     // the cycling character stays as it is
  static void onCell(lv_event_t *e);
  static void onLayer(lv_event_t *e);
  static void onSpace(lv_event_t *e);
  static void onDelete(lv_event_t *e);
  static void onOk(lv_event_t *e);

  std::string prompt_, text_;
  Done done_;
  bool masked_;
  lv_obj_t *cells_[CELLS] = {}, *cellLabels_[CELLS] = {};
  lv_obj_t *layerLabel_ = nullptr, *field_ = nullptr;
  int layer_ = 0;
  int cycling_ = -1;  // the key whose character is still cycling (the text's last one), -1 for none
  int cycleIndex_ = 0;
  uint32_t tappedAt_ = 0;
};
