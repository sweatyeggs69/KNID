#include "app/theme.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>

extern const unsigned char knid_font_Fredoka_Medium[];
extern const unsigned knid_font_Fredoka_Medium_size;
extern const unsigned char knid_font_PixelifySans_Medium[];
extern const unsigned knid_font_PixelifySans_Medium_size;
extern const unsigned char knid_font_PixelifySans_SemiBold[];
extern const unsigned knid_font_PixelifySans_SemiBold_size;

namespace knid::theme {

namespace {
ImFont* g_body = nullptr;
ImFont* g_small = nullptr;
ImFont* g_display = nullptr;
ImFont* g_title = nullptr;
int g_scale = 2;
float g_density = 1.0f;

ImVec4 hex(std::uint32_t rgb, float a = 1.0f) {
    return ImVec4(float((rgb >> 16) & 255) / 255.0f, float((rgb >> 8) & 255) / 255.0f, float(rgb & 255) / 255.0f, a);
}

float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return float(x & 0xFFFFFF) / float(0xFFFFFF);
}

// 7x7 icon bitmaps ('#' = pixel). Original, simple shapes.
const char* icon_rows(Icon i) {
    switch (i) {
        case Icon::Star:
            return "...#..."
                   "...#..."
                   "#######"
                   ".#####."
                   "..###.."
                   ".##.##."
                   "#.....#";
        case Icon::Play:
            return ".#....."
                   ".##...."
                   ".###..."
                   ".####.."
                   ".###..."
                   ".##...."
                   ".#.....";
        case Icon::Continue:
            return "#..#..."
                   "##.##.."
                   "######."
                   "#######"
                   "######."
                   "##.##.."
                   "#..#...";
        case Icon::Gear:
            return "..#.#.."
                   ".#####."
                   "###.###"
                   "##...##"
                   "###.###"
                   ".#####."
                   "..#.#..";
        case Icon::Folder:
            return "###...."
                   "#######"
                   "#.....#"
                   "#.....#"
                   "#.....#"
                   "#######"
                   ".......";
        case Icon::Shortcut:
            return "...####"
                   ".....##"
                   "....#.#"
                   "#..#..#"
                   "#.#...."
                   "#......"
                   "#####..";
        case Icon::Quit:
            return "#.....#"
                   ".#...#."
                   "..#.#.."
                   "...#..."
                   "..#.#.."
                   ".#...#."
                   "#.....#";
        case Icon::Check:
            return "......#"
                   ".....##"
                   "#...##."
                   "##.##.."
                   ".###..."
                   "..#...."
                   ".......";
        default: return nullptr;
    }
}

ImVec2 snap(ImVec2 p) { return ImVec2(std::floor(p.x), std::floor(p.y)); }

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

// Eased 0..1 animation per widget (hover/press feedback).
float ease(ImGuiID id, int channel, bool on) {
    static std::unordered_map<ImGuiID, float> anim[3];
    float& a = anim[channel][id];
    a += (on ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 10.0f;
    a = std::clamp(a, 0.0f, 1.0f);
    return a * a * (3.0f - 2.0f * a);
}

void build_style() {
    const Palette& c = palette();
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    st.WindowPadding = ImVec2(24, 20);
    st.FramePadding = ImVec2(12, 7);
    st.ItemSpacing = ImVec2(10, 10);
    st.ItemInnerSpacing = ImVec2(8, 6);
    st.CellPadding = ImVec2(8, 5);
    st.IndentSpacing = 18;
    st.ScrollbarSize = 12;
    st.GrabMinSize = 16;
    st.WindowRounding = 14;
    st.ChildRounding = 12;
    st.FrameRounding = 10;
    st.PopupRounding = 12;
    st.GrabRounding = 10;
    st.ScrollbarRounding = 10;
    st.TabRounding = 12;
    st.WindowBorderSize = 0;
    st.ChildBorderSize = 1;
    st.PopupBorderSize = 0;
    st.FrameBorderSize = 0;
    st.TabBorderSize = 0;
    st.TabBarBorderSize = 0;
    st.TabBarOverlineSize = 0;
    st.SeparatorTextBorderSize = 2;
    st.SeparatorTextPadding = ImVec2(12, 4);
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* col = st.Colors;
    col[ImGuiCol_Text] = c.text;
    col[ImGuiCol_TextDisabled] = c.text_dim;
    col[ImGuiCol_WindowBg] = ImVec4(c.cream.x, c.cream.y, c.cream.z, 0.97f);
    col[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_PopupBg] = c.cream;
    col[ImGuiCol_Border] = ImVec4(c.night.x, c.night.y, c.night.z, 0.14f);
    col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_FrameBg] = c.night_light;
    col[ImGuiCol_FrameBgHovered] = c.night_hover;
    col[ImGuiCol_FrameBgActive] = c.night_hover;
    col[ImGuiCol_TitleBg] = c.pink;
    col[ImGuiCol_TitleBgActive] = c.pink;
    col[ImGuiCol_TitleBgCollapsed] = c.pink;
    col[ImGuiCol_MenuBarBg] = c.night_light;
    col[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_ScrollbarGrab] = ImVec4(c.night.x, c.night.y, c.night.z, 0.18f);
    col[ImGuiCol_ScrollbarGrabHovered] = c.pink;
    col[ImGuiCol_ScrollbarGrabActive] = c.pink_active;
    col[ImGuiCol_CheckMark] = c.night;
    col[ImGuiCol_SliderGrab] = c.pink_active;
    col[ImGuiCol_SliderGrabActive] = c.pink;
    col[ImGuiCol_Button] = c.pink;
    col[ImGuiCol_ButtonHovered] = c.pink_hover;
    col[ImGuiCol_ButtonActive] = c.pink_active;
    col[ImGuiCol_Header] = ImVec4(c.sky.x, c.sky.y, c.sky.z, 0.55f);
    col[ImGuiCol_HeaderHovered] = ImVec4(c.sky.x, c.sky.y, c.sky.z, 0.75f);
    col[ImGuiCol_HeaderActive] = c.sky;
    col[ImGuiCol_Separator] = ImVec4(c.night.x, c.night.y, c.night.z, 0.12f);
    col[ImGuiCol_SeparatorHovered] = c.pink;
    col[ImGuiCol_SeparatorActive] = c.pink;
    col[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_TabHovered] = ImVec4(c.sky.x, c.sky.y, c.sky.z, 0.6f);
    col[ImGuiCol_TabSelected] = c.pink;
    col[ImGuiCol_TabSelectedOverline] = c.pink;
    col[ImGuiCol_TabDimmed] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_TabDimmedSelected] = c.pink;
    col[ImGuiCol_TableHeaderBg] = c.night_light;
    col[ImGuiCol_TableBorderStrong] = ImVec4(c.night.x, c.night.y, c.night.z, 0.15f);
    col[ImGuiCol_TableBorderLight] = ImVec4(c.night.x, c.night.y, c.night.z, 0.08f);
    col[ImGuiCol_TableRowBgAlt] = ImVec4(c.night.x, c.night.y, c.night.z, 0.03f);
    col[ImGuiCol_TextSelectedBg] = ImVec4(c.yellow.x, c.yellow.y, c.yellow.z, 0.6f);
    col[ImGuiCol_NavCursor] = c.yellow;
    col[ImGuiCol_NavWindowingHighlight] = c.yellow;
    col[ImGuiCol_ModalWindowDimBg] = hex(0x3B2A4A, 0.6f);
}
}  // namespace

const Palette& palette() {
    static const Palette p{
        hex(0x3B2A4A),        // night (plum)
        hex(0xF3E6EE),        // night_light
        hex(0xEBD6E3),        // night_hover
        hex(0xF7A8C4),        // pink
        hex(0xF9BCD2),        // pink_hover
        hex(0xEE8DB1),        // pink_active
        hex(0xF7A8C4, 0.45f), // pink_soft
        hex(0xFFD95A),        // yellow
        hex(0x3B2A4A),        // text
        hex(0x7D6A86),        // text_dim
        hex(0x3B2A4A),        // text_on_pink
        hex(0x3E8E5E),        // good
        hex(0xC8445F),        // bad
        hex(0x9FD3F5),        // sky
        hex(0xFFF8F0),        // cream
        hex(0x3B2A4A, 0.25f), // shadow
    };
    return p;
}

void apply(float density) {
    ImGuiIO& io = ImGui::GetIO();
    g_density = std::max(1.0f, std::round(density));
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    auto add = [&](const unsigned char* data, unsigned size, float px) {
        return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), int(size), px * g_density, &cfg);
    };
    g_body = add(knid_font_Fredoka_Medium, knid_font_Fredoka_Medium_size, 20.0f);
    g_small = add(knid_font_Fredoka_Medium, knid_font_Fredoka_Medium_size, 16.0f);
    g_display = add(knid_font_PixelifySans_Medium, knid_font_PixelifySans_Medium_size, 22.0f);
    g_title = add(knid_font_PixelifySans_SemiBold, knid_font_PixelifySans_SemiBold_size, 44.0f);
    io.FontDefault = g_body;
    io.FontGlobalScale = 1.0f / g_density;
    build_style();
}

void update_scale(ImVec2 ds) { g_scale = (ds.x >= 1600 && ds.y >= 1000) ? 3 : 2; }
int ui_scale() { return g_scale; }
ImFont* font_body() { return g_body; }
ImFont* font_small() { return g_small; }
ImFont* font_title() { return g_title; }
ImFont* font_display() { return g_display; }
ImFont* font_button() { return g_display; }

void card(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    const Palette& c = palette();
    const float r = 16.0f;
    a = snap(a);
    b = snap(b);
    // Soft shadow from layered translucent rounded rects.
    for (int i = 6; i >= 1; --i) {
        float o = float(i) * 2.0f;
        dl->AddRectFilled(ImVec2(a.x - o * 0.5f, a.y + o * 0.6f), ImVec2(b.x + o * 0.5f, b.y + o * 1.2f),
                          u32(c.night, 0.035f), r + o);
    }
    dl->AddRectFilled(a, b, u32(c.cream, 0.95f), r);
    // Retro accent: a pixel-stepped highlight along the top edge.
    const float p = 2.0f;
    ImU32 hi = u32(ImVec4(1, 1, 1, 1), 0.9f);
    dl->AddRectFilled(ImVec2(a.x + r + 2 * p, a.y + 2 * p), ImVec2(b.x - r - 2 * p, a.y + 3 * p), hi);
    dl->AddRectFilled(ImVec2(a.x + r, a.y + 3 * p), ImVec2(a.x + r + 2 * p, a.y + 4 * p), hi);
    dl->AddRectFilled(ImVec2(b.x - r - 2 * p, a.y + 3 * p), ImVec2(b.x - r, a.y + 4 * p), hi);
}

void pixel_icon(ImDrawList* dl, ImVec2 pos, Icon icon, ImU32 color) {
    const char* rows = icon_rows(icon);
    if (!rows) return;
    const float s = float(ui_scale());
    pos = snap(pos);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            if (rows[y * 7 + x] == '#')
                dl->AddRectFilled(ImVec2(pos.x + x * s, pos.y + y * s), ImVec2(pos.x + (x + 1) * s, pos.y + (y + 1) * s), color);
}

void focus_star(ImDrawList* dl, ImVec2 center) {
    if (std::fmod(ImGui::GetTime(), 1.0) > 0.7) return;  // blink
    const float s = float(ui_scale());
    pixel_icon(dl, ImVec2(center.x - 3.5f * s, center.y - 3.5f * s), Icon::Star, u32(palette().yellow));
    // Plum outline pixels so the star reads on light cards.
    (void)s;
}

bool pixel_button(const char* label, ImVec2 size, bool primary, Icon icon) {
    const Palette& c = palette();
    ImGui::PushFont(g_display);
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 ts = ImGui::CalcTextSize(label, end);
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0) size.y = std::floor(ts.y + 22.0f);
    size = snap(size);
    ImVec2 pos = snap(ImGui::GetCursorScreenPos());
    bool pressed = ImGui::InvisibleButton(label, size);
    ImGuiID id = ImGui::GetItemID();
    bool hovered = ImGui::IsItemHovered();
    bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
    bool held = ImGui::IsItemActive() && hovered;
    float h = ease(id, 0, hovered || focused), p = ease(id, 1, held), f = ease(id, 2, focused);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = std::floor(size.y * 0.5f), lip = 4.0f;
    ImVec2 a = pos, b(pos.x + size.x, pos.y + size.y);
    // Soft yellow glow for focus.
    for (int i = 3; i >= 1 && f > 0.01f; --i)
        dl->AddRectFilled(ImVec2(a.x - i * 2.0f, a.y - i * 2.0f), ImVec2(b.x + i * 2.0f, b.y + i * 2.0f),
                          u32(c.yellow, 0.22f * f), r + i * 2.0f);
    ImVec4 face = primary ? c.pink : ImVec4(1, 1, 1, 1);
    ImVec4 face_hover = primary ? c.pink_hover : hex(0xEAF6FD);
    ImVec4 face_press = primary ? c.pink_active : hex(0xD6EEFB);
    ImVec4 base = primary ? hex(0xD97FA3) : hex(0xC9DCEB);
    ImVec4 fc = mix(mix(face, face_hover, h), face_press, p);
    dl->AddRectFilled(a, b, u32(base), r);                                 // lip
    dl->AddRectFilled(a, ImVec2(b.x, b.y - lip), u32(fc), r);              // face
    dl->AddRectFilled(ImVec2(a.x + r * 0.6f, a.y + 3), ImVec2(b.x - r * 0.6f, a.y + 5),
                      u32(ImVec4(1, 1, 1, 1), primary ? 0.35f : 0.8f), 1.0f);  // highlight
    // Content: pixel icon + label, centered on the face.
    const float s = float(ui_scale());
    float icon_w = icon != Icon::None ? 7 * s + 10.0f : 0.0f;
    float x = std::floor(a.x + (size.x - ts.x - icon_w) * 0.5f);
    float face_h = size.y - lip;
    if (icon != Icon::None) {
        pixel_icon(dl, ImVec2(x, std::floor(a.y + (face_h - 7 * s) * 0.5f)), icon, u32(c.night));
        x += icon_w;
    }
    dl->AddText(ImVec2(x, std::floor(a.y + (face_h - ts.y) * 0.5f)), u32(c.text), label, end);
    if (f > 0.01f) focus_star(dl, ImVec2(a.x - 16.0f, a.y + face_h * 0.5f));
    ImGui::PopFont();
    return pressed;
}

bool pixel_window_begin(const char* name, ImVec2 size, bool* open, ImGuiWindowFlags flags) {
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(36, 30));
    bool visible = ImGui::Begin(name, open, flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImVec2 p = ImGui::GetWindowPos(), sz = ImGui::GetWindowSize();
    // Drawn first, so the window's contents appear on top of the card.
    card(ImGui::GetWindowDrawList(), ImVec2(p.x + 8, p.y + 4), ImVec2(p.x + sz.x - 8, p.y + sz.y - 12));
    return visible;
}

void pixel_window_end() { ImGui::End(); }

void draw_star(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float rotation, bool filled) {
    (void)rotation;
    (void)filled;
    // The 7x7 star icon, scaled so its half-width is about `radius`.
    const char* rows = icon_rows(Icon::Star);
    float px = std::max(1.0f, std::round(radius / 3.5f));
    ImVec2 pos = snap(ImVec2(center.x - 3.5f * px, center.y - 3.5f * px));
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            if (rows[y * 7 + x] == '#')
                dl->AddRectFilled(ImVec2(pos.x + x * px, pos.y + y * px), ImVec2(pos.x + (x + 1) * px, pos.y + (y + 1) * px), color);
}

void draw_starfield(ImDrawList* dl, ImVec2 mn, ImVec2 mx, double t) {
    const Palette& c = palette();
    // Pastel dusk: plum at the top, soft mauve at the horizon.
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(mn, mx, u32(hex(0x3B2A4A)));
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, mn, ImVec2(mn.x, mx.y), u32(hex(0x3B2A4A)),
                                                  u32(hex(0xB07AA8)));
    const float s = float(ui_scale());
    float w = mx.x - mn.x, h = mx.y - mn.y;
    int count = int(w * h / (2600.0f * s));
    for (int i = 0; i < count; ++i) {
        float sx = hash01(std::uint32_t(i) * 3 + 1), sy = hash01(std::uint32_t(i) * 3 + 2);
        float phase = hash01(std::uint32_t(i) * 3 + 3) * 6.2831f;
        float x = mn.x + std::fmod(sx * w + float(t) * (2.0f + phase), w);
        float y = mn.y + sy * h * 0.85f;
        float tw = 0.5f + 0.5f * float(std::sin(t * (0.6 + phase * 0.25) + phase));
        ImVec4 col = (i % 5 == 0) ? c.yellow : c.cream;
        float px = (i % 7 == 0) ? 2 * s : s;
        ImVec2 p = snap(ImVec2(x, y));
        dl->AddRectFilled(p, ImVec2(p.x + px, p.y + px), u32(col, 0.35f + 0.65f * tw));
    }
    // A few bigger pixel stars that twinkle in place.
    for (int i = 0; i < 6; ++i) {
        float x = mn.x + hash01(std::uint32_t(i) * 11 + 101) * w, y = mn.y + hash01(std::uint32_t(i) * 11 + 102) * h * 0.8f;
        float a = 0.4f + 0.6f * float(0.5 + 0.5 * std::sin(t * 1.1 + i * 1.7));
        draw_star(dl, ImVec2(x, y), 3.5f * s, u32(c.yellow, a));
    }
}

bool big_button(const char* label, ImVec2 size, bool primary) { return pixel_button(label, size, primary); }

void centered_text(const char* text, ImFont* font, ImVec4 color) {
    if (font) ImGui::PushFont(font);
    float w = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(std::floor((ImGui::GetWindowSize().x - w) * 0.5f));
    if (color.w > 0) ImGui::TextColored(color, "%s", text);
    else ImGui::TextUnformatted(text);
    if (font) ImGui::PopFont();
}

}  // namespace knid::theme
