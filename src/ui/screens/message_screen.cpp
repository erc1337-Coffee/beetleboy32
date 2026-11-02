#include "ui/screens/message_screen.h"

#include "ui/theme.h"
#include "ui/widgets.h"

void MessageScreen::build(lv_obj_t *root) {
  lv_obj_t *col = ui_column(root);
  if (look_ == Look::Blue) {
    theme::style_blue_screen(root);
    ui_vhs_text(col, title_.c_str(), UI_COLUMN_VHS_W);
    if (!body_.empty()) ui_vhs_text(col, body_.c_str(), UI_COLUMN_VHS_W);
    if (actions_.empty()) return;
    lv_obj_t *row = ui_vhs_button_row(col);
    lv_obj_set_style_pad_top(row, 8, 0);
    for (auto &action : actions_) ui_vhs_button(row, action.label.c_str(), onClick, &action);
    return;
  }
  ui_title(col, title_.c_str());
  if (!body_.empty()) ui_text(col, body_.c_str());
  if (actions_.empty()) return;
  lv_obj_t *row = ui_row(col);
  lv_obj_set_style_pad_top(row, 8, 0);
  for (auto &action : actions_) ui_button(row, action.label.c_str(), onClick, &action);
}

void MessageScreen::onClick(lv_event_t *e) {
  auto *action = static_cast<Action *>(lv_event_get_user_data(e));
  if (action->run) action->run();
}

bool MessageScreen::onInput(const InputEvent &event) {
  if (event.action != InputAction::Back || !onBack_) return false;
  onBack_();
  return true;
}
