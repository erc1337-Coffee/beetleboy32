#include "ui/screens/known_wifi_screen.h"

#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/widgets.h"

static constexpr int32_t ROW_H = 48, ICON = 24, DELETE_W = 48;
static constexpr int32_t NAME_W = UI_VHS_ROW_W - 2 * UI_VHS_ROW_GAP - ICON - DELETE_W;  // the row's rest
static constexpr int32_t DELETE_INSET = 4;  // above and below the delete button, in its row

void KnownWifiScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  list_ = ui_vhs_list(root, UI_VHS_LIST_TOP);
  fill();
  addBackButton(root, BackLook::Text);
}

void KnownWifiScreen::fill() {
  lv_obj_clean(list_);
  const auto &networks = app().knownWifi();
  shown_ = networks.size();
  App::WifiInfo info = app().wifiInfo();
  for (const WifiCredentials &n : networks) {
    lv_obj_t *row = ui_vhs_list_row(list_, ROW_H);
    lv_obj_t *bars = ui_wifi_bars(row, ICON);  // seen only for the one in use: names line up either way
    bool in_use = n.ssid == info.ssid && !info.ip.empty();
    ui_wifi_bars_set(bars, ui_wifi_bars_for(info.rssi));
    lv_obj_set_style_opa(bars, in_use ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_t *name = ui_vhs_text(row, n.ssid.c_str(), NAME_W);
    ui_vhs_one_line(name);
    if (networks.size() <= 1) continue;  // the only one can't go

    lv_obj_t *del = lv_button_create(row);
    lv_obj_remove_style_all(del);
    lv_obj_set_size(del, DELETE_W, ROW_H - 2 * DELETE_INSET);
    lv_obj_set_style_radius(del, 6, 0);
    lv_obj_set_style_bg_color(del, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(del, LV_OPA_20, LV_STATE_PRESSED);
    ui_squish(del);
    lv_obj_t *icon = ui_label(del, theme::font_title(), lv_color_white(), LV_SYMBOL_TRASH);
    lv_obj_set_style_text_opa(icon, LV_OPA_90, 0);
    lv_obj_center(icon);
    lv_obj_add_event_cb(del, onDelete, LV_EVENT_CLICKED, this);
  }
}

void KnownWifiScreen::update() {
  if (app().knownWifi().size() != shown_) fill();
}

void KnownWifiScreen::onDelete(lv_event_t *e) {
  auto *self = static_cast<KnownWifiScreen *>(lv_event_get_user_data(e));
  lv_obj_t *row = lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  std::string ssid = self->app().knownWifi()[lv_obj_get_index(row)].ssid;
  App *app = &self->app();  // not the screen: Back may close it while the question is up
  show_toast(LV_SYMBOL_TRASH, ("Forget " + ssid + "?").c_str(), 0,
             {{"Cancel", nullptr}, {"Forget", [app, ssid] { app->forgetWifi(ssid); }}});
}
