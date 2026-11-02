#pragma once

#include <string>

#include "account/beetle.h"
#include "media/sounds.h"
#include "ui/item_tile.h"
#include "ui/screen.h"

// Craft bench: 3 slots (plus hammer and sacrifice in smash mode), a mode switch and the action key.
// Each mode keeps its bench until Craft is closed.
class CraftScreen : public Screen {
 public:
  explicit CraftScreen(App &app) : Screen(app) {}
  ~CraftScreen() override;  // both benches emptied
  const char *name() const override { return "craft"; }
  void onShow() override;  // back from a picker: what it picked
  void update() override;  // the result, the counts, icons as they arrive

 protected:
  void build(lv_obj_t *root) override;

 private:
  static constexpr int HAMMER = CraftRequest::SLOTS, SACRIFICE = HAMMER + 1, TILES = SACRIFICE + 1;

  void showMode(bool animate);  // the room, the second row, the switch
  void showTiles();             // the squares, from the bench and the inventory
  void openPicker(int tile);
  void showResult();
  void playLoot();
  static void onSwitch(lv_event_t *e);
  static void onTile(lv_event_t *e);
  static void onAction(lv_event_t *e);

  lv_obj_t *switch_ = nullptr, *knob_ = nullptr;
  lv_obj_t *action_ = nullptr, *actionLabel_ = nullptr;
  std::string lootIcon_;  // what a craft made, waiting for its icon to play
  Sound lootSound_ = Sound::CraftSuccess;
  bool lootHammerBroke_ = false;
  uint32_t lootAskedAt_ = 0;
  UiItemTile tiles_[TILES];
  uint32_t resultSeq_ = 0;
  bool busyShown_ = false;
  bool waitingIcons_ = false;  // some squares' icons aren't there yet
  uint32_t checkedAt_ = 0;
  size_t catalogShown_ = 0;    // catalog size and inventory read the squares were drawn with
  uint32_t inventoryShown_ = 0;
};
