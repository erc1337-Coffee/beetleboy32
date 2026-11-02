#include "ui/screens/craft_screen.h"

#include <ctype.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "app/app.h"
#include "assets/assets.h"
#include "board_config.h"
#include "fonts/fonts.h"
#include "hal/hal.h"
#include "ui/loot_animation.h"
#include "ui/screens/item_picker_screen.h"
#include "ui/theme.h"
#include "ui/widgets.h"

// Like the website's bench, each mode with its own room behind. Tapping a square opens a picker of
// what can go there; no recipes are shown, since what comes out is the game's secret.
// Layout (screen is 280x240): the switch top right; the 3 slots, then the hammer, the sacrifice and
// SMASH (or ASSEMBLE across the whole row). The rows are centered horizontally.
static constexpr int32_t TILE_SIZE = 64, GAP = 16, ROW1_Y = 60, ROW2_Y = 152;
static constexpr int32_t X0 = (SCREEN_W - 3 * TILE_SIZE - 2 * GAP) / 2;
static constexpr int32_t SWITCH_W = 112, SWITCH_H = 36, SWITCH_Y = 4, KNOB_W = 60, KNOB_PAD = 3;
static constexpr int32_t SWITCH_X = SCREEN_W - theme::SAFE_PAD - SWITCH_W;
static constexpr int32_t SWITCH_RADIUS = 11, KNOB_RADIUS = 8, KNOB_SHADOW = 4, SWITCH_TOUCH = 8;
static constexpr int RIBS = 7;  // on the knob
static constexpr int32_t RIB_W = 2, RIB_STEP = 6, RIB_INSET = 5;  // inset from the knob's top and bottom
static constexpr uint32_t SWITCH_BORDER = 0x6d7078, RIB_COLOR = 0x55575d;
static constexpr uint32_t KNOB_TOP = 0xf6f6f7, KNOB_BOTTOM = 0xc9cacd;  // its gradient
static constexpr uint32_t SWITCH_MS = 150, BLINK_MS = 120, ICON_CHECK_MS = 200;
// An empty square and the switch's track: whitish, a shade below the action key (UI_WHITE_KEY).
static constexpr UiKeyColors EMPTY_COLORS = {0xe4e5e8, 0xc9cbcf, 0x55575c, theme::INK};
// The pickers: the ingredients 4 a row; the hammers and the sacrifice, bigger, centered.
static constexpr int32_t PICKER_TILE = 56, PICKER_BIG_TILE = 72;
// The result: the item made pops up over the middle of the bench (as a hunt's loot does), and
// "Hammer broke!" shows lower down.
static constexpr lv_point_t LOOT_CENTER = {SCREEN_W / 2, 110};
static constexpr lv_point_t HAMMER_BROKE_AT = {SCREEN_W / 2, 200};
static constexpr uint32_t LOOT_WAIT_MS = 8000;  // waited for at most while the item's icon downloads

static constexpr int THIRD_SLOT = 2;  // greyed out until the account unlocks it
// The game's item categories (Catalog's `category`) that the slots care about.
static constexpr int CATEGORY_BEETLES = 2, CATEGORY_JUNK = 3;

static const char *const HAMMERS[] = {"hammer_t1", "hammer_t2", "hammer_t3", "hammer_t4", "hammer_t5"};
static const char *const SACRIFICES[] = {"green", "purple"};

// Each mode's bench (what's on it), kept while switching between them; emptied when Craft is left.
static CraftRequest benches[2] = {{}, {true}};  // assemble, smash
static bool smashing = false;                   // the mode shown (kept)
static CraftRequest &bench() { return benches[smashing]; }

// Each mode's room, behind the bench.
static const lv_image_dsc_t *room() {
  return smashing ? &img_craft_smash_poster : &img_craft_assemble_poster;
}

static bool starts_with(const std::string &s, const char *prefix) { return s.rfind(prefix, 0) == 0; }

// Whether `key` may go in a slot, as the game allows: never cheese, trophies, hammers or shelves
// (profile decorations). Assembling takes no beetles; smashing takes beetles and junk cubes, but no
// other junk.
static bool fits(const Catalog &catalog, const std::string &key, bool smash) {
  if (key == "cheese" || starts_with(key, "trophy_") || starts_with(key, "hammer_")) return false;
  if (key.size() > 6 && key.compare(key.size() - 6, 6, "_shelf") == 0) return false;
  auto card = catalog.find(key);
  if (card != catalog.end() && card->second.icon.find("/shelves/") != std::string::npos) return false;
  int category = card == catalog.end() ? 0 : card->second.category;
  if (smash) return category != CATEGORY_JUNK || starts_with(key, "junk_cube");
  return category != CATEGORY_BEETLES;
}

// The slot picker's pages, in this order: the icon's folder on the site, then the page's title.
// Other folders come after these, and items with no known folder last ("Other").
static const char *const FAMILIES[][2] = {
  {"beetles", "Beetles"},
  {"flowers", "Flowers"},
  {"pollen", "Pollen"},
  {"artifacts", "Artifacts"},
  {"junk", "Junk"},
  {"trinkets", "Trinkets"},
  {"unique", "Unique"},
};

static constexpr int FAMILY_COUNT = sizeof(FAMILIES) / sizeof(FAMILIES[0]);

// The page `key` goes on: its sort order and title.
static std::pair<int, std::string> family(const Catalog &catalog, const std::string &key) {
  std::string folder;
  if (starts_with(key, "pollen")) {
    folder = "pollen";
  } else if (auto card = catalog.find(key); card != catalog.end()) {
    const std::string &icon = card->second.icon;
    size_t at = icon.find("/icons/");
    if (at != std::string::npos) folder = icon.substr(at + 7, icon.find('/', at + 7) - (at + 7));
  }
  for (int i = 0; i < FAMILY_COUNT; i++)
    if (folder == FAMILIES[i][0]) return {i, FAMILIES[i][1]};
  if (folder.empty()) return {FAMILY_COUNT + 1, "Other"};
  folder[0] = (char)toupper((unsigned char)folder[0]);
  return {FAMILY_COUNT, folder};
}

static int break_rate(const BeetleState &b, const std::string &hammer) {  // -1: not one of theirs
  for (const HammerState &h : b.hammers)
    if (h.key == hammer) return h.breakRate;
  return -1;
}
static bool owns(const BeetleState &b, const std::string &hammer) {
  return b.count(hammer) > 0 || break_rate(b, hammer) >= 0;
}

// How many of `key` the slots hold, not counting slot `except` (-1: all of them).
static int in_slots(const std::string &key, int except) {
  int n = 0;
  for (int i = 0; i < CraftRequest::SLOTS; i++) n += i != except && bench().slots[i] == key;
  return n;
}

// How many of `key` are left besides those on the bench (2 flowers owned, both on it: 0 left).
static long available(const BeetleState &b, const std::string &key) {
  return b.count(key) - in_slots(key, -1) - (smashing && bench().sacrifice == key);
}

// Clears what can't stay on the bench: what ran out, what this mode doesn't take, and the third slot
// while it's locked.
static void prune(const BeetleState &b, const Catalog &catalog) {
  std::map<std::string, long> used;
  for (int i = 0; i < CraftRequest::SLOTS; i++) {
    std::string &key = bench().slots[i];
    if (key.empty()) continue;
    bool locked = i == THIRD_SLOT && b.loaded && !b.slot3Unlocked;
    if (locked || !fits(catalog, key, smashing) || (b.loaded && ++used[key] > b.count(key))) key.clear();
  }
  if (b.loaded && !bench().hammer.empty() && !owns(b, bench().hammer)) bench().hammer.clear();
  const std::string &sacrifice = bench().sacrifice;
  if (b.loaded && !sacrifice.empty() && b.count(sacrifice) - used[sacrifice] < 1) bench().sacrifice.clear();
}

// Points at a missing piece.
static void blink(lv_obj_t *obj) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(o), (lv_opa_t)v, 0);
  });
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_20);
  lv_anim_set_duration(&a, BLINK_MS);
  lv_anim_set_playback_duration(&a, BLINK_MS);
  lv_anim_set_repeat_count(&a, 2);
  lv_anim_start(&a);
}

static void set_knob_x(void *knob, int32_t x) { lv_obj_set_x(static_cast<lv_obj_t *>(knob), x); }

void CraftScreen::build(lv_obj_t *root) {
  lv_obj_set_style_bg_image_recolor_opa(root, LV_OPA_TRANSP, 0);  // the room is the scene: no dimming

  for (int i = 0; i < TILES; i++) {
    tiles_[i] = ui_item_tile(root, TILE_SIZE);
    int32_t x = i < HAMMER ? X0 + i * (TILE_SIZE + GAP) : X0 + (i - HAMMER) * (TILE_SIZE + GAP);
    lv_obj_set_pos(tiles_[i].key, x, i < HAMMER ? ROW1_Y : ROW2_Y);
    lv_obj_set_user_data(tiles_[i].key, (void *)(intptr_t)i);
    lv_obj_add_event_cb(tiles_[i].key, onTile, LV_EVENT_CLICKED, this);
  }

  action_ = ui_keycap(root, TILE_SIZE, TILE_SIZE, UI_WHITE_KEY, Sound::ClaimPress);
  lv_obj_add_event_cb(action_, onAction, LV_EVENT_CLICKED, this);
  actionLabel_ = lv_label_create(action_);
  lv_obj_set_style_text_font(actionLabel_, &font_menlo_16, 0);
  lv_obj_center(actionLabel_);

  // The switch: a wrench on the left, a hammer on the right (silhouettes); the ribbed knob covers
  // the other mode's.
  switch_ = lv_obj_create(root);
  lv_obj_remove_style_all(switch_);
  lv_obj_set_pos(switch_, SWITCH_X, SWITCH_Y);
  lv_obj_set_size(switch_, SWITCH_W, SWITCH_H);
  lv_obj_set_style_radius(switch_, SWITCH_RADIUS, 0);
  lv_obj_set_style_bg_color(switch_, lv_color_hex(EMPTY_COLORS.top), 0);
  lv_obj_set_style_bg_grad_color(switch_, lv_color_hex(EMPTY_COLORS.bottom), 0);
  lv_obj_set_style_bg_grad_dir(switch_, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(switch_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(switch_, 1, 0);
  lv_obj_set_style_border_color(switch_, lv_color_hex(SWITCH_BORDER), 0);
  lv_obj_set_style_border_opa(switch_, LV_OPA_60, 0);
  lv_obj_set_ext_click_area(switch_, SWITCH_TOUCH);
  lv_obj_remove_flag(switch_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(switch_, onSwitch, LV_EVENT_CLICKED, this);
  const int32_t side = SWITCH_W - KNOB_W;  // what the knob leaves uncovered
  for (int dir : {-1, 1}) {
    lv_obj_t *l = ui_label(switch_, &font_shapes_22, lv_color_black(), dir < 0 ? SHAPE_WRENCH : SHAPE_HAMMER);
    lv_obj_set_style_text_opa(l, theme::SHAPE_OPA, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, dir * (SWITCH_W - side) / 2, 0);
  }
  knob_ = lv_obj_create(switch_);
  lv_obj_remove_style_all(knob_);
  lv_obj_set_size(knob_, KNOB_W, SWITCH_H - 2 * KNOB_PAD);
  lv_obj_set_y(knob_, KNOB_PAD);
  lv_obj_set_style_radius(knob_, KNOB_RADIUS, 0);
  lv_obj_set_style_bg_color(knob_, lv_color_hex(KNOB_TOP), 0);
  lv_obj_set_style_bg_grad_color(knob_, lv_color_hex(KNOB_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(knob_, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(knob_, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(knob_, KNOB_SHADOW, 0);
  lv_obj_set_style_shadow_opa(knob_, LV_OPA_40, 0);
  lv_obj_remove_flag(knob_, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < RIBS; i++) {
    lv_obj_t *rib = lv_obj_create(knob_);
    lv_obj_remove_style_all(rib);
    lv_obj_set_size(rib, RIB_W, SWITCH_H - 2 * KNOB_PAD - 2 * RIB_INSET);
    lv_obj_align(rib, LV_ALIGN_CENTER, (i - RIBS / 2) * RIB_STEP, 0);
    lv_obj_set_style_bg_color(rib, lv_color_hex(RIB_COLOR), 0);
    lv_obj_set_style_bg_opa(rib, LV_OPA_COVER, 0);
  }

  addBackButton(root);
  resultSeq_ = app().beetleResultSeq();  // only results that arrive from now on
  busyShown_ = false;
  lv_label_set_text(actionLabel_, "");
  showMode(false);
}

CraftScreen::~CraftScreen() {
  for (CraftRequest &b : benches) b = CraftRequest{b.smash};
}

void CraftScreen::onShow() {
  app().loadCatalog();
  app().refreshBeetle();
  prune(app().beetle(), app().catalog());
  showTiles();
}

void CraftScreen::showMode(bool animate) {
  lv_obj_set_style_bg_image_src(root(), room(), 0);
  lv_obj_update_flag(tiles_[HAMMER].key, LV_OBJ_FLAG_HIDDEN, !smashing);
  lv_obj_update_flag(tiles_[SACRIFICE].key, LV_OBJ_FLAG_HIDDEN, !smashing);
  if (smashing) {
    lv_obj_set_pos(action_, X0 + 2 * (TILE_SIZE + GAP), ROW2_Y);
    lv_obj_set_width(action_, TILE_SIZE);
  } else {
    lv_obj_set_pos(action_, X0, ROW2_Y);
    lv_obj_set_width(action_, 3 * TILE_SIZE + 2 * GAP);
  }
  if (!busyShown_) lv_label_set_text(actionLabel_, smashing ? "SMASH" : "ASSEMBLE");

  // Smash: the knob over the wrench (the hammer shows). Assemble: over the hammer.
  const int32_t to = smashing ? KNOB_PAD : SWITCH_W - KNOB_W - KNOB_PAD;
  lv_anim_delete(knob_, set_knob_x);
  if (!animate) {
    lv_obj_set_x(knob_, to);
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, knob_);
  lv_anim_set_exec_cb(&a, set_knob_x);
  lv_anim_set_values(&a, lv_obj_get_x(knob_), to);
  lv_anim_set_duration(&a, SWITCH_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

void CraftScreen::showTiles() {
  const BeetleState &b = app().beetle();
  const Catalog &catalog = app().catalog();
  catalogShown_ = catalog.size();
  inventoryShown_ = b.fetchedAt;
  waitingIcons_ = false;
  std::vector<std::string> icons;

  auto show = [&](int i, const std::string &key, const char *empty_text, const std::string &label) {
    if (key.empty()) {
      ui_item_tile_set(tiles_[i], nullptr, empty_text, "", EMPTY_COLORS);
      return;
    }
    auto card = catalog.find(key);
    std::string path = card == catalog.end() ? "" : card->second.icon;
    const std::string &name = card == catalog.end() ? key : card->second.name;
    const lv_image_dsc_t *icon =
        path.empty() ? nullptr : app().itemIcon(path, ui_item_tile_icon_size(TILE_SIZE));
    if (!icon && !path.empty() && !app().itemIconFailed(path)) waitingIcons_ = true;
    icons.push_back(path);
    const int rarity = card == catalog.end() ? 0 : card->second.rarity;
    ui_item_tile_set(tiles_[i], icon, name.c_str(), label.c_str(), ui_rarity_colors(rarity));
  };

  for (int i = 0; i < CraftRequest::SLOTS; i++) {
    bool locked = i == THIRD_SLOT && !b.slot3Unlocked;
    ui_set_enabled(tiles_[i].key, !locked);
    const std::string &key = bench().slots[i];
    show(i, key, "", key.empty() ? "" : std::to_string(available(b, key)));
  }
  int rate = break_rate(b, bench().hammer);
  show(HAMMER, bench().hammer, SHAPE_HAMMER, rate >= 0 ? std::to_string(rate) + "%" : "");
  const std::string &sacrifice = bench().sacrifice;
  show(SACRIFICE, sacrifice, SHAPE_BEETLE, sacrifice.empty() ? "" : std::to_string(available(b, sacrifice)));
  app().loadItemIcons(icons);
  checkedAt_ = hal_millis();
}

void CraftScreen::update() {
  if (app().beetleResultSeq() != resultSeq_) {
    resultSeq_ = app().beetleResultSeq();
    if (app().beetleResult().task == App::BeetleTask::Craft) {
      prune(app().beetle(), app().catalog());
      showTiles();
      showResult();
    }
  }
  bool busy = app().beetleTask() == App::BeetleTask::Craft;
  if (busy != busyShown_) {
    busyShown_ = busy;
    lv_label_set_text(actionLabel_, busy ? "..." : smashing ? "SMASH" : "ASSEMBLE");
  }
  playLoot();  // waiting for its icon
  // The catalog or a fresh inventory read arrived, or icons are still on their way.
  bool stale = app().catalog().size() != catalogShown_ || app().beetle().fetchedAt != inventoryShown_;
  if (stale) prune(app().beetle(), app().catalog());
  if (stale || (waitingIcons_ && hal_millis() - checkedAt_ >= ICON_CHECK_MS)) showTiles();
}

void CraftScreen::openPicker(int tile) {
  const BeetleState &b = app().beetle();
  const Catalog &catalog = app().catalog();
  std::vector<ItemPickerScreen::Item> items;
  auto item = [&](const std::string &key, const std::string &label, bool enabled) {
    auto card = catalog.find(key);
    ItemPickerScreen::Item it;
    it.key = key;
    it.label = label;
    it.enabled = enabled;
    if (card != catalog.end()) {
      it.icon = card->second.icon;
      it.name = card->second.name;
      it.rarity = card->second.rarity;
    } else {
      it.name = key;
    }
    return it;
  };

  std::string current;
  if (tile < HAMMER) {
    current = bench().slots[tile];
    // Everything owned that fits, minus what the other slots already hold: a page per family, each
    // from the most common to the rarest (the junk cubes first on theirs).
    std::vector<std::pair<int, ItemPickerScreen::Item>> owned;  // family order, item
    for (const auto &inv : b.items()) {
      long left = inv.second - in_slots(inv.first, tile);
      if (left <= 0 || !fits(catalog, inv.first, smashing)) continue;
      auto f = family(catalog, inv.first);
      ItemPickerScreen::Item it = item(inv.first, std::to_string(left), true);
      it.section = f.second;
      owned.push_back({f.first, std::move(it)});
    }
    std::sort(owned.begin(), owned.end(), [](const auto &x, const auto &y) {
      if (x.first != y.first) return x.first < y.first;
      if (x.second.section != y.second.section) return x.second.section < y.second.section;
      const bool x_cube = starts_with(x.second.key, "junk_cube");
      const bool y_cube = starts_with(y.second.key, "junk_cube");
      if (x_cube != y_cube) return x_cube;
      if (x.second.rarity != y.second.rarity) return x.second.rarity < y.second.rarity;
      return x.second.name < y.second.name;
    });
    for (auto &o : owned) items.push_back(std::move(o.second));
  } else if (tile == HAMMER) {
    current = bench().hammer;
    for (const char *hammer : HAMMERS) {  // the whole line, the ones not owned greyed out
      int rate = break_rate(b, hammer);
      items.push_back(item(hammer, rate >= 0 ? std::to_string(rate) + "%" : "", owns(b, hammer)));
    }
  } else {
    current = bench().sacrifice;
    for (const char *beetle : SACRIFICES) {
      long left = b.count(beetle) - in_slots(beetle, -1);
      items.push_back(item(beetle, std::to_string(left > 0 ? left : 0), left > 0));
    }
  }

  ItemPickerScreen::Options options;
  options.removable = !current.empty();
  options.background = room();
  options.tile = tile < HAMMER ? PICKER_TILE : PICKER_BIG_TILE;
  options.centered = tile >= HAMMER;
  options.bySection = tile < HAMMER;  // the slots: a page per family
  options.wrapPages = true;
  CraftRequest *on = &bench();
  auto pick = [tile, on](const std::string &key) {
    if (tile < HAMMER) on->slots[tile] = key;
    else if (tile == HAMMER) on->hammer = key;
    else on->sacrifice = key;
  };
  app().pushScreen(std::make_unique<ItemPickerScreen>(app(), std::move(items), options, pick));
}

// Not while a craft is out: its result would come while the picker is up (and the bench rebuilt).
void CraftScreen::onTile(lv_event_t *e) {
  auto *self = static_cast<CraftScreen *>(lv_event_get_user_data(e));
  lv_obj_t *tile = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  if (lv_obj_has_state(tile, LV_STATE_DISABLED) || self->app().beetleTask() != App::BeetleTask::None) return;
  self->openPicker((int)(intptr_t)lv_obj_get_user_data(tile));
}

void CraftScreen::onSwitch(lv_event_t *e) {
  auto *self = static_cast<CraftScreen *>(lv_event_get_user_data(e));
  play_sound(Sound::ItemClick);  // the buttons' click
  smashing = !smashing;  // the other bench, as it was left
  prune(self->app().beetle(), self->app().catalog());
  self->showMode(true);
  self->showTiles();
}

// Missing pieces blink, with the error sound; otherwise off it goes.
void CraftScreen::onAction(lv_event_t *e) {
  auto *self = static_cast<CraftScreen *>(lv_event_get_user_data(e));
  if (self->app().beetleTask() != App::BeetleTask::None) return;
  bool any = false;
  for (const std::string &slot : bench().slots) any = any || !slot.empty();
  bool missing = false;
  if (!any) {
    for (int i = 0; i < CraftRequest::SLOTS; i++)
      if (!lv_obj_has_state(self->tiles_[i].key, LV_STATE_DISABLED)) blink(self->tiles_[i].key);
    missing = true;
  }
  if (smashing && bench().hammer.empty()) {
    blink(self->tiles_[HAMMER].key);
    missing = true;
  }
  if (smashing && bench().sacrifice.empty()) {
    blink(self->tiles_[SACRIFICE].key);
    missing = true;
  }
  if (missing) {
    play_sound(Sound::Error);
    return;
  }
  self->app().craft(bench());
}

// What came of it: the item made plays like a hunt's loot, over the bench, once its icon is here
// (a new one downloads first). Anything else: a short line that fades.
void CraftScreen::showResult() {
  const App::BeetleResult &r = app().beetleResult();
  const CraftReply &c = r.craft;
  lootIcon_.clear();
  if (r.ok && c.success) {
    lootIcon_ = c.item.icon;
    lootSound_ = c.hammerBroke ? Sound::HammerBreak : smashing ? Sound::SmashSuccess : Sound::CraftSuccess;
    lootHammerBroke_ = c.hammerBroke;
    lootAskedAt_ = hal_millis();
    playLoot();
    return;
  }
  Sound sound = Sound::CraftFail;
  const char *text = c.message.c_str();
  if (r.rateLimited) {
    return;  // the rate-limit popup says it
  } else if (!r.ok) {
    sound = Sound::Error;
    text = "Error";
  } else if (c.message == "UNLUCKY_ROLL") {
    sound = Sound::SmashFail;
    text = "Unlucky..";
  } else if (c.message == "ALREADY_OWNED") {
    text = "Already owned";
  } else if (c.message == "INVALID_RECIPE") {
    text = "Not a recipe";
  } else if (c.message == "NOT_ENOUGH_RESOURCES") {
    sound = Sound::Error;
    text = "Not enough";
  }
  play_sound(c.hammerBroke ? Sound::HammerBreak : sound);
  play_loot_message(root(), text, LOOT_CENTER, /*boxed=*/true);
  if (c.hammerBroke) play_loot_message(root(), "Hammer broke!", HAMMER_BROKE_AT, true);
}

// Plays the loot once its icon is here, or given up on (the generic beetle then).
void CraftScreen::playLoot() {
  if (lootIcon_.empty()) return;
  const lv_image_dsc_t *icon = app().itemIcon(lootIcon_);
  if (!icon && !app().itemIconFailed(lootIcon_) && hal_millis() - lootAskedAt_ < LOOT_WAIT_MS) return;
  lootIcon_.clear();
  play_loot(root(), {{icon ? icon : &img_icon_beetle, false}}, LOOT_CENTER);
  play_sound(lootSound_);
  // The item was made, but the hammer was lost.
  if (lootHammerBroke_) play_loot_message(root(), "Hammer broke!", HAMMER_BROKE_AT, /*boxed=*/true);
}
