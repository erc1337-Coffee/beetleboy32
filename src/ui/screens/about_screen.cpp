#include "ui/screens/about_screen.h"

#include <string>

#include "app/app.h"
#include "app/project_info.h"
#include "assets/assets.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t PLAQUE_Y = BELOW_BACK;  // top of the plaque (180x99 image)
static constexpr int32_t PLAQUE_TEXT_DY = 6;     // the plaque's inner panel sits below its center
static constexpr int32_t CONTENT_Y = 148;        // link, version, button below the plaque
static constexpr int32_t LINK_W = 256;
static constexpr int32_t CHECK_W = 170, CHECK_H = 30;
static constexpr uint32_t NAME_COLOR = 0x1a1a1a, NAME_HIGHLIGHT = 0xf2f2f2;

void AboutScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);
  const theme::UserPalette pal = app().palette();

  // Plaque (scripts/plaque.sh): the frame takes the theme hue, the center panel stays gray.
  lv_obj_t *plaque = lv_image_create(root);
  lv_image_set_src(plaque, &img_about_plaque);
  lv_obj_set_style_image_recolor(plaque, pal.border, 0);
  lv_obj_set_style_image_recolor_opa(plaque, LV_OPA_60, 0);
  lv_obj_align(plaque, LV_ALIGN_TOP_MID, 0, PLAQUE_Y);
  // A long-press on it opens the demos (keyboard, sounds, perf overlay).
  lv_obj_add_flag(plaque, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(plaque, [](lv_event_t *e) {
    static_cast<AboutScreen *>(lv_event_get_user_data(e))->app().openDemos();
  }, LV_EVENT_LONG_PRESSED, this);
  lv_obj_t *panel = lv_image_create(root);
  lv_image_set_src(panel, &img_about_plaque_inner);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, PLAQUE_Y);

  // Embossed name: a light copy 1px down-right, the dark text on top.
  for (bool highlight : {true, false}) {
    const lv_color_t color = lv_color_hex(highlight ? NAME_HIGHLIGHT : NAME_COLOR);
    lv_obj_t *name = ui_label(root, &font_hei_18, color, "BeetleBoy32");
    lv_obj_align_to(name, plaque, LV_ALIGN_CENTER, highlight ? 1 : 0, PLAQUE_TEXT_DY + (highlight ? 1 : 0));
  }

  std::string url = project::REPO_URL;
  size_t scheme = url.find("://");
  if (scheme != std::string::npos) url = url.substr(scheme + 3);
  lv_obj_t *link =
      ui_fit_line(root, url.c_str(), LINK_W, {theme::font_body(), theme::font_small(), theme::font_tiny()});
  lv_obj_set_style_text_color(link, lv_color_hex(theme::INK), 0);
  lv_obj_align(link, LV_ALIGN_TOP_MID, 0, CONTENT_Y);

  lv_obj_t *version = lv_label_create(root);
  lv_label_set_text_fmt(version, "Version %s", project::VERSION);
  lv_obj_set_style_text_color(version, lv_color_hex(theme::INK_MUTED), 0);
  lv_obj_align(version, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 20);

  lv_obj_t *check = lv_button_create(root);
  lv_obj_remove_style_all(check);
  theme::style_tile(check);
  ui_squish(check);
  lv_obj_set_size(check, CHECK_W, CHECK_H);
  lv_obj_align(check, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 44);
  lv_obj_t *label = lv_label_create(check);
  lv_label_set_text(label, "Check for updates");
  lv_obj_center(label);
  lv_obj_add_event_cb(check, onCheckUpdates, LV_EVENT_CLICKED, label);

  addBackButton(root);
}

// Placeholder for update checks (not implemented yet).
void AboutScreen::onCheckUpdates(lv_event_t *e) {
  hal_log("[about] update check not implemented yet (current %s)\n", project::VERSION);
  lv_label_set_text(static_cast<lv_obj_t *>(lv_event_get_user_data(e)), "Coming soon");
}
