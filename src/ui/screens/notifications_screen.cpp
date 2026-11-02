#include "ui/screens/notifications_screen.h"

#include <stdint.h>

#include <algorithm>

#include "app/app.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr size_t PER_PAGE = 3;
static constexpr int32_t TOP = BELOW_BACK + 6;  // the same as the item pickers'
static constexpr int32_t SIDE = 14;
static constexpr int32_t ROW_H = 46, ROW_GAP = 6, ROW_PAD = 7;  // ROW_PAD: inside a row, left and right
static constexpr int32_t NAME_GAP = 8;                          // PFP to name, name to the right side
static constexpr int32_t RIGHT_W = 86, RIGHT_H = 26;            // Poke back, or the cooldown
static constexpr uint32_t PFP_BLANK = 0xdcdde2;  // the PFP's square until it's there
static constexpr int32_t NOTE_Y = -10;            // from the middle: "Loading...", "No pokes yet"...

void NotificationsScreen::build(lv_obj_t *root) {
  theme::style_light_screen(root);  // like Settings

  const int32_t nav_top = SCREEN_H - UI_PAGE_NAV_H;
  list_ = lv_obj_create(root);
  lv_obj_remove_style_all(list_);
  lv_obj_set_pos(list_, SIDE, TOP);
  lv_obj_set_size(list_, SCREEN_W - 2 * SIDE, nav_top - TOP);
  lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list_, ROW_GAP, 0);
  lv_obj_remove_flag(list_, LV_OBJ_FLAG_SCROLLABLE);

  note_ = ui_label(root, &font_menlo_16, lv_color_hex(theme::INK_FAINT));
  lv_obj_align(note_, LV_ALIGN_CENTER, 0, NOTE_Y);

  nav_ = ui_page_nav(root, /*plated=*/false, onPage, this);

  addBackButton(root);
  showPage(page_);
}

NotificationsScreen::~NotificationsScreen() { app().closePokeList(); }  // nothing draws the PFPs anymore

void NotificationsScreen::showPage(size_t page) {
  page_ = page;
  app().showPokeRange(page * PER_PAGE, PER_PAGE);
  buildRows();
}

void NotificationsScreen::buildRows() {
  const auto &list = app().pokeList();
  const size_t first = page_ * PER_PAGE;
  const size_t count = list.size() > first ? std::min(PER_PAGE, list.size() - first) : 0;
  lv_obj_clean(list_);
  rows_.clear();
  rowsBuilt_ = count;
  for (size_t i = 0; i < count; i++) {
    const App::PokeEntry &entry = list[first + i];
    lv_obj_t *row = lv_obj_create(list_);
    lv_obj_remove_style_all(row);
    theme::style_tile(row);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    Row r;
    r.pfp = lv_obj_create(row);
    lv_obj_remove_style_all(r.pfp);
    lv_obj_set_size(r.pfp, App::POKE_ROW_PFP, App::POKE_ROW_PFP);
    lv_obj_set_style_bg_color(r.pfp, lv_color_hex(PFP_BLANK), 0);
    lv_obj_set_style_bg_opa(r.pfp, LV_OPA_COVER, 0);
    lv_obj_align(r.pfp, LV_ALIGN_LEFT_MID, ROW_PAD, 0);

    const std::string &name = entry.poke.displayName.empty() ? entry.poke.handle : entry.poke.displayName;
    r.name = ui_label(row, &font_menlo_16, lv_color_hex(theme::INK), name.c_str());
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    // One line: "..." only cuts a label whose height is set (otherwise it wraps).
    lv_obj_set_size(r.name, SCREEN_W - 2 * SIDE - 2 * ROW_PAD - App::POKE_ROW_PFP - RIGHT_W - 2 * NAME_GAP,
                    lv_font_get_line_height(&font_menlo_16));
    lv_obj_align(r.name, LV_ALIGN_LEFT_MID, ROW_PAD + App::POKE_ROW_PFP + NAME_GAP, 0);

    r.right = lv_obj_create(row);
    lv_obj_remove_style_all(r.right);
    lv_obj_set_size(r.right, RIGHT_W, RIGHT_H);
    lv_obj_align(r.right, LV_ALIGN_RIGHT_MID, -ROW_PAD, 0);
    lv_obj_remove_flag(r.right, LV_OBJ_FLAG_CLICKABLE);
    r.shown = Right::None;
    rows_.push_back(r);
    updateRow(i);
  }
  updateNav();
}

// The note, when there are no rows; the arrows and page count (it grows as the list is read).
void NotificationsScreen::updateNav() {
  const auto &list = app().pokeList();
  const size_t first = page_ * PER_PAGE;
  const size_t count = rowsBuilt_;
  const bool more = moreShown_ = app().pokeListMore();
  failedShown_ = app().pokeListFailed();
  sizeShown_ = list.size();
  const char *note = count                     ? ""
                     : failedShown_            ? "Couldn't load"
                     : more                    ? "Loading..."
                     : page_ == 0              ? "No pokes yet"
                                               : "";
  lv_label_set_text(note_, note);
  const size_t pages = (list.size() + PER_PAGE - 1) / PER_PAGE;
  ui_page_nav_set(nav_, page_ + 1, pages > page_ ? pages : page_ + 1, page_ > 0,
                  first + PER_PAGE < list.size() || more);
}

// The PFP once it's there; on the right: "..." while its details come, Poke back, or the countdown.
void NotificationsScreen::updateRow(size_t i) {
  Row &r = rows_[i];
  const size_t index = page_ * PER_PAGE + i;
  const App::PokeEntry &entry = app().pokeList()[index];

  if (!r.pfpShown) {
    if (const lv_image_dsc_t *pfp = app().pokeListPfp(index)) {
      r.pfpShown = true;
      lv_obj_set_style_bg_opa(r.pfp, LV_OPA_TRANSP, 0);
      lv_image_set_src(lv_image_create(r.pfp), pfp);
    }
  }

  long left = 0;
  Right state = Right::Loading;
  if (entry.statusDone && !app().pokingBack(entry.poke.handle)) {
    const PokeStatus &s = entry.status;
    left = s.cooldownS - (long)((hal_millis() - entry.statusAt) / 1000);
    // Status unknown: Poke back anyway, and the site decides.
    state = !entry.statusKnown || s.canPoke || (s.cooldownS > 0 && left <= 0) ? Right::PokeBack
            : s.cooldownS > 0                                                ? Right::Waiting
                                                                             : Right::None;
  }
  // Minutes once over 60 s, so the label only redraws when its text would change.
  const long wait = left >= 60 ? (left + 59) / 60 : left;
  if (state == r.shown && (state != Right::Waiting || wait == r.waitShown)) return;
  r.shown = state;
  r.waitShown = wait;
  lv_obj_clean(r.right);
  if (state == Right::PokeBack) {
    // Like the poke notification's buttons.
    lv_obj_t *btn = ui_bordered_button(r.right, onPokeBack, this);
    lv_obj_set_size(btn, LV_PCT(100), LV_PCT(100));
    lv_obj_center(ui_label(btn, &font_menlo_14, lv_color_hex(theme::INK), "Poke back"));
    lv_obj_set_user_data(btn, (void *)index);
  } else if (state != Right::None) {
    const std::string wait_text = state == Right::Loading ? "..." : "in " + poke_wait_text(left);
    lv_obj_center(ui_label(r.right, &font_menlo_14, lv_color_hex(theme::INK_FAINT), wait_text.c_str()));
  }
}

void NotificationsScreen::update() {
  const auto &list = app().pokeList();
  const size_t first = page_ * PER_PAGE;
  const size_t count = list.size() > first ? std::min(PER_PAGE, list.size() - first) : 0;
  // The list finished loading and this page turned out empty: step back a page.
  if (!count && page_ > 0 && !app().pokeListMore() && !app().pokeListLoading()) {
    showPage(page_ - 1);
    return;
  }
  if (count != rowsBuilt_) {
    buildRows();
    return;
  }
  if (list.size() != sizeShown_ || app().pokeListMore() != moreShown_ ||
      app().pokeListFailed() != failedShown_) {
    updateNav();
  }
  for (size_t i = 0; i < rows_.size(); i++) updateRow(i);
}

void NotificationsScreen::onPage(lv_event_t *e) {
  auto *self = static_cast<NotificationsScreen *>(lv_event_get_user_data(e));
  int dir = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  if (dir < 0 && self->page_ == 0) return;
  self->showPage(self->page_ + dir);
}

void NotificationsScreen::onPokeBack(lv_event_t *e) {
  auto *self = static_cast<NotificationsScreen *>(lv_event_get_user_data(e));
  size_t index = (size_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  if (index < self->app().pokeList().size()) self->app().pokeBack(self->app().pokeList()[index].poke);
}
