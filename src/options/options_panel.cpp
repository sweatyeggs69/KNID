#include "options/options_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/config.h"
#include "options/display.h"
#include "options/input.h"

namespace knid {
SettingsIO settings_io(Config& config) {
    return SettingsIO{
        [&config](const std::string& k, const std::string& d) { return config.get(k, d); },
        [&config](const std::string& k, const std::string& v) { config.set(k, v); },
        [&config]() { config.save(); },
    };
}
}  // namespace knid

namespace {
using namespace knid;
constexpr bool kShowWidescreen = true;
using namespace knid::display;
using knid::input::Button;

// The rebind state lives across frames; it is created on first draw for this Config.
struct PanelState {
    Config* config = nullptr;
    std::unique_ptr<input::Manager> input;
    struct ExtraTab {
        std::string name;
        std::function<void(Config&)> draw;
        bool before_builtin = false;
    };
    std::vector<ExtraTab> extra_tabs;
    std::string select_tab;  // applied once on the next draw
    std::string current_tab;  // the page shown in the sidebar layout
    bool menu_open = false;
    std::function<void()> on_quit;
    ImFont* heading_font = nullptr;
    ImFont* small_font = nullptr;
};
PanelState& state() {
    static PanelState s;
    return s;
}

void help_marker(const char* text) {
    ImGui::SameLine();
    ImGui::PushTextWrapPos(-1.0f);  // the marker never wraps onto extra lines
    ImGui::TextDisabled("(?)");
    ImGui::PopTextWrapPos();
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Widens the next combo so its longest choice is not cut off by the arrow button in a small
// window. It still leaves room on the line for the label and its (?) marker.
float fit_combo_width(const char* label, std::initializer_list<const char*> choices) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float text = 0.0f;
    for (const char* c : choices) text = std::max(text, ImGui::CalcTextSize(c).x);
    const float need = text + style.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
    const float room = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(label).x -
                       ImGui::CalcTextSize("(?)").x - style.ItemInnerSpacing.x - style.ItemSpacing.x * 2.0f;
    const float width = std::max(ImGui::CalcItemWidth(), std::min(need, room));
    ImGui::SetNextItemWidth(width);
    return width;
}

// True when `text` shows in full in a combo box `width` wide.
bool combo_fits(const char* text, float width) {
    return ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight() <= width;
}

template <typename E>
bool enum_combo(const char* label, E& value) {
    bool changed = false;
    if (ImGui::BeginCombo(label, name(value))) {
        for (int i = 0; i < static_cast<int>(E::Count); ++i) {
            const E v = static_cast<E>(i);
            if (ImGui::Selectable(name(v), v == value)) {
                value = v;
                changed = true;
            }
            if (v == value) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

const char* aspect_label(Aspect a) {
    switch (a) {
        case Aspect::Native: return "Off (original 3:2)";
        case Aspect::R16x10: return "16:10";
        case Aspect::R16x9: return "16:9";
        default: return name(a);
    }
}

void display_tab(Config& config) {
    Settings s = load([&](const std::string& k, const std::string& d) { return config.get(k, d); });
    bool changed = false;

    ImGui::SeparatorText("Size and shape");
    int scale = static_cast<int>(s.scale);
    changed |= ImGui::RadioButton("Pixel perfect (integer scaling)", &scale, int(ScaleMode::Integer));
    help_marker("Every GBA pixel becomes an exact block of screen pixels, so nothing shimmers or "
                "blurs. Leaves a small border when the window is not an exact multiple.");
    changed |= ImGui::RadioButton("Fit to window", &scale, int(ScaleMode::Fit));
    help_marker("Fills as much of the window as possible. Some pixels end up one screen pixel "
                "wider than others.");
    s.scale = static_cast<ScaleMode>(scale);

    // Widescreen shows extra level at the sides (Gba::set_wide_margin); the host applies
    // display::wide_margin(aspect) and presents Gba::wide_framebuffer().
    if (kShowWidescreen) {
        const float w = fit_combo_width("Widescreen view", {aspect_label(Aspect::Native), aspect_label(Aspect::R16x10),
                                                            aspect_label(Aspect::R16x9)});
        // In a small window the long "Off" label would hide under the arrow, so it shortens.
        const char* preview = aspect_label(s.aspect);
        if (s.aspect == Aspect::Native && !combo_fits(preview, w)) preview = "Off (3:2)";
        if (ImGui::BeginCombo("Widescreen view", preview)) {
            for (int i = 0; i < int(Aspect::Count); ++i) {
                const Aspect a = Aspect(i);
                if (ImGui::Selectable(aspect_label(a), a == s.aspect)) {
                    s.aspect = a;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        help_marker("Shows more of the level at the left and right instead of stretching the "
                    "picture. Pixels always stay square, and the HUD stays where it was. Menus "
                    "and the title screen stay 3:2.");
    }

    bool fullscreen = config.get("display.fullscreen", "0") == "1";
    if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
        config.set("display.fullscreen", fullscreen ? "1" : "0");
        if (SDL_Window* w = SDL_GetKeyboardFocus()) SDL_SetWindowFullscreen(w, fullscreen);
    }
    help_marker("F11 also switches fullscreen.");

    ImGui::SeparatorText("Look");
    bool gba_colors = s.color == ColorMode::GbaLcd;
    if (ImGui::Checkbox("Original GBA screen colors", &gba_colors)) {
        s.color = gba_colors ? ColorMode::GbaLcd : ColorMode::Raw;
        changed = true;
    }
    help_marker("Softer, less saturated colors like the original unlit GBA screen. The game's "
                "colors were chosen for that screen, so this is how they were meant to look.");

    fit_combo_width("Frame blending", {"Off", "Blend two frames", "LCD ghosting"});
    if (ImGui::BeginCombo("Frame blending", s.blend == Blend::Off ? "Off"
                                           : s.blend == Blend::Mix ? "Blend two frames"
                                                                   : "LCD ghosting")) {
        const char* labels[] = {"Off", "Blend two frames", "LCD ghosting"};
        for (int i = 0; i < int(Blend::Count); ++i)
            if (ImGui::Selectable(labels[i], int(s.blend) == i)) {
                s.blend = Blend(i);
                changed = true;
            }
        ImGui::EndCombo();
    }
    help_marker("GBA games flicker some sprites every other frame on purpose, counting on the "
                "slow GBA screen to smooth it out. Blending averages frames to match. LCD "
                "ghosting also leaves short trails like the real screen.");

    const char* overlay_labels[] = {"None", "Pixel grid", "LCD subpixels", "Scanlines"};
    int ov = int(s.overlay);
    fit_combo_width("Screen overlay", {overlay_labels[0], overlay_labels[1], overlay_labels[2], overlay_labels[3]});
    if (ImGui::Combo("Screen overlay", &ov, overlay_labels, int(Overlay::Count))) {
        s.overlay = Overlay(ov);
        changed = true;
    }
    help_marker("Pixel grid draws thin lines between pixels like the GBA's visible pixel gaps. "
                "LCD subpixels adds red, green and blue stripes. Overlays need at least 2x "
                "scaling (3x for subpixels).");
    ImGui::BeginDisabled(s.overlay == Overlay::None);
    changed |= ImGui::SliderInt("Overlay strength", &s.overlay_strength, 0, 100, "%d%%");
    ImGui::EndDisabled();

    if (changed) store(s, [&](const std::string& k, const std::string& v) { config.set(k, v); });

}

void controls_tab(input::Manager& in, Config& config) {
    const Button waiting = in.remap_target();

    ImGui::TextWrapped("Click a button to rebind it, then press a key, controller button or "
                       "stick direction. A keyboard key replaces the keyboard binding and a "
                       "controller input replaces the controller binding, so both keep working.");
    ImGui::Text("Controllers connected: %d", in.gamepad_count());
    ImGui::Spacing();

    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                             ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("GBA button", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Bound to");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        static constexpr Button kOrder[] = {Button::Up, Button::Down, Button::Left, Button::Right,
                                            Button::A,  Button::B,    Button::L,    Button::R,
                                            Button::Start, Button::Select};
        for (Button b : kOrder) {
            ImGui::PushID(int(b));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(input::name(b));
            ImGui::TableNextColumn();
            if (waiting == b) ImGui::TextUnformatted("Press a key or button...  (Esc to keep)");
            else ImGui::TextWrapped("%s", in.describe(b).c_str());
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(in.remapping());
            if (ImGui::SmallButton("Rebind")) in.start_remap(b);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Turbo");
    {
        const input::TurboSettings& t = in.config().turbo;
        bool on = config.get_bool("input.turbo", t.enabled);
        if (ImGui::Checkbox("Turbo buttons", &on)) {
            config.set_bool("input.turbo", on);
            in.reload();
        }
        help_marker("While you hold a turbo button, the game sees it pressed over and over. "
                    "Handy for rapid fire, but turn it off for a button you need to hold, "
                    "such as B to inhale.");
        ImGui::BeginDisabled(!on);
        int rate = std::clamp(config.get_int("input.turbo.rate", t.rate), input::kTurboMinRate,
                              input::kTurboMaxRate);
        if (ImGui::SliderInt("Turbo rate", &rate, input::kTurboMinRate, input::kTurboMaxRate,
                             rate == 1 ? "%d press per second" : "%d presses per second",
                             ImGuiSliderFlags_AlwaysClamp)) {
            config.set("input.turbo.rate", std::to_string(rate));
            in.reload();
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Turbo on:");
        static constexpr Button kTurboButtons[] = {Button::A, Button::B, Button::L, Button::R};
        std::uint16_t mask = t.buttons;
        bool mask_changed = false;
        for (Button b : kTurboButtons) {
            ImGui::SameLine();
            ImGui::PushID(200 + int(b));
            const std::uint16_t bit = std::uint16_t(1u << int(b));
            bool sel = (mask & bit) != 0;
            if (ImGui::Checkbox(input::name(b), &sel)) {
                mask = sel ? std::uint16_t(mask | bit) : std::uint16_t(mask & ~bit);
                mask_changed = true;
            }
            ImGui::PopID();
        }
        if (mask_changed) {
            config.set("input.turbo.buttons", input::turbo_buttons_to_string(mask));
            in.reload();
        }
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Shortcuts");
    ImGui::TextWrapped("For a controller shortcut, hold the buttons together (up to two) and "
                       "let go. F1, F11 and Esc are kept for the menu and fullscreen.");
    const input::Hotkey hk_waiting = in.hotkey_remap_target();
    if (ImGui::BeginTable("shortcuts", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Bound to");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (int i = 0; i < input::kHotkeyCount; ++i) {
            const auto h = input::Hotkey(i);
            ImGui::PushID(100 + i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(input::name(h));
            ImGui::TableNextColumn();
            if (hk_waiting == h) ImGui::TextUnformatted("Press a key or buttons...  (Esc to keep)");
            else ImGui::TextWrapped("%s", in.describe(h).c_str());
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(in.remapping());
            if (ImGui::SmallButton("Rebind")) in.start_remap(h);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) in.clear(h);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(in.remapping());
    if (ImGui::Button("Rebind all")) in.start_remap();
    ImGui::SameLine();
    if (ImGui::Button("Reset to defaults")) in.reset_to_defaults();
    ImGui::EndDisabled();
    if (in.remapping()) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) in.cancel_remap();
    }

    int dz = config.get_int("input.deadzone", in.config().deadzone_percent);
    if (ImGui::SliderInt("Stick dead zone", &dz, 5, 95, "%d%%")) {
        config.set("input.deadzone", std::to_string(dz));
        in.reload();
    }
    help_marker("How far a stick or trigger must move before it counts as a press.");

}
}  // namespace

namespace {
// One sidebar entry: a full-width rounded pill. The selected page is pink with dark text.
bool nav_item(const char* label, bool selected, float height) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    const float rounding = style.FrameRounding;
    if (selected) {
        dl->AddRectFilled(pos, end, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_Button), rounding);
    } else if (hovered || held) {
        dl->AddRectFilled(pos, end, ImGui::GetColorU32(ImGuiCol_FrameBgHovered), rounding);
    }
    // Dark text on the pink pill, the theme's normal text elsewhere.
    const ImU32 text = selected ? IM_COL32(58, 20, 46, 255) : ImGui::GetColorU32(ImGuiCol_Text);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(pos.x + style.FramePadding.x + 2.0f, pos.y + (size.y - ts.y) * 0.5f), text, label);
    return pressed;
}

struct Page {
    std::string name;
    std::function<void()> draw;
};

std::vector<Page> collect_pages(PanelState& st, knid::Config& config) {
    std::vector<Page> pages;
    auto extras = [&](bool before) {
        for (auto& t : st.extra_tabs)
            if (t.before_builtin == before) pages.push_back({t.name, [&t, &config] { if (t.draw) t.draw(config); }});
    };
    extras(true);
    pages.push_back({"Display", [&config] { display_tab(config); }});
    pages.push_back({"Controls", [&st, &config] { controls_tab(*st.input, config); }});
    extras(false);
    return pages;
}

void ensure_input(PanelState& st, knid::Config& config) {
    if (st.config != &config || !st.input) {
        st.config = &config;
        st.input = std::make_unique<input::Manager>(settings_io(config));
        st.input->set_shortcuts_enabled(false);
    }
}

// Draws the selected page in a padded, scrollable pane that fills the rest of the window.
void draw_page(PanelState& st, const Page& page) {
    const ImGuiStyle& style = ImGui::GetStyle();
    // Outer pane: rounded background with even padding on every side.
    // The pane and its body split the side padding (16 + 10 = 26) so the body's scrollbar sits
    // 16 from the edge with a 10 gap before the content.
    constexpr float kPaneX = 16.0f, kBodyX = 10.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPaneX, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
    const bool pane = ImGui::BeginChild("page", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    if (pane) {
        if (st.heading_font) ImGui::PushFont(st.heading_font);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kBodyX);
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Button), "%s", page.name.c_str());
        if (st.heading_font) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, style.ItemSpacing.y * 0.25f));
        // Inner body scrolls under the fixed heading. Its padding is small, so headers and
        // separators stay inside the pane's padding instead of reaching into it.
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kBodyX, 0.0f));
        const bool body = ImGui::BeginChild("body", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();
        if (body) {
            // Inputs take about half the pane so their labels stay readable beside them.
            ImGui::PushItemWidth(std::min(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetFontSize() * 14.0f));
            ImGui::PushTextWrapPos(0.0f);  // long lines wrap inside the pane instead of running off
            page.draw();
            ImGui::PopTextWrapPos();
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}
}  // namespace

void draw_options_panel(knid::Config& config) {
    auto& st = state();
    ensure_input(st, config);
    std::vector<Page> pages = collect_pages(st, config);
    if (!st.select_tab.empty()) {
        st.current_tab = std::move(st.select_tab);
        st.select_tab.clear();
    }
    auto it = std::find_if(pages.begin(), pages.end(), [&](const Page& p) { return p.name == st.current_tab; });
    if (it == pages.end()) {
        it = pages.begin();
        if (it != pages.end()) st.current_tab = it->name;
    }
    if (st.current_tab != "Controls" && st.input->remapping()) st.input->cancel_remap();  // left mid-rebind
    if (it != pages.end()) draw_page(st, *it);
    st.input->take_message();
}

bool options_panel_handle_event(const SDL_Event& event) {
    auto& st = state();
    return st.input ? st.input->handle_event(event) : false;
}

void register_options_tab(const char* name, std::function<void(knid::Config&)> draw, bool before_builtin) {
    auto& tabs = state().extra_tabs;
    for (auto& t : tabs)
        if (t.name == name) {
            t.draw = std::move(draw);
            return;
        }
    tabs.push_back({name, std::move(draw), before_builtin});
}

void options_select_tab(const char* name) { state().select_tab = name; }

void options_menu_toggle() { options_menu_set_open(!state().menu_open); }

void options_menu_set_open(bool open) {
    auto& st = state();
    if (!open && st.input && st.input->remapping()) st.input->cancel_remap();
    st.menu_open = open;
}

bool options_menu_open() { return state().menu_open; }

void set_options_menu_quit_handler(std::function<void()> on_quit) { state().on_quit = std::move(on_quit); }

void set_options_menu_fonts(ImFont* heading, ImFont* small) {
    state().heading_font = heading;
    state().small_font = small;
}

void draw_options_menu(knid::Config& config) {
    auto& st = state();
    if (!st.menu_open) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // Dim the paused game behind the menu with the theme's modal dim color (set in theme.cpp).
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                  ImGui::GetColorU32(ImGuiCol_ModalWindowDimBg));
    // A fixed 3:2 panel in window points. It never grows past kMaxWidth, so it stays the same
    // size in fullscreen and on HiDPI screens, and shrinks (keeping 3:2) in small windows.
    constexpr float kMaxWidth = 900.0f, kMargin = 24.0f;
    float w = std::min(vp->Size.x - 2 * kMargin, kMaxWidth);
    float h = w / 1.5f;
    if (h > vp->Size.y - 2 * kMargin) {
        h = vp->Size.y - 2 * kMargin;
        w = h * 1.5f;
    }
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
    ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    bg.w = 1.0f;  // opaque, so nothing on the paused game shows through the text
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    const bool shown = ImGui::Begin("Settings", nullptr,
                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (shown) {
        ensure_input(st, config);
        std::vector<Page> pages = collect_pages(st, config);
        if (!st.select_tab.empty()) {
            st.current_tab = std::move(st.select_tab);
            st.select_tab.clear();
        }
        if (st.current_tab.empty() && !pages.empty()) st.current_tab = pages.front().name;

        // Sidebar: title, one entry per page, then Resume / Quit pinned to the bottom.
        const float nav_w = std::max(ImGui::GetFontSize() * 8.5f, w * 0.22f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        if (ImGui::BeginChild("nav", ImVec2(nav_w, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                              ImGuiWindowFlags_NoScrollbar)) {
            if (st.heading_font) ImGui::PushFont(st.heading_font);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x + 2.0f);
            ImGui::TextUnformatted("Settings");
            if (st.heading_font) ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 6.0f));

            // Bottom block: buttons and the close hint. In short windows the hint goes first,
            // then the entries get shorter, so every page and both buttons stay visible.
            const ImGuiStyle& style = ImGui::GetStyle();
            const float btn_h = ImGui::GetFrameHeight();
            const int buttons = st.on_quit ? 2 : 1;
            const float hint_h = ImGui::GetTextLineHeight() * 1.5f;
            const float bottom = ImGui::GetWindowHeight() - style.WindowPadding.y;
            const float n = float(pages.size());
            float block = (btn_h + style.ItemSpacing.y) * float(buttons);
            float room = bottom - ImGui::GetCursorPosY() - block - style.ItemSpacing.y * 2.0f;
            const bool show_hint = room - hint_h >= n * (btn_h + style.ItemSpacing.y);
            if (show_hint) {
                block += hint_h;
                room -= hint_h;
            }
            const float item_h = std::clamp(room / n - style.ItemSpacing.y, ImGui::GetTextLineHeight() + 4.0f, btn_h);
            for (const Page& p : pages) {
                if (nav_item(p.name.c_str(), p.name == st.current_tab, item_h)) st.current_tab = p.name;
            }
            const float y = bottom - block;
            if (y > ImGui::GetCursorPosY()) ImGui::SetCursorPosY(y);
            if (ImGui::Button("Resume", ImVec2(-FLT_MIN, 0))) open = false;
            if (st.on_quit) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                if (ImGui::Button("Quit to launcher", ImVec2(-FLT_MIN, 0))) {
                    open = false;
                    st.on_quit();
                }
                ImGui::PopStyleColor(3);
            }
            if (show_hint) {
                if (st.small_font) ImGui::PushFont(st.small_font);
                ImGui::TextDisabled("F1 or Esc closes this menu");
                if (st.small_font) ImGui::PopFont();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImGui::SameLine(0.0f, 8.0f);
        draw_options_panel(config);
    }
    ImGui::End();
    if (!open) options_menu_set_open(false);
}
