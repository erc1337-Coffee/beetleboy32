#include "media/sounds.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "hal/hal.h"

// Note frequencies, Hz. In a namespace: Arduino names pins A0-A19.
namespace pitch {
constexpr float A2 = 110, B2 = 123.47f, C3 = 130.81f, D3 = 146.83f, F3 = 174.61f, G3 = 196, GS3 = 207.65f, A3 = 220;
constexpr float C4 = 261.63f, D4 = 293.66f, DS4 = 311.13f, E4 = 329.63f, F4 = 349.23f, G4 = 392, GS4 = 415.3f, A4 = 440;
constexpr float AS4 = 466.16f, B4 = 493.88f;
constexpr float C5 = 523.25f, D5 = 587.33f, E5 = 659.25f, F5 = 698.46f, G5 = 783.99f, A5 = 880;
constexpr float C6 = 1046.5f, CS6 = 1100, E6 = 1318.51f, F6 = 1396.91f, G6 = 1567.98f, GS6 = 1661.22f, A6 = 1760;
constexpr float C7 = 2093, CS7 = 2200, D7 = 2349.32f, E7 = 2637.02f, F7 = 2793.83f, G7 = 3135.96f, A7 = 3520, C8 = 4186.01f;
}  // namespace pitch
using namespace pitch;

// Notes placed on a timeline (ms), as the website schedules them, then played one after the
// other: each is cut where the next starts, and gaps become rests.
class Score {
 public:
  explicit Score(float transpose = 1) : transpose_(transpose) {}
  Score &note(uint32_t at, float hz, uint32_t len) { return glide(at, hz, 0, len); }
  Score &glide(uint32_t at, float from, float to, uint32_t len) {
    events_.push_back({at, len, from, to});
    return *this;
  }

  std::vector<HalNote> notes() const {
    std::vector<Event> events = events_;
    std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.at < b.at; });
    std::vector<HalNote> out;
    uint32_t t = 0;
    for (size_t i = 0; i < events.size(); i++) {
      const Event &e = events[i];
      uint32_t len = e.len;
      if (i + 1 < events.size()) len = std::min(len, events[i + 1].at - e.at);
      if (len == 0) continue;
      if (e.at > t) out.push_back({0, (uint16_t)(e.at - t)});
      out.push_back({hz(e.hz), (uint16_t)len, e.to ? hz(e.to) : (uint16_t)0});
      t = e.at + len;
    }
    return out;
  }

 private:
  struct Event {
    uint32_t at, len;
    float hz, to;
  };
  uint16_t hz(float f) const { return (uint16_t)lroundf(std::min(f * transpose_, 20000.0f)); }

  std::vector<Event> events_;
  float transpose_;
};

// The website's getMusicalVariation(): a pentatonic step up, never the same twice in a row.
static float random_key() {
  static const int STEPS[] = {0, 2, 4, 7, 9};
  static int last = -1;
  int step;
  do step = STEPS[hal_random() % 5];
  while (step == last);
  last = step;
  return powf(2, step / 12.0f);
}

static uint32_t random_below(uint32_t n) { return hal_random() % n; }

// Each: the website's notes and times, with what a buzzer can't do left out (noise, low thumps under
// the melody, reverb).

static Score hunt_success() {
  Score s(random_key());
  const float run[] = {C5, C5, C5, G5, C6, E6, G6, C7};
  for (int i = 0; i < 8; i++) s.note(i * 80, run[i], 400);
  return s.note(700, G6, 80).note(780, E6, 80).note(860, C6, 300);
}

static Score claim_success() {
  Score s(random_key());
  const struct { uint32_t at; float hz; } chime[] = {
      {0, C6},    {80, E6},   {160, G6},   {240, C7},   {400, E7},   {480, C7},   {560, A6},
      {600, E6},  {640, G6},  {720, C7},   {780, E7},   {840, G7},   {950, A7},   {1000, G7},
      {1050, E7}, {1100, C7}, {1150, A6},  {1200, G6},  {1300, C7},  {1350, E7},  {1400, G7}};
  for (const auto &n : chime) s.note(n.at, n.hz, 400);
  return s;
}

static Score claim_press() { return Score(random_key()).note(0, 185, 20).note(30, A5, 25).note(55, E6, 200); }

static Score cheese_success() {
  Score s;
  const struct { uint32_t at; float hz; } chime[] = {
      {0, A4},    {50, E5},   {100, A5},  {200, CS6}, {250, E6}, {300, A6}, {450, E6},
      {500, CS6}, {550, A5},  {600, E5},  {650, A4},  {750, A6}, {780, CS7}};
  for (const auto &n : chime) s.note(n.at, n.hz, 400);
  return s;
}

static Score cheese_squish() { return Score().glide(0, 800, 100, 15).glide(15, 220, 165, 90).note(105, 165, 250); }

static Score item_click() { return Score(random_key()).note(0, C7, 30); }

static Score hunt_loop() {
  static const int PATTERNS[4][8] = {{0, 1, 2, 3, 4, 3, 2, 1}, {0, 2, 0, 3, 0, 4, 0, 3}, {4, 3, 2, 1, 0, 1, 2, 3}, {0, 4, 1, 3, 2, 4, 1, 0}};
  const float scale[] = {C4, DS4, F4, G4, AS4};  // C minor pentatonic
  const int *pattern = PATTERNS[random_below(4)];
  Score s(random_key());
  for (int round = 0; round < 2; round++)
    for (int i = 0; i < 8; i++) s.note((round * 8 + i) * 150, scale[pattern[i]], 150);
  return s;
}

// Am, F, C, G, each spread upwards; an octave higher than the website's, where the buzzer sings.
static Score cheese_loop() {
  const struct { float root; bool minor; } chords[] = {{A3, true}, {F3, false}, {C3, false}, {G3, false}};
  const int MINOR[] = {0, 3, 7, 10, 12, 15}, MAJOR[] = {0, 4, 7, 11, 12, 16};
  Score s;
  for (int c = 0; c < 4; c++)
    for (int k = 0; k < 6; k++)
      s.note(c * 1000 + k * 120, chords[c].root * 2 * powf(2, (chords[c].minor ? MINOR : MAJOR)[k] / 12.0f), 300);
  return s;
}

static Score notification() { return Score().note(0, E6, 100).note(100, G6, 150); }

static Score poke() {
  Score s(random_key());
  s.glide(0, 200, 100, 40);
  const float run[] = {C5, E5, G5, A5, C6};
  for (int i = 0; i < 5; i++) s.note(40 + i * 40, run[i], 150);
  std::vector<float> sparkles = {C7, D7, E7, F7, G7, A7, C8};
  for (int i = 0; i < 4; i++) {  // 4 of them, in a random order
    size_t pick = i + random_below(sparkles.size() - i);
    std::swap(sparkles[i], sparkles[pick]);
    s.note(240 + i * 30, sparkles[i], 120);
  }
  return s;
}

static Score friendship() {
  Score s(random_key());
  s.note(0, C5, 30).note(30, E5, 30).note(60, G5, 140);
  const float scale[] = {C5, D5, E5, F5, G5, A5, C6};
  for (int i = 0; i < 7; i++) s.note(200 + i * 150, scale[i], 150);
  s.note(1250, C7, 100).note(1350, A6, 50);
  const float last[] = {C5, E5, G5, C6};
  for (int i = 0; i < 4; i++) s.note(1400 + i * 40, last[i], 500);
  return s;
}

static Score success() { return Score().note(0, C5, 50).note(50, E5, 50).note(100, G5, 50).note(150, C6, 400); }

static Score achievement() {
  Score s;
  const float up[] = {C5, E5, G5, C6}, chord[] = {C4, E4, G4, C5};
  for (int i = 0; i < 4; i++) s.note(i * 30, up[i], 60);
  for (int i = 0; i < 4; i++) s.note(150 + i * 25, chord[i], 25);
  return s.note(250, C7, 450);
}

static Score triumph() {
  Score s(random_key());
  const float groups[3][4] = {{C4, E4, G4, C5}, {F4, A4, C5, F5}, {C5, E5, G5, C6}};
  for (int g = 0; g < 3; g++)
    for (int i = 0; i < 4; i++) s.note(g * 400 + i * 80, groups[g][i], 600);
  const float end[] = {G5, E5, C5};
  for (int i = 0; i < 3; i++) s.note(1300 + i * 100, end[i], 500);
  return s;
}

static Score error() { return Score().note(0, 200, 100).note(120, 150, 80); }

static Score menu_open() {
  Score s;
  const float up[] = {C4, E4, G4, C5};
  for (int i = 0; i < 4; i++) s.note(i * 30, up[i], 120);
  return s.glide(150, 300, 2000, 80);
}

static Score menu_close() {
  Score s;
  const float down[] = {C5, G4, E4, C4};
  for (int i = 0; i < 4; i++) s.note(i * 20, down[i], 150);
  return s;
}

static Score click() { return Score(random_key()).note(0, C5, 40).note(40, G5, 110); }
static Score secondary_click() { return Score().glide(0, 650, 600, 30).note(30, 600, 50); }
static Score tick() { return Score().note(0, 1400 + random_below(200), 40); }
static Score hover() { return Score().note(0, 440, 150); }
static Score transition() { return Score().glide(0, 200, 800, 100).glide(100, 800, 400, 100); }

static Score mystery() {
  Score s;
  const float notes[] = {A2, B2, C3, D3};
  for (int i = 0; i < 4; i++) s.note(i * 150, notes[i], 650);
  return s;
}

// The last of the website's 5 slots (G5): its ticks climb a scale while slowing down, then a ding,
// then (last slot only) a C major run.
static Score roulette() {
  const float base = G5;
  const float ratios[] = {1, 1.125f, 1.25f, 1.333f, 1.5f, 1.667f, 1.875f, 2};
  Score s;
  float at = 0;
  for (int w = 0; w < 25; w++) {
    float e = w / 25.0f;
    s.note((uint32_t)at, base * ratios[(int)(e * 8)], (uint32_t)(30 + e * 50));
    float slow = e < 0.4f ? 0 : (e - 0.4f) / 0.6f;
    at += 50 + slow * slow * 200;
  }
  s.note(3000, base * 2, 300);
  const float run[] = {C6, E6, G6, C7};
  for (int i = 0; i < 4; i++) s.note(3200 + i * 80, run[i], 400);
  return s;
}

static Score cash_register() {
  Score s;
  s.note(0, C7, 80);
  const float jingle[] = {E6, G6, A6, F6, GS6};
  for (int i = 0; i < 5; i++) s.note(80 + i * 30, jingle[i], 100);
  return s;
}

static Score craft_success() {
  Score s;
  s.note(0, 1000, 40).glide(40, 220, 80, 40);
  const float up[] = {C5, E5, G5, C6};
  const uint32_t at[] = {80, 160, 240, 340};
  for (int i = 0; i < 4; i++) s.note(at[i], up[i], i == 3 ? 500 : 120);
  for (int m = 0; m < 8; m++) s.note(450 + m * 40, 2000 + m * 250 + random_below(100), 40);
  return s;
}

// A blip down, then two sighing slides.
static Score craft_fail() { return Score().glide(0, 300, 100, 80).glide(150, G4, DS4, 250).glide(400, DS4, GS3, 350); }

// A clank, then E major up and a high E.
static Score smash_success() {
  Score s;
  s.glide(0, 600, 200, 50).note(50, 1200, 30);
  const float up[] = {E4, GS4, B4, E5};
  const uint32_t at[] = {80, 160, 240, 340};
  for (int i = 0; i < 4; i++) s.note(at[i], up[i], 120);
  return s.note(480, E6, 350);
}

// A thud, then falling whistles.
static Score smash_fail() {
  return Score().glide(0, 120, 50, 50).glide(50, 1200, 800, 70).glide(120, 900, 400, 260).glide(380, 1400, 600, 120);
}

// A crack, a thud, splinters tinkling, then a sad A-G-F-D (the website's tinkles overlap the line:
// here they come first).
static Score hammer_break() {
  Score s;
  s.glide(0, 1200, 150, 40).glide(60, 180, 50, 60);
  for (int i = 0; i < 6; i++) s.note(120 + i * 70 + random_below(30), 1800 + random_below(1200), 60);
  const float sad[] = {A4, G4, F4, D4};
  for (int i = 0; i < 4; i++) s.note(560 + i * 200, sad[i], i == 3 ? 650 : 200);
  return s;
}

static const struct {
  const char *name;
  Score (*make)();
} SOUNDS[] = {
    {"HUNT SUCCESS", hunt_success},
    {"CLAIM SUCCESS", claim_success},
    {"CLAIM PRESS", claim_press},
    {"CHEESE SUCCESS", cheese_success},
    {"CHEESE SQUISH", cheese_squish},
    {"ITEM CLICK", item_click},
    {"HUNT LOOP", hunt_loop},
    {"CHEESE LOOP", cheese_loop},
    {"NOTIFICATION", notification},
    {"POKE", poke},
    {"FRIENDSHIP", friendship},
    {"SUCCESS", success},
    {"ACHIEVEMENT", achievement},
    {"TRIUMPH", triumph},
    {"ERROR", error},
    {"MENU OPEN", menu_open},
    {"MENU CLOSE", menu_close},
    {"CLICK", click},
    {"CLICK 2", secondary_click},
    {"TICK", tick},
    {"HOVER", hover},
    {"TRANSITION", transition},
    {"MYSTERY", mystery},
    {"ROULETTE", roulette},
    {"CASH REGISTER", cash_register},
    {"CRAFT SUCCESS", craft_success},
    {"CRAFT FAIL", craft_fail},
    {"SMASH SUCCESS", smash_success},
    {"SMASH FAIL", smash_fail},
    {"HAMMER BREAK", hammer_break},
};
static_assert(std::size(SOUNDS) == (size_t)Sound::Count, "one entry per Sound, in order");

const char *sound_name(Sound sound) { return SOUNDS[(size_t)sound].name; }

void play_sound(Sound sound) {
  std::vector<HalNote> notes = SOUNDS[(size_t)sound].make().notes();
  hal_play_tune(notes.data(), notes.size());
}

// The hidden picture's tune: ~2.6 s, in C major. Exact pitches, not the Score's float ones.
namespace picture_tune {
enum : uint16_t { G5 = 784, A5 = 880, B5 = 988, C6 = 1047, D6 = 1175, E6 = 1319, F6 = 1397, G6 = 1568 };
static constexpr uint16_t BEAT = 125;  // ms
static const HalNote NOTES[] = {
    {E6, BEAT}, {G6, BEAT}, {C6, BEAT}, {E6, 2 * BEAT}, {D6, BEAT}, {C6, BEAT}, {A5, 2 * BEAT},  // phrase 1
    {0, BEAT / 2},                                                                                // breath
    {F6, BEAT}, {A5, BEAT}, {C6, BEAT}, {D6, 2 * BEAT}, {C6, BEAT}, {G5, BEAT}, {B5, BEAT},      // phrase 2
    {C6, 4 * BEAT},                                                                               // home
};
}  // namespace picture_tune

void play_picture_tune() { hal_play_tune(picture_tune::NOTES, std::size(picture_tune::NOTES)); }
