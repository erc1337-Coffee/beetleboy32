#include "ui/screens/add_wifi_screen.h"

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t ROW_H = 48, ICON = 24, OPEN_W = 50;
static constexpr int32_t NAME_W = UI_VHS_ROW_W - 2 * UI_VHS_ROW_GAP - ICON - OPEN_W;  // the row's rest

void AddWifiScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  list_ = ui_vhs_list(root, UI_VHS_LIST_TOP);
  scan();
  addBackButton(root, BackLook::Text);
}

void AddWifiScreen::scan() {
  app().scanWifi();
  waiting_ = true;
  lv_obj_clean(list_);
  lv_obj_t *row = ui_vhs_list_row(list_, ROW_H);
  ui_vhs_text(row, "SCANNING...", UI_VHS_ROW_W);
}

void AddWifiScreen::update() {
  if (!waiting_ || app().wifiScanning()) return;
  waiting_ = false;
  networks_ = app().wifiScan();
  fill();
}

void AddWifiScreen::fill() {
  lv_obj_clean(list_);
  if (networks_.empty()) ui_vhs_text(ui_vhs_list_row(list_, ROW_H), "NOTHING FOUND", UI_VHS_ROW_W);
  for (size_t i = 0; i < networks_.size(); i++) {
    const WifiStation::ScanResult &n = networks_[i];
    lv_obj_t *row = ui_vhs_list_row(list_, ROW_H, onPick, this);
    lv_obj_set_user_data(row, (void *)(intptr_t)i);
    ui_wifi_bars_set(ui_wifi_bars(row, ICON), ui_wifi_bars_for(n.rssi));
    lv_obj_t *name = ui_vhs_text(row, n.ssid.c_str(), NAME_W);
    ui_vhs_one_line(name);
    if (!n.secured) ui_vhs_text(row, "OPEN", OPEN_W);
  }
  lv_obj_t *again = ui_vhs_list_row(list_, ROW_H, onScan, this);
  lv_obj_set_flex_align(again, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_vhs_text(again, "SCAN AGAIN", UI_VHS_ROW_W);
}

void AddWifiScreen::onPick(lv_event_t *e) {
  auto *self = static_cast<AddWifiScreen *>(lv_event_get_user_data(e));
  size_t i = (size_t)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  if (i < self->networks_.size()) self->app().pickWifi(self->networks_[i]);
}

// A pending rescan must not outlive the screen.
AddWifiScreen::~AddWifiScreen() { lv_async_call_cancel(rescan, this); }

// After the click: scanning empties the list, this row included.
void AddWifiScreen::onScan(lv_event_t *e) { lv_async_call(rescan, lv_event_get_user_data(e)); }

void AddWifiScreen::rescan(void *self) { static_cast<AddWifiScreen *>(self)->scan(); }
