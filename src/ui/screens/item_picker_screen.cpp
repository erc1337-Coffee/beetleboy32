#include "ui/screens/item_picker_screen.h"

#include <stdint.h>

#include "app/app.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t TOP = BELOW_BACK + 6;
static constexpr int32_t SIDE = 14, GAP = 8, ROW_GAP = 12;  // the row gap clears the 3D side
static constexpr int32_t BIG_GAP = 14;                       // between big tiles
static constexpr int32_t PAGE_ROW_GAP = 8;                   // fixed pages: tighter rows
static constexpr int32_t SQUISH_ROOM = 4;  // above the top row: the press animation grows a tile
static constexpr int32_t NAV_GAP = 4;      // between a page and the page arrows below it
static constexpr int32_t TITLE_Y = 12;     // a section's name, top right of Back
static constexpr uint32_t NOTHING_COLOR = 0x55575c;
static constexpr uint32_t ICON_CHECK_MS = 200;
// Built a few at a time: the first screenful at once, the rest over the next frames (making ~100
// tiles in one go froze the screen).
static constexpr size_t FIRST_TILES = 16, TILES_PER_FRAME = 8;
static constexpr intptr_t REMOVE = -1;  // the Remove tile's user data

void ItemPickerScreen::build(lv_obj_t *root) {
  if (options_.background) {
    lv_obj_set_style_bg_image_src(root, options_.background, 0);
    lv_obj_set_style_bg_image_recolor_opa(root, LV_OPA_TRANSP, 0);  // as is, not dimmed
  } else {
    theme::style_light_screen(root);  // like Settings
  }

  // The pages: item ranges.
  pages_.clear();
  if (options_.bySection) {
    for (size_t i = 0; i < items_.size(); i++)
      if (i == 0 || items_[i].section != items_[i - 1].section) pages_.push_back({i, i});
    for (size_t p = 0; p < pages_.size(); p++) {
      pages_[p].second = p + 1 < pages_.size() ? pages_[p + 1].first : items_.size();
    }
  } else if (options_.pageSize) {
    for (size_t i = 0; i < items_.size(); i += options_.pageSize)
      pages_.push_back({i, i + options_.pageSize < items_.size() ? i + options_.pageSize : items_.size()});
  }
  if (pages_.empty()) pages_.push_back({0, items_.size()});

  grid_ = lv_obj_create(root);
  lv_obj_remove_style_all(grid_);
  lv_obj_set_pos(grid_, SIDE, TOP);
  const int32_t nav_top = SCREEN_H - UI_PAGE_NAV_H;
  lv_obj_set_size(grid_, SCREEN_W - 2 * SIDE,
                  paged() ? nav_top - TOP - (options_.bySection ? NAV_GAP : 0) : SCREEN_H - TOP);
  lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
  if (options_.centered) {
    lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  }
  lv_obj_set_style_pad_column(grid_, options_.centered ? BIG_GAP : GAP, 0);
  lv_obj_set_style_pad_row(grid_, options_.pageSize ? PAGE_ROW_GAP : ROW_GAP, 0);
  // Headroom so the press animation isn't clipped; fixed pages have no spare height.
  lv_obj_set_style_pad_top(grid_, options_.pageSize ? 0 : SQUISH_ROOM, 0);
  lv_obj_set_style_pad_bottom(grid_, paged() ? NAV_GAP : theme::SAFE_PAD, 0);
  lv_obj_set_scrollbar_mode(grid_, LV_SCROLLBAR_MODE_OFF);
  if (options_.pageSize) {
    lv_obj_remove_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);  // exactly a page
  } else {
    lv_obj_set_scroll_dir(grid_, LV_DIR_VER);
  }

  nav_ = {};
  if (paged()) nav_ = ui_page_nav(root, /*plated=*/true, onPage, this);
  if (options_.bySection) {
    title_ = ui_plate_label(root);
    lv_obj_align(title_, LV_ALIGN_TOP_RIGHT, -theme::SAFE_PAD, TITLE_Y);
  }

  showPage(page_ < pages_.size() ? page_ : 0);
  if (items_.empty() && !options_.removable) {
    ui_label(grid_, &font_menlo_16, lv_color_hex(NOTHING_COLOR), "Nothing here");
  }
  addBackButton(root);  // a tile: the background may be light
}

// A page's tiles (the first ones now, the rest over the next frames), and its icons first in line.
// The page left: its downloads not done yet are dropped; what's loaded stays (paging back is quick).
void ItemPickerScreen::showPage(size_t page) {
  if (!tiles_.empty()) {
    std::vector<std::string> left;
    for (size_t i = pages_[page_].first; i < pages_[page_].second; i++) left.push_back(items_[i].icon);
    app().dropQueuedIcons(left);
  }
  page_ = page;
  lv_obj_clean(grid_);
  lv_obj_scroll_to_y(grid_, 0, LV_ANIM_OFF);
  tiles_.clear();
  shown_.clear();
  if (options_.removable) {
    UiItemTile remove = ui_item_tile(grid_, options_.tile, Sound::ItemClick, /*flat=*/true);
    ui_item_tile_set(remove, nullptr, LV_SYMBOL_CLOSE, "", ui_rarity_colors(0));
    lv_obj_add_event_cb(remove.key, onPick, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(remove.key, (void *)REMOVE);
  }
  addTiles(FIRST_TILES);

  std::vector<std::string> icons;
  for (size_t i = pages_[page].first; i < pages_[page].second; i++) icons.push_back(items_[i].icon);
  app().loadItemIcons(icons, /*first=*/paged());  // the page on screen before anything else

  if (nav_.label) {
    // Wrapping, both arrows work as soon as there's another page.
    const bool more = pages_.size() > 1, wrap = options_.wrapPages;
    ui_page_nav_set(nav_, page + 1, pages_.size(), more && (wrap || page > 0),
                    more && (wrap || page + 1 < pages_.size()));
  }
  if (title_) {
    const size_t first = pages_[page].first;
    lv_label_set_text(title_, first < items_.size() ? items_[first].section.c_str() : "");
  }
  checkedAt_ = hal_millis();
}

void ItemPickerScreen::addTiles(size_t count) {
  const size_t end = pages_[page_].second;
  for (size_t i = pages_[page_].first + tiles_.size(); i < end && count; i++, count--) {
    const Item &item = items_[i];
    UiItemTile tile = ui_item_tile(grid_, options_.tile, Sound::ItemClick, /*flat=*/true);
    lv_obj_set_user_data(tile.key, (void *)(intptr_t)i);
    lv_obj_add_event_cb(tile.key, onPick, LV_EVENT_CLICKED, this);
    if (!item.enabled) ui_set_enabled(tile.key, false);
    const lv_image_dsc_t *icon =
        item.icon.empty() ? nullptr : app().itemIcon(item.icon, ui_item_tile_icon_size(options_.tile));
    ui_item_tile_set(tile, icon, item.name.c_str(), item.label.c_str(), ui_rarity_colors(item.rarity));
    tiles_.push_back(tile);
    shown_.push_back(icon != nullptr || item.icon.empty());
  }
}

// Closing: nothing is drawn again before ~Screen deletes the tiles, so the images can go now.
ItemPickerScreen::~ItemPickerScreen() {
  std::vector<std::string> icons;
  for (const Item &item : items_) icons.push_back(item.icon);
  app().releaseItemIcons(icons, options_.releaseAll ? 0 : ui_item_tile_icon_size(options_.tile));
}

void ItemPickerScreen::onPage(lv_event_t *e) {
  auto *self = static_cast<ItemPickerScreen *>(lv_event_get_user_data(e));
  int dir = (int)(intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  const size_t count = self->pages_.size();
  if (self->options_.wrapPages && count > 1) {
    self->showPage((self->page_ + count + dir) % count);
    return;
  }
  if (dir < 0 && self->page_ == 0) return;
  size_t page = self->page_ + dir;
  if (page < count) self->showPage(page);
}

// The rest of the page's tiles, then only the ones whose icon just arrived change (redrawing them all
// would stall the scrolling).
void ItemPickerScreen::update() {
  if (pages_[page_].first + tiles_.size() < pages_[page_].second) addTiles(TILES_PER_FRAME);
  if (hal_millis() - checkedAt_ < ICON_CHECK_MS) return;
  checkedAt_ = hal_millis();
  for (size_t i = 0; i < tiles_.size(); i++) {
    const Item &item = items_[pages_[page_].first + i];
    if (shown_[i] || app().itemIconFailed(item.icon)) continue;
    const lv_image_dsc_t *icon = app().itemIcon(item.icon, ui_item_tile_icon_size(options_.tile));
    if (!icon) continue;
    shown_[i] = true;
    lv_image_set_src(tiles_[i].icon, icon);
    lv_obj_remove_flag(tiles_[i].icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(tiles_[i].text, LV_OBJ_FLAG_HIDDEN);
  }
}

void ItemPickerScreen::onPick(lv_event_t *e) {
  auto *self = static_cast<ItemPickerScreen *>(lv_event_get_user_data(e));
  intptr_t i = (intptr_t)lv_obj_get_user_data(static_cast<lv_obj_t *>(lv_event_get_current_target(e)));
  if (i != REMOVE && !self->items_[(size_t)i].enabled) return;  // greyed out
  std::string key = i == REMOVE ? "" : self->items_[(size_t)i].key;
  self->pick_(key);  // before closing: the bench below takes it when shown again
  self->app().closeTop();
}
