#pragma once

#include "ui/screen.h"
#include "ui/widgets.h"

// The Cheeseman's shop: claim the daily cheese, or see when it's back. The table shows a cheese
// while it's claimable. Tapping him squishes him; every third squish claims the junk faucet if ready.
// A cheese claim rains cheese; junk plays the loot animation (ui/loot_animation.h).
class CheeseScreen : public Screen {
 public:
  explicit CheeseScreen(App &app) : Screen(app) {}
  const char *name() const override { return "cheese"; }
  void update() override;  // countdown, claim progress, poster swap

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onClaim(lv_event_t *e);
  static void onSquish(lv_event_t *e);
  void showResult();

  UiIconTile claim_{};
  lv_obj_t *cooldown_ = nullptr;  // "Ready in" over the countdown, shown instead of the claim button
  lv_obj_t *countdown_ = nullptr;
  lv_obj_t *cheeseman_ = nullptr;
  int squishes_ = 0;
  uint32_t resultSeq_ = 0;  // last result shown
  uint32_t animatedUntil_ = 0;  // a claim's animation plays: the cooldown shows after it
  int ready_ = -1;  // poster shown: 1 claimable, 0 empty, -1 none yet
};
