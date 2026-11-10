#pragma once
// KNID launcher theme: dreamy night sky, soft pink and warm yellow. Original artwork only:
// everything is drawn procedurally; the font is Fredoka (SIL OFL 1.1, see assets/fonts/OFL.txt).

#include <imgui.h>

namespace knid::theme {

// Shared palette so other panels (options, pause menu) match the launcher.
struct Palette {
    ImVec4 night;        // window background, deep purple-blue
    ImVec4 night_light;  // panels / frames
    ImVec4 night_hover;
    ImVec4 pink;         // primary buttons, accents
    ImVec4 pink_hover;
    ImVec4 pink_active;
    ImVec4 pink_soft;    // headers, selection
    ImVec4 yellow;       // highlights, focus
    ImVec4 text;         // warm white
    ImVec4 text_dim;
    ImVec4 text_on_pink;  // dark text for pink buttons
    ImVec4 good;         // verified / ok
    ImVec4 bad;          // errors
};
const Palette& palette();
inline ImU32 u32(const ImVec4& c, float alpha_mul = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alpha_mul));
}

// Loads the fonts and applies the style. ui_scale is the display content scale (1.0, 2.0, ...).
// Call once after ImGui::CreateContext() and before the first NewFrame().
void apply(float ui_scale);

ImFont* font_body();   // ~20 px
ImFont* font_title();  // ~44 px
ImFont* font_small();  // ~16 px
ImFont* font_button(); // ~23 px semibold

// Soft night gradient with a pink haze, filling [min, max].
void draw_backdrop(ImDrawList* dl, ImVec2 min, ImVec2 max);
// Large rounded button that lights up and gains an outline on hover.
// primary = pink, otherwise night-light.
bool big_button(const char* label, ImVec2 size, bool primary = true);
// Text centered horizontally in the current window.
void centered_text(const char* text, ImFont* font = nullptr, ImVec4 color = ImVec4(0, 0, 0, 0));

}  // namespace knid::theme
