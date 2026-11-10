#include "app/theme.h"

#include <imgui_internal.h>

#include <cmath>
#include <cstdint>
#include <unordered_map>

extern const unsigned char knid_font_fredoka_Medium[];
extern const unsigned knid_font_fredoka_Medium_size;
extern const unsigned char knid_font_fredoka_SemiBold[];
extern const unsigned knid_font_fredoka_SemiBold_size;

namespace knid::theme {

namespace {
ImFont* g_body = nullptr;
ImFont* g_title = nullptr;
ImFont* g_small = nullptr;
ImFont* g_button = nullptr;

ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }

}  // namespace

const Palette& palette() {
    static const Palette p{
        rgb(27, 20, 56),      // night
        rgb(46, 36, 92),      // night_light
        rgb(64, 51, 122),     // night_hover
        rgb(255, 143, 186),   // pink
        rgb(255, 170, 205),   // pink_hover
        rgb(238, 112, 160),   // pink_active
        rgb(255, 143, 186, 0.35f),
        rgb(255, 214, 92),    // yellow
        rgb(255, 247, 240),   // text
        rgb(196, 184, 230),   // text_dim
        rgb(58, 20, 46),      // text_on_pink
        rgb(140, 230, 170),   // good
        rgb(255, 120, 120),   // bad
    };
    return p;
}

void apply(float ui_scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    auto* medium = const_cast<unsigned char*>(knid_font_fredoka_Medium);
    auto* semibold = const_cast<unsigned char*>(knid_font_fredoka_SemiBold);
    int medium_size = int(knid_font_fredoka_Medium_size), semibold_size = int(knid_font_fredoka_SemiBold_size);
    // Fonts are rasterized at the pixel density and scaled back, so text stays sharp on HiDPI.
    float px = ui_scale > 0 ? ui_scale : 1.0f;
    g_body = io.Fonts->AddFontFromMemoryTTF(medium, medium_size, 21.0f * px, &cfg);
    g_title = io.Fonts->AddFontFromMemoryTTF(semibold, semibold_size, 48.0f * px, &cfg);
    g_small = io.Fonts->AddFontFromMemoryTTF(medium, medium_size, 16.0f * px, &cfg);
    g_button = io.Fonts->AddFontFromMemoryTTF(semibold, semibold_size, 23.0f * px, &cfg);
    io.FontDefault = g_body;
    io.FontGlobalScale = 1.0f / px;

    const Palette& c = palette();
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowRounding = 22.0f;
    s.ChildRounding = 18.0f;
    s.FrameRounding = 14.0f;
    s.PopupRounding = 14.0f;
    s.GrabRounding = 12.0f;
    s.ScrollbarRounding = 12.0f;
    s.TabRounding = 12.0f;
    s.WindowPadding = ImVec2(26, 22);
    s.FramePadding = ImVec2(14, 9);
    s.ItemSpacing = ImVec2(12, 12);
    s.ItemInnerSpacing = ImVec2(10, 8);
    s.ScrollbarSize = 16.0f;
    s.GrabMinSize = 18.0f;
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 0.0f;
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = c.text;
    col[ImGuiCol_TextDisabled] = c.text_dim;
    col[ImGuiCol_WindowBg] = ImVec4(c.night.x, c.night.y, c.night.z, 0.92f);
    col[ImGuiCol_ChildBg] = ImVec4(c.night_light.x, c.night_light.y, c.night_light.z, 0.55f);
    col[ImGuiCol_PopupBg] = ImVec4(c.night_light.x, c.night_light.y, c.night_light.z, 0.98f);
    col[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.08f);
    col[ImGuiCol_FrameBg] = c.night_light;
    col[ImGuiCol_FrameBgHovered] = c.night_hover;
    col[ImGuiCol_FrameBgActive] = c.night_hover;
    col[ImGuiCol_TitleBg] = c.night;
    col[ImGuiCol_TitleBgActive] = c.night_light;
    col[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0.1f);
    col[ImGuiCol_ScrollbarGrab] = c.night_hover;
    col[ImGuiCol_ScrollbarGrabHovered] = c.pink_active;
    col[ImGuiCol_ScrollbarGrabActive] = c.pink;
    col[ImGuiCol_CheckMark] = c.yellow;
    col[ImGuiCol_SliderGrab] = c.pink;
    col[ImGuiCol_SliderGrabActive] = c.pink_hover;
    col[ImGuiCol_Button] = c.pink;
    col[ImGuiCol_ButtonHovered] = c.pink_hover;
    col[ImGuiCol_ButtonActive] = c.pink_active;
    col[ImGuiCol_Header] = c.pink_soft;
    col[ImGuiCol_HeaderHovered] = ImVec4(c.pink.x, c.pink.y, c.pink.z, 0.5f);
    col[ImGuiCol_HeaderActive] = ImVec4(c.pink.x, c.pink.y, c.pink.z, 0.7f);
    col[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.12f);
    col[ImGuiCol_Tab] = c.night_light;
    col[ImGuiCol_TabHovered] = c.pink_active;
    col[ImGuiCol_TabSelected] = c.pink_active;
    col[ImGuiCol_NavCursor] = c.yellow;
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.03f, 0.12f, 0.6f);
}

ImFont* font_body() { return g_body; }
ImFont* font_title() { return g_title; }
ImFont* font_small() { return g_small; }
ImFont* font_button() { return g_button; }

void draw_backdrop(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    const Palette& c = palette();
    ImU32 top = u32(c.night), bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f, 0.16f, 0.42f, 1.0f));
    dl->AddRectFilledMultiColor(mn, mx, top, top, bottom, bottom);
    float w = mx.x - mn.x, h = mx.y - mn.y;
    // Soft glowing "moon" haze.
    ImVec2 glow(mn.x + w * 0.82f, mn.y + h * 0.18f);
    for (int i = 24; i >= 1; --i)
        dl->AddCircleFilled(glow, h * 0.013f * float(i), u32(c.pink, 0.012f), 64);
}

bool big_button(const char* label, ImVec2 size, bool primary) {
    const Palette& c = palette();
    ImGuiID id = ImGui::GetID(label);
    static std::unordered_map<ImGuiID, float> hover_anim;
    float& anim = hover_anim[id];

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    bool held = ImGui::IsItemActive();
    float dt = ImGui::GetIO().DeltaTime;
    anim += (hovered ? 1.0f : -1.0f) * dt * 8.0f;
    anim = anim < 0 ? 0 : anim > 1 ? 1 : anim;

    // Bounce: lift up a few pixels with an overshoot ease.
    float e = anim * anim * (3 - 2 * anim);
    float lift = 0.0f;  // feedback is color and the outline only; buttons never move
    ImVec2 a(pos.x, pos.y - lift), b(pos.x + size.x, pos.y + size.y - lift);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = size.y * 0.5f;
    dl->AddRectFilled(ImVec2(a.x, a.y + 5 + lift), ImVec2(b.x, b.y + 5 + lift), u32(ImVec4(0, 0, 0, 1), 0.25f),
                      rounding);
    ImVec4 base = primary ? (held ? c.pink_active : hovered ? c.pink_hover : c.pink)
                          : (held ? c.night_light : hovered ? c.night_hover : c.night_light);
    dl->AddRectFilled(a, b, u32(base), rounding);
    dl->AddRectFilled(ImVec2(a.x + 6, a.y + 3), ImVec2(b.x - 6, a.y + size.y * 0.45f),
                      u32(ImVec4(1, 1, 1, 1), primary ? 0.18f : 0.06f), rounding);
    if (hovered) dl->AddRect(a, b, u32(c.yellow, 0.9f * e), rounding, 0, 2.5f);

    ImGui::PushFont(g_button);
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 ts = ImGui::CalcTextSize(label, end);
    ImVec2 tp(a.x + (size.x - ts.x) * 0.5f, a.y + (size.y - ts.y) * 0.5f);
    dl->AddText(tp, u32(primary ? c.text_on_pink : c.text), label, end);
    ImGui::PopFont();
    return pressed;
}

void centered_text(const char* text, ImFont* font, ImVec4 color) {
    if (font) ImGui::PushFont(font);
    float w = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - w) * 0.5f);
    if (color.w > 0) ImGui::TextColored(color, "%s", text);
    else ImGui::TextUnformatted(text);
    if (font) ImGui::PopFont();
}

}  // namespace knid::theme
