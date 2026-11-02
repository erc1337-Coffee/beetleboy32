#pragma once

#include <string>

#include "ui/screen.h"

// Loading animation + one line of text, for anything the user just waits on.
class BusyScreen : public Screen {
 public:
  BusyScreen(App &app, std::string text) : Screen(app), text_(std::move(text)) {}
  const char *name() const override { return "busy"; }
  // Back is swallowed: there's nothing to go back to.
  bool onInput(const InputEvent &e) override { return e.action == InputAction::Back; }

 protected:
  void build(lv_obj_t *root) override;

 private:
  std::string text_;
};
