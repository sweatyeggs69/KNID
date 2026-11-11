// KNID's colors for the shared launcher theme (theme.h): purple night surfaces, Kirby pink accent
// and a warm yellow focus ring. This is the only theme file that differs between ports.
#include "app/theme.h"

namespace knid::theme {

namespace {
ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
}  // namespace

const Palette& palette() {
    static const Palette p{
        rgb(27, 20, 56),            // surface
        rgb(34, 26, 69),            // card
        rgb(44, 35, 86),            // raised
        rgb(56, 46, 105),           // high
        rgb(190, 170, 240, 0.16f),  // line
        rgb(255, 247, 240),         // ink
        rgb(196, 184, 230),         // ink_muted
        rgb(138, 127, 176),         // ink_faint
        rgb(255, 143, 186),         // accent
        rgb(255, 170, 205),         // accent_hover
        rgb(238, 112, 160),         // accent_active
        rgb(255, 143, 186, 0.15f),  // accent_soft
        rgb(58, 20, 46),            // on_accent
        rgb(255, 214, 92),          // highlight
        rgb(52, 211, 153),          // good
        rgb(248, 113, 113),         // bad
        rgb(120, 80, 200),          // glow
    };
    return p;
}

}  // namespace knid::theme
