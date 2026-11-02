#include "ui/screens/setup_choice_screen.h"

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t BUTTON_PAD = 14;  // above and below the labels: finger-sized

void SetupChoiceScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *col = ui_column(root);
  const char *title = mode_ == SetupMode::WifiOnly      ? "NEW WI-FI"
                      : mode_ == SetupMode::AccountOnly ? "SIGN IN AGAIN"
                                                        : "SET UP";
  ui_vhs_text(col, title, UI_COLUMN_VHS_W);
  ui_vhs_text(col, "HOW?", UI_COLUMN_VHS_W);
  for (auto [label, cb] :
       {std::pair<const char *, lv_event_cb_t>{"WITH MY PHONE", onPhone}, {"ON THIS DEVICE", onDevice}}) {
    // A row each, wide enough for one line (not the two-to-a-row width).
    lv_obj_t *btn = ui_vhs_button(ui_vhs_button_row(col), label, cb, this, UI_COLUMN_VHS_W - 20);
    lv_obj_set_style_pad_ver(btn, BUTTON_PAD, 0);
  }
}

void SetupChoiceScreen::onPhone(lv_event_t *e) {
  static_cast<SetupChoiceScreen *>(lv_event_get_user_data(e))->app().setupByPhone();
}

void SetupChoiceScreen::onDevice(lv_event_t *e) {
  static_cast<SetupChoiceScreen *>(lv_event_get_user_data(e))->app().setupOnDevice();
}
