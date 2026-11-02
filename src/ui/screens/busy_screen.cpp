#include "ui/screens/busy_screen.h"

#include "ui/theme.h"
#include "ui/widgets.h"

void BusyScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *col = ui_column(root);
  ui_loading(col);
  ui_vhs_text(col, text_.c_str(), UI_COLUMN_VHS_W);
}
