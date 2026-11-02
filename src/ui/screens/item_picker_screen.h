#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui/item_tile.h"
#include "ui/screen.h"
#include "ui/widgets.h"

// A grid of items to pick one from (the craft bench's slots, hammer, sacrifice; the PFPs). Picking
// one (or Remove, first, if offered) calls `pick` and closes it. Three ways to lay it out:
//  - one scrolling grid (a few items: the hammers, the sacrifice);
//  - pages of `pageSize`, no scrolling (the PFPs);
//  - a page per Item::section, each scrolling, its name on top (the slots: Flowers, Junk...).
// When paged, only the page on screen loads its icons, ahead of other downloads. Pages seen stay
// loaded until the picker closes; then its grid-sized icons are freed (all of them with `releaseAll`).
class ItemPickerScreen : public Screen {
 public:
  struct Item {
    std::string key;      // what `pick` gets
    std::string icon;     // site path ("" if unknown: the name shows)
    std::string name;
    std::string label;    // under the icon: a count, a break chance...
    std::string section;  // its page, with bySection (items come grouped by it)
    int rarity = 0;
    bool enabled = true;  // greyed out and not pickable otherwise (a hammer not owned)
  };
  struct Options {
    bool removable = false;
    const void *background = nullptr;  // an image (the bench's room), as is; nullptr: Settings' texture
    int32_t tile = 56;
    bool centered = false;  // rows centered (big tiles), not left to right
    size_t pageSize = 0;
    bool bySection = false;
    bool releaseAll = false;  // the icons themselves too, on close (a big set no one else shows)
    bool wrapPages = false;   // back on the first page goes to the last, and next on the last to the first
  };
  using Pick = std::function<void(const std::string &key)>;  // "" = Remove

  ItemPickerScreen(App &app, std::vector<Item> items, Options options, Pick pick)
      : Screen(app), items_(std::move(items)), options_(options), pick_(std::move(pick)) {}
  ~ItemPickerScreen() override;
  const char *name() const override { return "item_picker"; }
  void update() override;  // the rest of a page's tiles, icons as they arrive

 protected:
  void build(lv_obj_t *root) override;

 private:
  bool paged() const { return options_.pageSize || options_.bySection; }
  void showPage(size_t page);
  void addTiles(size_t count);  // the page's next ones
  static void onPick(lv_event_t *e);
  static void onPage(lv_event_t *e);

  std::vector<Item> items_;
  Options options_;
  Pick pick_;
  std::vector<std::pair<size_t, size_t>> pages_;  // item ranges [first, end)
  size_t page_ = 0;
  lv_obj_t *grid_ = nullptr, *title_ = nullptr;
  UiPageNav nav_{};  // paged only
  std::vector<UiItemTile> tiles_;  // the page's, built so far (after Remove)
  std::vector<bool> shown_;        // its icon is on it (or it has none to wait for)
  uint32_t checkedAt_ = 0;
};
