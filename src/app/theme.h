#pragma once
// KNID theme: modern UI with a retro touch in the pastel colors of the game's world. Original
// artwork only: cards, buttons, stars and icons are drawn from code. Fonts (SIL OFL 1.1, see
// assets/fonts): Fredoka for text and values, Pixelify Sans for titles, tabs and button labels.
// Small pixel icons are drawn at whole multiples of their pixel grid (ui_scale()) to stay crisp.

#include <imgui.h>

namespace knid::theme {

// Shared palette (the options panel uses these too).
struct Palette {
    ImVec4 night;         // plum: dark accents (#3B2A4A)
    ImVec4 night_light;   // frame / input background (light lavender)
    ImVec4 night_hover;   // frame hover
    ImVec4 pink;          // Kirby pink: primary buttons (#F7A8C4)
    ImVec4 pink_hover;
    ImVec4 pink_active;
    ImVec4 pink_soft;     // headers, selection
    ImVec4 yellow;        // star yellow: focus and highlights (#FFD95A)
    ImVec4 text;          // plum body text
    ImVec4 text_dim;      // secondary text
    ImVec4 text_on_pink;  // text on pink buttons
    ImVec4 good;          // ok
    ImVec4 bad;           // errors
    ImVec4 sky;           // sky blue: secondary accents (#9FD3F5)
    ImVec4 cream;         // card fill (#FFF8F0)
    ImVec4 shadow;        // soft shadow color
};
const Palette& palette();
inline ImU32 u32(const ImVec4& c, float alpha_mul = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alpha_mul));
}

// Loads the fonts and applies the style. density = window pixel density (1, 2, ...).
// Call once after ImGui::CreateContext() and before the first NewFrame().
void apply(float density);
// Picks the pixel-icon scale for this window size. Call before every ImGui::NewFrame().
void update_scale(ImVec2 display_size);
int ui_scale();  // points per icon pixel (2 or 3)

ImFont* font_body();     // Fredoka ~20 px: text, descriptions, values (the default font)
ImFont* font_small();    // Fredoka ~16 px: secondary text
ImFont* font_title();    // Pixelify Sans SemiBold ~44 px: screen titles
ImFont* font_display();  // Pixelify Sans Medium ~22 px: button labels, tab names, headings
ImFont* font_button();   // same as font_display()

enum class Icon { None, Star, Play, Continue, Gear, Folder, Shortcut, Quit, Check };

// Rounded card with a soft layered shadow and a small pixel-stepped highlight along the top.
void card(ImDrawList* dl, ImVec2 min, ImVec2 max);
// 7x7-pixel icon, top-left at pos, drawn at ui_scale() points per pixel.
void pixel_icon(ImDrawList* dl, ImVec2 pos, Icon icon, ImU32 color);
// Rounded pill button with a darker bottom lip. Hover/press change color only (eased); the layout
// box never moves. Keyboard/gamepad focus shows a soft yellow glow and a blinking pixel star to the
// left. size.x <= 0 takes the available width; size.y <= 0 uses the default height.
bool pixel_button(const char* label, ImVec2 size = ImVec2(0, 0), bool primary = false, Icon icon = Icon::None);
// A window drawn as a card (no title bar). End with pixel_window_end().
bool pixel_window_begin(const char* name, ImVec2 size, bool* open = nullptr, ImGuiWindowFlags flags = 0);
void pixel_window_end();
// Blinking pixel star cursor centered at `center` (used for focus).
void focus_star(ImDrawList* dl, ImVec2 center);

// Pastel dusk sky with slowly twinkling and drifting pixel stars, filling [min, max]. t = seconds.
void draw_starfield(ImDrawList* dl, ImVec2 min, ImVec2 max, double t);
// Pixel star centered at center, about `radius` points half-width.
void draw_star(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float rotation = 0.0f,
               bool filled = true);
// Kept for existing callers: same as pixel_button (primary = pink).
bool big_button(const char* label, ImVec2 size, bool primary = true);
// Text centered horizontally in the current window.
void centered_text(const char* text, ImFont* font = nullptr, ImVec4 color = ImVec4(0, 0, 0, 0));

}  // namespace knid::theme
