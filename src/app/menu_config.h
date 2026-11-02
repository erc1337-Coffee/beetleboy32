#pragma once

#include "app/app.h"
#include "assets/assets.h"

// The home menu's tiles, in order (2 per row; scrolls past 4). Edit freely:
//  - label: shown under the icon (ASCII only: the Menlo font has nothing else)
//  - icon:  &img_<name> for assets/<name>.png, or nullptr for a plain placeholder. Only icons in
//           assets/ are built in; scripts/menu_icons.sh makes them from square images.
//  - open:  the App method that opens the tile's screen, or nullptr (the tile does nothing)
//  - ready: what it needs has loaded (e.g. the game's data), greyed out until then; nullptr: always

struct MenuEntry {
  const char *label;
  const void *icon;
  void (App::*open)();
  bool (App::*ready)() const;
};

inline const MenuEntry HOME_MENU[] = {
    {"HUNT", &img_menu_pfp_125, &App::openHunt, &App::gameLoaded},
    {"CRAFT", &img_menu_pfp_021, &App::openCraft, &App::gameLoaded},
    {"SETTINGS", &img_menu_pfp_004, &App::openSettings, nullptr},
    {"ABOUT", &img_menu_pfp_096, &App::openAbout, nullptr},
};
