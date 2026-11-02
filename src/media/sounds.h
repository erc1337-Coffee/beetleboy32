#pragma once

#include <stddef.h>

// The website's sound effects (remilia.com's Web Audio code), redone for the buzzer: one note at a
// time, so chords become quick arpeggios. Most are moved to a random key each time (0, 2, 4, 7 or 9
// semitones up, never the same twice in a row), as the website does.
enum class Sound {
  HuntSuccess,    // beetle hunt result
  ClaimSuccess,   // beetle trap claimed
  ClaimPress,     // pressing claim
  CheeseSuccess,  // cheese claimed
  CheeseSquish,   // the cheese button
  ItemClick,      // a beetle item
  HuntLoop,       // while hunting (2 of the website's 12 rounds)
  CheeseLoop,     // while claiming cheese (one round of 4 chords)
  Notification,
  Poke,
  Friendship,     // friendship explosion
  Success,
  Achievement,
  Triumph,        // onboarding done
  Error,
  MenuOpen,
  MenuClose,
  Click,
  SecondaryClick,
  Tick,           // checkbox
  Hover,
  Transition,
  Mystery,
  Roulette,
  CashRegister,
  CraftSuccess,   // assembled
  CraftFail,      // nothing made (assemble)
  SmashSuccess,
  SmashFail,      // unlucky roll
  HammerBreak,
  Count
};

const char *sound_name(Sound sound);  // short, upper case
void play_sound(Sound sound);         // cuts off whatever the buzzer was playing
void play_picture_tune();             // the hidden picture's (App::showPicture), not one of the website's
