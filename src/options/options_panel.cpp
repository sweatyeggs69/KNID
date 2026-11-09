#include "options/options_panel.h"

#include <imgui.h>

#include "app/config.h"

// Stub: the options thread replaces this body (video, audio, controls, fast-forward, pause).
void draw_options_panel(knid::Config& config) {
    (void)config;
    ImGui::TextWrapped("Options are coming soon.");
}
