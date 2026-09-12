#pragma once

#include <string>

namespace hud {

// The whole JavaScript expression that installs the old HUD. Evaluating it twice is harmless:
// it tears down anything it left behind first.
std::string buildInstallScript();

// Cheap liveness check for the supervisor. Returns true while the HUD is installed, and
// re-attaches the root if something removed it from the page.
std::string buildProbeScript();

}  // namespace hud
