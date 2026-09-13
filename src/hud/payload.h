#pragma once

#include <string>

namespace hud {

// The whole JavaScript expression that installs the old HUD. Evaluating it twice is harmless:
// it tears down anything it left behind first.
std::string buildInstallScript();

// Cheap liveness check for the supervisor. Evaluates to "" when the HUD is gone, "ok" while it
// is installed (re-attaching the root if something removed it from the page), and "ok" followed
// by a newline and a layout when the game's layout editor changed where the old HUD sits.
std::string buildProbeScript();

}  // namespace hud
