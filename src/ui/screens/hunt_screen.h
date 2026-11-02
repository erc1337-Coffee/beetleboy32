#pragma once

#include "ui/screen.h"

// The forest, with two keys at the bottom:
//  - the beetle key: green "CLAIM BEETLE" when the trap is ready, else yellow "HUNT BEETLE" with a
//    dot per hunt left, else grey with the countdown. Each press sends one request.
//  - the Cheeseman key opens his view.
// What each press wins plays as a loot animation over the forest (ui/loot_animation.h).
class HuntScreen : public Screen {
 public:
  explicit HuntScreen(App &app) : Screen(app) {}
  const char *name() const override { return "hunt"; }
  void update() override;  // key look, countdown, request in flight

 protected:
  void build(lv_obj_t *root) override;

 private:
  enum class Look { None, Claim, Hunt, Waiting };
  Look currentLook();
  static void onBeetle(lv_event_t *e);
  static void onCheeseman(lv_event_t *e);
  void showResult();

  lv_obj_t *beetle_ = nullptr;
  lv_obj_t *top_ = nullptr;   // small line above: "Ready in"
  lv_obj_t *main_ = nullptr;  // CLAIM BEETLE / HUNT BEETLE / countdown
  lv_obj_t *dots_ = nullptr;  // hunts left, out of BeetleState::MAX_HUNTS
  Look look_ = Look::None;
  uint32_t resultSeq_ = 0;  // last result shown
};
