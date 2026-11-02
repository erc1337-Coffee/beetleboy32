#include "ui/screen_manager.h"

#include "hal/hal.h"

static constexpr uint32_t FADE_MS = 160;  // each way

void ScreenManager::push(std::unique_ptr<Screen> s) { pending_.push_back({Op::Push, std::move(s)}); }
void ScreenManager::replace(std::unique_ptr<Screen> s) { pending_.push_back({Op::Replace, std::move(s)}); }
void ScreenManager::reset(std::unique_ptr<Screen> s, Transition t) {
  pending_.push_back({Op::Reset, std::move(s), t});
}
void ScreenManager::pop() { pending_.push_back({Op::Pop, nullptr}); }
void ScreenManager::rebuildHidden() {
  for (size_t i = 0; i + 1 < stack_.size(); i++) stack_[i]->destroyView();
}

bool ScreenManager::dispatch(const InputEvent &event) {
  if (fade_ != Fade::None) return true;  // swallowed: nothing reacts mid-transition
  return top() && top()->onInput(event);
}

void ScreenManager::update() {
  stepFade();
  // In order; ops may queue more ops (e.g. onShow navigating). A fade holds the rest until it's done.
  while (!pending_.empty() && fade_ == Fade::None) {
    Pending p = std::move(pending_.front());
    pending_.erase(pending_.begin());
    if (p.op == Op::Pop && stack_.size() <= 1) continue;  // nothing to close: no fade either
    if (p.transition == Transition::Cut) {
      apply(p);
    } else {
      startFade(std::move(p));
    }
  }
  if (Screen *s = top()) s->update();
}

void ScreenManager::startFade(Pending p) {
  if (!curtain_) {
    curtain_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(curtain_);
    lv_obj_set_size(curtain_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(curtain_, lv_color_black(), 0);
    lv_obj_add_flag(curtain_, LV_OBJ_FLAG_CLICKABLE);  // touches stop here
  }
  lv_obj_set_style_bg_opa(curtain_, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(curtain_, LV_OBJ_FLAG_HIDDEN);
  lv_indev_set_group(keypad_, nullptr);  // buttons too; showTop() gives them the new screen
  fading_ = std::move(p);
  fade_ = Fade::Out;
  fadeStart_ = hal_millis();
}

void ScreenManager::stepFade() {
  if (fade_ == Fade::None) return;
  uint32_t elapsed = hal_millis() - fadeStart_;
  lv_opa_t opa = elapsed >= FADE_MS ? LV_OPA_COVER : (lv_opa_t)(elapsed * LV_OPA_COVER / FADE_MS);
  lv_obj_set_style_bg_opa(curtain_, fade_ == Fade::Out ? opa : LV_OPA_COVER - opa, 0);
  if (elapsed < FADE_MS) return;

  if (fade_ == Fade::Out) {
    apply(fading_);
    fade_ = Fade::In;
    fadeStart_ = hal_millis();  // after the new screen's build, so the fade-in isn't cut short
  } else {
    lv_obj_add_flag(curtain_, LV_OBJ_FLAG_HIDDEN);
    fade_ = Fade::None;
  }
}

void ScreenManager::apply(Pending &p) {
  // Screens leaving the stack are kept alive until the next one is loaded:
  // LVGL must never be left without an active screen.
  std::vector<std::unique_ptr<Screen>> closing;

  switch (p.op) {
    case Op::Pop:
      if (stack_.size() <= 1) return;
      closing.push_back(std::move(stack_.back()));
      stack_.pop_back();
      break;
    case Op::Replace:
      if (!stack_.empty()) {
        closing.push_back(std::move(stack_.back()));
        stack_.pop_back();
      }
      stack_.push_back(std::move(p.screen));
      break;
    case Op::Reset:
      for (auto &s : stack_) closing.push_back(std::move(s));
      stack_.clear();
      stack_.push_back(std::move(p.screen));
      break;
    case Op::Push:
      stack_.push_back(std::move(p.screen));
      break;
  }
  showTop();
  for (auto &s : closing) hal_log("[ui] close %s\n", s->name());
}

void ScreenManager::showTop() {
  Screen *s = top();
  if (!s) return;
  if (!s->root_) s->create();
  lv_screen_load(s->root_);
  lv_indev_set_group(keypad_, s->group_);
  hal_log("[ui] show %s (depth %u)\n", s->name(), (unsigned)stack_.size());
  s->onShow();
}
