#include <bb3/psu/psu.h>
#include <bb3/psu/persist_conf.h>

namespace eez {
namespace gui {
namespace display {

uint8_t getDisplayBackgroundLuminosityStepHook() {
	return psu::persist_conf::devConf.displayBackgroundLuminosityStep;
}

uint8_t getSelectedThemeIndexHook() {
	return psu::persist_conf::devConf.selectedThemeIndex;
}

} // namespace display
} // namespace gui
} // namespace eez
