#include "input/controller.h"

#include "hal/hal.h"

static constexpr uint32_t LONG_PRESS_MS = 3000;   // Cancel held this long = Reset
static constexpr uint32_t TOUCH_HOLD_MS = 5000;  // the screen held this long = Hold
static constexpr uint32_t WAKE_SWALLOW_MS = 150;  // swallowInput() also ignores presses starting this soon after

// A widget is anything in a focus group (or inside one): it handles its own clicks.
static bool is_widget(lv_obj_t *obj) {
  for (; obj; obj = lv_obj_get_parent(obj))
    if (lv_obj_get_group(obj)) return true;
  return false;
}

void Controller::begin(Handler handler) {
  handler_ = std::move(handler);
  buttons_begin();

  keypad_ = lv_indev_create();
  lv_indev_set_type(keypad_, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(keypad_, readKeypad);
  lv_indev_set_user_data(keypad_, this);

  // LVGL reports every click/long-press/gesture to the input device before the target object.
  for (lv_indev_t *i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i)) {
    if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) {
      touch_ = i;
      lv_indev_add_event_cb(touch_, onTouch, LV_EVENT_ALL, this);
      break;
    }
  }
}

void Controller::swallowInput() {
  if (touch_) lv_indev_wait_release(touch_);
  swallowed_ |= held_;
  held_ = 0;
  swallowUntil_ = hal_millis() + WAKE_SWALLOW_MS;
}

void Controller::update() {
  const uint8_t raw = buttons_read();
  const uint32_t now = hal_millis();
  swallowed_ &= raw;  // released: counts again
  if (dark_ || (int32_t)(now - swallowUntil_) < 0) {
    if (dark_ && (raw & ~swallowed_)) darkPress_ = true;
    swallowed_ |= raw;
  }
  uint8_t prev = held_;
  held_ = raw & ~swallowed_;
  uint8_t pressed = held_ & ~prev, released = prev & ~held_;
  // Buttons count as activity like touches do (the screen-off and sleep timers read LVGL's).
  if (pressed) lv_display_trigger_activity(nullptr);

  // Cancel: short press = Back (on release), held = Reset (once).
  if (pressed & BTN_CANCEL) {
    cancelDownAt_ = now;
    cancelWasLong_ = false;
  } else if ((held_ & BTN_CANCEL) && !cancelWasLong_ && now - cancelDownAt_ >= LONG_PRESS_MS) {
    cancelWasLong_ = true;
    emitButton(BTN_CANCEL, InputAction::Reset);
  }
  if ((released & BTN_CANCEL) && !cancelWasLong_) emitButton(BTN_CANCEL, InputAction::Back);

  // Touch held for TOUCH_HOLD_MS, wherever (even on a widget or while dragging): Hold, once.
  bool down = touch_ && lv_indev_get_state(touch_) == LV_INDEV_STATE_PRESSED;
  if (down && !touchDown_) touchDownAt_ = now;
  if (!down) holdFired_ = false;
  touchDown_ = down;
  if (down && !holdFired_ && now - touchDownAt_ >= TOUCH_HOLD_MS) {
    holdFired_ = true;
    lv_indev_wait_release(touch_);  // the finger's eventual release must not click what it was on
    InputEvent event{InputSource::Touch, InputAction::Hold};
    lv_indev_get_point(touch_, &event.point);
    emit(event);
  }

  // OK / Next drive the focused widget through the keypad (readKeypad); with no widgets on
  // screen there is nothing to focus, so they become screen events.
  if (!screenHasWidgets()) {
    if (pressed & BTN_OK) emitButton(BTN_OK, InputAction::Confirm);
    if (pressed & BTN_NEXT) emitButton(BTN_NEXT, InputAction::Next);
  }
}

bool Controller::screenHasWidgets() const {
  lv_group_t *group = lv_indev_get_group(keypad_);
  return group && lv_group_get_obj_count(group) > 0;
}

void Controller::emit(const InputEvent &event) {
  if (handler_) handler_(event);
}

void Controller::emitButton(uint8_t button, InputAction action) {
  InputEvent event{InputSource::Button, action};
  event.button = button;
  emit(event);
}

void Controller::readKeypad(lv_indev_t *indev, lv_indev_data_t *data) {
  auto *self = static_cast<Controller *>(lv_indev_get_user_data(indev));
  data->key = self->lastKey_;
  data->state = LV_INDEV_STATE_RELEASED;
  if (!self->screenHasWidgets()) return;
  if (self->held_ & BTN_OK) {
    self->lastKey_ = LV_KEY_ENTER;
  } else if (self->held_ & BTN_NEXT) {
    self->lastKey_ = LV_KEY_NEXT;
  } else {
    return;
  }
  data->key = self->lastKey_;
  data->state = LV_INDEV_STATE_PRESSED;
}

void Controller::onTouch(lv_event_t *e) {
  auto *self = static_cast<Controller *>(lv_event_get_user_data(e));
  auto *target = static_cast<lv_obj_t *>(lv_event_get_param(e));
  // Overlays on LVGL's top layer (the transition curtain, the hidden picture) handle their own.
  if (target && lv_obj_get_screen(target) != lv_screen_active()) return;
  InputEvent event{InputSource::Touch, InputAction::Tap};

  switch (lv_event_get_code(e)) {
    case LV_EVENT_SHORT_CLICKED:  // not sent after a long press, unlike CLICKED
      if (is_widget(target)) return;
      event.action = InputAction::Tap;
      break;
    case LV_EVENT_LONG_PRESSED:
      if (is_widget(target)) return;
      event.action = InputAction::LongPress;
      break;
    case LV_EVENT_GESTURE:
      switch (lv_indev_get_gesture_dir(self->touch_)) {
        case LV_DIR_LEFT: event.action = InputAction::SwipeLeft; break;
        case LV_DIR_RIGHT: event.action = InputAction::SwipeRight; break;
        case LV_DIR_TOP: event.action = InputAction::SwipeUp; break;
        case LV_DIR_BOTTOM: event.action = InputAction::SwipeDown; break;
        default: return;
      }
      break;
    default:
      return;
  }
  lv_indev_get_point(self->touch_, &event.point);
  self->emit(event);
}
