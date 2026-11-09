#pragma once
// Options panel drawn inside the launcher (and later an in-game pause menu).
// Owns the display.* and input.* settings in knid::Config.

namespace knid {
class Config;
}

// Draws the options UI with Dear ImGui into the current window. Call between ImGui::NewFrame()
// and ImGui::Render().
void draw_options_panel(knid::Config& config);
