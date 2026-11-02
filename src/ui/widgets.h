#pragma once

#include <lvgl.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "board_config.h"
#include "media/sounds.h"
#include "ui/screen.h"
#include "ui/theme.h"

// Small builders so screens read as layout, not styling.

// --- Layout and text

lv_obj_t *ui_column(lv_obj_t *root);  // full-screen, centered vertical stack inside the safe area

// Full-screen two-pane layout: `left` sized to its content (e.g. a QR code), `right` takes the rest.
struct UiSplit {
  lv_obj_t *left;
  lv_obj_t *right;
};
UiSplit ui_split(lv_obj_t *root);
lv_obj_t *ui_row(lv_obj_t *parent);  // horizontal strip, full width; children share it via flex grow
lv_obj_t *ui_title(lv_obj_t *parent, const char *text);
lv_obj_t *ui_text(lv_obj_t *parent, const char *text);   // wrapped, centered, muted
lv_obj_t *ui_value(lv_obj_t *parent, const char *text);  // wrapped, centered, full-contrast
// A plain label in `font` and `color`. Without `text`, it shows LVGL's placeholder until set.
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text = nullptr);
// Sets the text only if it differs: setting a label's text redraws it, even when it's the same.
void ui_set_text_if_changed(lv_obj_t *label, const char *text);
// Single line, no wrapping or scrolling: uses the largest of `fonts` (largest first) that fits `width`.
lv_obj_t *ui_fit_line(lv_obj_t *parent, const char *text, int32_t width,
                      std::initializer_list<const lv_font_t *> fonts);
// Dark text on a light rounded backing: readable on the craft rooms' busy pictures too.
lv_obj_t *ui_plate_label(lv_obj_t *parent);
lv_obj_t *ui_qr(lv_obj_t *parent, const char *data, int32_t size);
lv_obj_t *ui_loading(lv_obj_t *parent);  // the animated Remilia flower
std::string ui_countdown(uint32_t ms);   // "1h 07mn 42s", "7mn 42s" or "42s" (rounded up)

// --- Buttons

// A default LVGL button that grows to fill a ui_row.
lv_obj_t *ui_button(lv_obj_t *parent, const char *label, lv_event_cb_t on_click, void *user_data);
// Greyed out (DISABLED state, theme::DISABLED_OPA) or back to normal. Cheap when nothing changes,
// so it can run every frame.
void ui_set_enabled(lv_obj_t *obj, bool enabled);

// Whitish tile (theme::style_tile) with an optional 20px icon left of a Menlo label, centered.
struct UiIconTile {
  lv_obj_t *button;
  lv_obj_t *label;
};
UiIconTile ui_icon_tile(lv_obj_t *parent, const void *icon, const char *text, int32_t w, int32_t h,
                        Sound press = Sound::ItemClick);

// A whitish tile showing a setting's value in `label` (the caller sets it), and a down arrow if
// `arrow` (when tapping it opens a list of choices).
lv_obj_t *ui_value_button(lv_obj_t *parent, int32_t w, int32_t h, bool arrow, lv_obj_t **label);
// A list of choices over a dark veil covering `parent`: squares 3 a row, centered, `current` (an
// index, -1 for none) darker. Clicking one calls `on_choice`, and the clicked object's user data is
// its index. Returns the veil: delete it to close the list.
lv_obj_t *ui_choice_veil(lv_obj_t *parent, const std::vector<std::string> &choices, int current,
                         lv_event_cb_t on_choice, void *user_data);

// A rectangle with a thin border (theme::BORDER), for the caller to size and put a label in. With
// `on_click` it's a button that darkens and squishes while pressed; without, it isn't clickable.
lv_obj_t *ui_bordered_button(lv_obj_t *parent, lv_event_cb_t on_click = nullptr, void *user_data = nullptr);

// Page arrows at the bottom middle, with "x/y" between them. Each arrow's user data is its
// direction (-1 or 1), for `on_page`. `plated`: the count on a light backing, for busy backgrounds.
struct UiPageNav {
  lv_obj_t *prev, *next, *label;
};
constexpr int32_t UI_PAGE_NAV_H = 46;  // from the screen's bottom edge to the top of the arrows
UiPageNav ui_page_nav(lv_obj_t *parent, bool plated, lv_event_cb_t on_page, void *user_data);
// Shows "page/pages" (page counted from 1); an arrow that can't be used is greyed out.
void ui_page_nav_set(const UiPageNav &nav, size_t page, size_t pages, bool can_prev, bool can_next);

// Pressing squishes `obj` (wider and shorter, standing on its bottom edge), then it springs back,
// and plays `press` on the buzzer. Every button has it.
void ui_squish(lv_obj_t *obj, Sound press = Sound::ItemClick);

// --- Keycaps

// Chunky keyboard-key button: a gradient face over a darker edge (its 3D side); it sinks when pressed.
struct UiKeyColors {
  uint32_t top, bottom;  // face gradient
  uint32_t edge;         // the key's side and outline
  uint32_t text;
};
constexpr int32_t UI_KEY_DEPTH = 5;  // how far a keycap's side shows below its face
// A plain whitish key: the craft bench's action key, the choices of ui_choice_veil.
constexpr UiKeyColors UI_WHITE_KEY = {0xf3f3f4, 0xd5d6d9, 0x4a4b50, theme::INK};
lv_obj_t *ui_keycap(lv_obj_t *parent, int32_t w, int32_t h, const UiKeyColors &colors,
                    Sound press = Sound::ItemClick);
void ui_keycap_colors(lv_obj_t *key, const UiKeyColors &colors);  // e.g. on a state change

// --- The blue screen (VHS)

// VHS on-screen-display text: white at 90% with faint red/cyan ghosts either side (the tape's
// color bleed). Wraps and centers within `width`. Change it with ui_vhs_set_text().
lv_obj_t *ui_vhs_text(lv_obj_t *parent, const char *text, int32_t width);
void ui_vhs_set_text(lv_obj_t *vhs, const char *text);
void ui_vhs_one_line(lv_obj_t *vhs);  // left-aligned on one line, cut with "..." past its width
void ui_vhs_set_width(lv_obj_t *vhs, int32_t width);
// VHS text filling a ui_column: its inner width, less 2 px for the ghosts.
constexpr int32_t UI_COLUMN_VHS_W = SCREEN_W - 2 * theme::SAFE_PAD - 2;

// A vertically scrolling list on the blue screen, from `top` to the bottom edge; rows go in it.
lv_obj_t *ui_vhs_list(lv_obj_t *root, int32_t top);
// One row of it, `height` tall and full width: a flex row (children left to right, centered
// vertically), lit while pressed. Clickable only if `on_click` is given (user data: `user_data`).
lv_obj_t *ui_vhs_list_row(lv_obj_t *list, int32_t height, lv_event_cb_t on_click = nullptr,
                          void *user_data = nullptr);
constexpr int32_t UI_VHS_LIST_TOP = BELOW_BACK + 4;  // where the lists start: a little air under Back
// The list's inset from the screen's sides (they're straight at mid-height); a row's padding left
// and right, and the gap between its children.
constexpr int32_t UI_VHS_LIST_SIDE = 12, UI_VHS_ROW_PAD = 6, UI_VHS_ROW_GAP = 8;
constexpr int32_t UI_VHS_ROW_W = SCREEN_W - 2 * UI_VHS_LIST_SIDE - 2 * UI_VHS_ROW_PAD;  // for a row's content

// A text-only button for the blue screen: white VHS-style label, a faint light while pressed, squish.
// The label wraps past `text_w`; the default fits two buttons to a row.
constexpr int32_t UI_VHS_BUTTON_TEXT_W = 100;
lv_obj_t *ui_vhs_button(lv_obj_t *parent, const char *label, lv_event_cb_t on_click, void *user_data,
                        int32_t text_w = UI_VHS_BUTTON_TEXT_W);
lv_obj_t *ui_vhs_button_row(lv_obj_t *parent);  // full width; its buttons share it, spread evenly

// The Wi-Fi fan, in white for the blue screen: a dot and 3 arcs, `size` px wide. ui_wifi_bars_set()
// lights the dot and the first `bars` arcs (0-3); the rest stay faint.
lv_obj_t *ui_wifi_bars(lv_obj_t *parent, int32_t size);
void ui_wifi_bars_set(lv_obj_t *icon, int bars);
int ui_wifi_bars_for(int rssi);  // dBm to 1-3 (0: no link, rssi 0)
