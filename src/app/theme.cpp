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

// Small deterministic hash for star placement.
float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return float(x & 0xFFFFFF) / float(0xFFFFFF);
}
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

void draw_star(ImDrawList* dl, ImVec2 c, float r, ImU32 color, float rot, bool filled) {
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i) {
        float a = rot - IM_PI / 2 + float(i) * IM_PI / 5;
        float rr = (i & 1) ? r * 0.45f : r;
        pts[i] = ImVec2(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr);
    }
    if (filled) {
        // Fan from the center keeps the concave shape correct.
        for (int i = 0; i < 10; ++i) dl->AddTriangleFilled(c, pts[i], pts[(i + 1) % 10], color);
    } else {
        dl->AddPolyline(pts, 10, color, ImDrawFlags_Closed, std::max(1.5f, r * 0.12f));
    }
}

void draw_starfield(ImDrawList* dl, ImVec2 mn, ImVec2 mx, double t) {
    const Palette& c = palette();
    ImU32 top = u32(c.night), bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f, 0.16f, 0.42f, 1.0f));
    dl->AddRectFilledMultiColor(mn, mx, top, top, bottom, bottom);
    float w = mx.x - mn.x, h = mx.y - mn.y;
    // Soft glowing "moon" haze.
    ImVec2 glow(mn.x + w * 0.82f, mn.y + h * 0.18f);
    for (int i = 24; i >= 1; --i)
        dl->AddCircleFilled(glow, h * 0.013f * float(i), u32(c.pink, 0.012f), 64);
    // Twinkling dots.
    int dots = int(w * h / 3500.0f);
    for (int i = 0; i < dots; ++i) {
        float x = mn.x + hash01(std::uint32_t(i) * 3 + 1) * w;
        float y = mn.y + hash01(std::uint32_t(i) * 3 + 2) * h;
        float phase = hash01(std::uint32_t(i) * 3 + 3) * 6.2831f;
        float tw = 0.45f + 0.55f * float(0.5 + 0.5 * std::sin(t * (0.8 + phase * 0.3) + phase));
        float size = 0.8f + hash01(std::uint32_t(i) * 7 + 5) * 1.6f;
        dl->AddCircleFilled(ImVec2(x, y), size, u32(c.text, tw * 0.8f), 8);
    }
    // A few big drifting stars.
    for (int i = 0; i < 7; ++i) {
        float bx = hash01(std::uint32_t(i) * 11 + 101), by = hash01(std::uint32_t(i) * 11 + 102);
        float speed = 4.0f + hash01(std::uint32_t(i) * 11 + 103) * 8.0f;
        float x = mn.x + std::fmod(bx * w + float(t) * speed, w + 60.0f) - 30.0f;
        float y = mn.y + by * h + std::sin(float(t) * 0.7f + float(i)) * 6.0f;
        float r = 6.0f + hash01(std::uint32_t(i) * 11 + 104) * 9.0f;
        float a = 0.35f + 0.35f * float(0.5 + 0.5 * std::sin(t * 1.3 + i));
        draw_star(dl, ImVec2(x, y), r, u32(c.yellow, a), float(t) * 0.3f + float(i));
    }
    // A shooting star every few seconds.
    double cycle = std::fmod(t, 7.0);
    if (cycle < 0.9) {
        float k = float(cycle / 0.9);
        ImVec2 a(mn.x + w * (0.15f + 0.5f * k), mn.y + h * (0.1f + 0.25f * k));
        ImVec2 b(a.x - w * 0.08f, a.y - h * 0.04f);
        dl->AddLine(a, b, u32(c.text, 0.6f * (1 - k)), 2.0f);
        draw_star(dl, a, 5.0f, u32(c.yellow, 1 - k), float(t) * 4);
    }
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
    float lift = 0.0f;  // feedback is color and the star only; buttons never move
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
    if (anim > 0.01f) {
        draw_star(dl, ImVec2(tp.x - 18, a.y + size.y * 0.5f), 8.0f * e, u32(c.yellow, e),
                  float(ImGui::GetTime()) * 2.0f);
    }
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
