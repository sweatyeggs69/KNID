#pragma once
// Options panel drawn inside the launcher (and later an in-game pause menu).
// Owns the display.* and input.* settings in knid::Config.
#include <SDL3/SDL.h>

#include "options/settings_io.h"

namespace knid {
class Config;

// Adapts Config to the get/set/save interface the display and input modules use.
SettingsIO settings_io(Config& config);
}  // namespace knid

// Draws the options UI with Dear ImGui into the current window. Call between ImGui::NewFrame()
// and ImGui::Render().
void draw_options_panel(knid::Config& config);

// Forward every SDL event here while the options panel is open. Needed for gamepad hotplug
// and for capturing the next key or button when rebinding a control. Returns true when the
// event was consumed by a rebind (do not pass it on to other UI).
bool options_panel_handle_event(const SDL_Event& event);
