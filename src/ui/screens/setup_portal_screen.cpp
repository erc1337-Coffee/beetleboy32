#include "ui/screens/setup_portal_screen.h"

#include "ui/theme.h"
#include "ui/widgets.h"

static constexpr int32_t QR_SIZE = 120;   // leaves the text column ~12 characters
static constexpr int32_t TEXT_W = 116;

void SetupPortalScreen::build(lv_obj_t *root) {
  // Shown without the scheme: shorter, and browsers add it themselves.
  std::string host = url_.substr(url_.find("://") == std::string::npos ? 0 : url_.find("://") + 3);

  theme::style_blue_screen(root);
  UiSplit split = ui_split(root);
  ui_qr(split.left, url_.c_str(), QR_SIZE);
  ui_vhs_text(split.right, "SETUP PAGE", TEXT_W);
  ui_vhs_text(split.right, "NOT OPEN? SCAN, OR GO TO", TEXT_W);
  ui_vhs_text(split.right, host.c_str(), TEXT_W);
  addBackButton(root, BackLook::Text);  // back to the choice: setup on the device instead
}
