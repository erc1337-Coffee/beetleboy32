#include "ui/screens/setup_join_screen.h"

#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t TOP = BELOW_BACK;
static constexpr int32_t QR_SIZE = 136;
// Tighter than ui_column's defaults so the QR code can be as large as possible.
static constexpr int32_t BOTTOM = 8, ROW_GAP = 6, DETAILS_GAP = 2;

void SetupJoinScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  // Standard Wi-Fi QR payload, understood by iOS and Android cameras.
  // SSID and password are generated from [A-Za-z0-9-], so no escaping is needed.
  std::string payload = "WIFI:T:WPA;S:" + network_.ssid + ";P:" + network_.password + ";;";

  lv_obj_t *col = ui_column(root);
  lv_obj_set_style_pad_ver(col, BOTTOM, 0);
  lv_obj_set_style_pad_top(col, TOP, 0);
  lv_obj_set_style_pad_row(col, ROW_GAP, 0);

  ui_qr(col, payload.c_str(), QR_SIZE);

  lv_obj_t *details = lv_obj_create(col);
  lv_obj_remove_style_all(details);
  lv_obj_set_size(details, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(details, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(details, DETAILS_GAP, 0);
  ui_vhs_text(details, ("WI-FI " + network_.ssid).c_str(), UI_COLUMN_VHS_W);
  ui_vhs_text(details, ("PASS  " + network_.password).c_str(), UI_COLUMN_VHS_W);
  addBackButton(root, BackLook::Text);  // back to the choice: setup on the device instead
}
