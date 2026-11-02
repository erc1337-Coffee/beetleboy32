#include "ui/screens/sounds_screen.h"

#include <stdint.h>

#include "media/sounds.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t ROW_H = 44, NAME_W = UI_VHS_ROW_W;

void SoundsScreen::build(lv_obj_t *root) {
  theme::style_blue_screen(root);
  lv_obj_t *list = ui_vhs_list(root, UI_VHS_LIST_TOP);
  for (int i = 0; i < (int)Sound::Count; i++) {
    lv_obj_t *row = ui_vhs_list_row(list, ROW_H, onPlay, (void *)(intptr_t)i);
    ui_vhs_one_line(ui_vhs_text(row, sound_name((Sound)i), NAME_W));
  }
  addBackButton(root, BackLook::Text);
}

void SoundsScreen::onPlay(lv_event_t *e) { play_sound((Sound)(intptr_t)lv_event_get_user_data(e)); }
