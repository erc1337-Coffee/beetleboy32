#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/screen.h"

// Title, body and up to a few action buttons. Used for errors and confirmations.
class MessageScreen : public Screen {
 public:
  struct Action {
    std::string label;
    std::function<void()> run;
  };

  // Plain: the default screen look. Blue: the RWO blue screen with VHS text (startup and Wi-Fi).
  enum class Look { Plain, Blue };

  // `name` identifies the instance (logs, "is this already showing?" checks).
  MessageScreen(App &app, const char *name, std::string title, std::string body,
                std::vector<Action> actions, std::function<void()> on_back = nullptr, Look look = Look::Plain)
      : Screen(app),
        look_(look),
        name_(name),
        title_(std::move(title)),
        body_(std::move(body)),
        actions_(std::move(actions)),
        onBack_(std::move(on_back)) {}

  const char *name() const override { return name_; }
  bool onInput(const InputEvent &event) override;

 protected:
  void build(lv_obj_t *root) override;

 private:
  static void onClick(lv_event_t *e);

  Look look_;
  const char *name_;
  std::string title_;
  std::string body_;
  std::vector<Action> actions_;  // not resized after build(): buttons hold pointers into it
  std::function<void()> onBack_;
};
