#pragma once

#include <lvgl.h>

#include <vector>

// What a claim or hunt won, shown over `parent` (~3 s), then it cleans itself up:
//  1. each icon grows from nothing (1 s, with a little overshoot); the main one spins in as it does,
//  2. wiggles bigger/smaller 3 times while a glare sweeps across it, left to right,
//  3. hops, then tumbles off the bottom of the screen.
// The main item plays at `center` first; secondary ones follow a beat apart, at half size, alternating
// left and right of it. Every item gets its own timings, so no two move in lockstep.
struct LootIcon {
  const lv_image_dsc_t *image;  // must outlive the animation
  bool secondary;
};
// Returns how long it plays, in ms.
uint32_t play_loot(lv_obj_t *parent, const std::vector<LootIcon> &icons, lv_point_t center);

// Fast-forwards the loot and messages playing over `parent`, so the next can play alone: each item
// plays what's left of its timeline in a fraction of a second (one not on screen yet is dropped),
// each message fades out.
void hurry_loot(lv_obj_t *parent);

// A shower of `icon` falling across the whole screen, from above its top edge to below its bottom
// (~3 s, sizes and speeds vary per drop), then it cleans itself up. E.g. cheese on a cheese claim.
// Returns how long it plays, in ms.
uint32_t play_rain(lv_obj_t *parent, const lv_image_dsc_t *icon);

// A short white line at `center` ("Nothing..", "Error") that fades out after a moment; `boxed`: dark
// on a white box instead.
void play_loot_message(lv_obj_t *parent, const char *text, lv_point_t center, bool boxed = false);
