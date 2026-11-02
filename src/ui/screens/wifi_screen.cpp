#include "ui/screens/wifi_screen.h"

#include "app/app.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr uint32_t REFRESH_MS = 1000;
static constexpr int32_t TOP = BELOW_BACK;
static constexpr int32_t ROW_GAP = 8, BUTTONS_TOP = 6;  // BUTTONS_TOP: extra space above the buttons
static constexpr int32_t BARS_SIZE = 56;
static constexpr int32_t BUTTON_PAD = 14;  // above and below the labels: finger-sized

void WifiScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *col = ui_column(root);
  lv_obj_set_style_pad_top(col, TOP, 0);
  lv_obj_set_style_pad_row(col, ROW_GAP, 0);
  bars_ = ui_wifi_bars(col, BARS_SIZE);
  network_ = ui_vhs_text(col, "", UI_COLUMN_VHS_W);
  status_ = ui_vhs_text(col, "", UI_COLUMN_VHS_W);  // only when not connected

  lv_obj_t *row = ui_vhs_button_row(col);
  lv_obj_set_style_pad_top(row, BUTTONS_TOP, 0);
  for (lv_obj_t *btn : {ui_vhs_button(row, "Known", onKnown, this), ui_vhs_button(row, "Add", onAdd, this)})
    lv_obj_set_style_pad_ver(btn, BUTTON_PAD, 0);

  addBackButton(root, BackLook::Text);
  refresh();
}

void WifiScreen::update() {
  if (hal_millis() - shownAt_ >= REFRESH_MS) refresh();
}

void WifiScreen::refresh() {
  shownAt_ = hal_millis();
  App::WifiInfo info = app().wifiInfo();
  ui_wifi_bars_set(bars_, ui_wifi_bars_for(info.rssi));
  ui_vhs_set_text(network_, info.ssid.c_str());
  bool connected = !info.ip.empty();
  ui_vhs_set_text(status_, connected ? "" : info.status);
  lv_obj_update_flag(status_, LV_OBJ_FLAG_HIDDEN, connected);
}

void WifiScreen::onKnown(lv_event_t *e) {
  static_cast<WifiScreen *>(lv_event_get_user_data(e))->app().openKnownWifi();
}

void WifiScreen::onAdd(lv_event_t *e) {
  static_cast<WifiScreen *>(lv_event_get_user_data(e))->app().openAddWifi();
}
