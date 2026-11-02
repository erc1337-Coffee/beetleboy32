#pragma once

#include <stddef.h>

#include <vector>

#include "ui/screen.h"
#include "ui/widgets.h"

// The bell (menu): the pokes received lately, a few per page, newest first. A row: the poker's PFP
// and name, then Poke back or how long until they can be poked. App reads the list as pages need it,
// and only the rows on screen get their PFP and cooldown.
class NotificationsScreen : public Screen {
 public:
  explicit NotificationsScreen(App &app) : Screen(app) {}
  ~NotificationsScreen() override;  // the list and its PFPs go
  const char *name() const override { return "notifications"; }
  void update() override;  // rows as their entries and details arrive, the countdowns

 protected:
  void build(lv_obj_t *root) override;

 private:
  enum class Right { None, Loading, PokeBack, Waiting };
  struct Row {
    lv_obj_t *pfp = nullptr;  // an image once the PFP is there, a blank square until then
    lv_obj_t *name = nullptr;
    lv_obj_t *right = nullptr;  // what's on the right goes in here
    bool pfpShown = false;
    Right shown = Right::None;
    long waitShown = -1;  // minutes (or seconds under one) on screen
  };
  void showPage(size_t page);
  void buildRows();  // the page's entries read so far
  void updateNav();
  void updateRow(size_t i);
  static void onPage(lv_event_t *e);
  static void onPokeBack(lv_event_t *e);

  size_t page_ = 0;
  std::vector<Row> rows_;
  size_t rowsBuilt_ = 0;  // entries the rows were made for (more may come in)
  bool moreShown_ = false, failedShown_ = false;  // what the note and arrows were made for
  size_t sizeShown_ = 0;
  lv_obj_t *list_ = nullptr, *note_ = nullptr;
  UiPageNav nav_{};
};
