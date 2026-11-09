#include "options/options_panel.h"

#include <imgui.h>

#include <algorithm>
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
    std::vector<std::pair<std::string, std::function<void(Config&)>>> extra_tabs;
    bool menu_open = false;
    std::function<void()> on_quit;
};
PanelState& state() {
    static PanelState s;
    return s;
}

void help_marker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
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
        case Aspect::R21x9: return "21:9 (experimental)";
        case Aspect::R32x9: return "32:9 (experimental)";
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
        if (ImGui::BeginCombo("Widescreen view", aspect_label(s.aspect))) {
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
                    "and the title screen stay 3:2. 16:10 and 16:9 show real scenery; 21:9 and "
                    "32:9 are experimental, because the game only loads a little ahead of the "
                    "original view, so the far edges can show unfinished scenery.");
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

void draw_options_panel(knid::Config& config) {
    auto& st = state();
    if (st.config != &config || !st.input) {
        st.config = &config;
        st.input = std::make_unique<input::Manager>(settings_io(config));
        st.input->set_shortcuts_enabled(false);
    }
    if (ImGui::BeginTabBar("options")) {
        for (auto& [tab_name, draw] : st.extra_tabs) {
            if (ImGui::BeginTabItem(tab_name.c_str())) {
                if (draw) draw(config);
                ImGui::EndTabItem();
            }
        }
        if (ImGui::BeginTabItem("Display")) {
            display_tab(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls")) {
            controls_tab(*st.input, config);
            ImGui::EndTabItem();
        } else if (st.input->remapping()) {
            st.input->cancel_remap();  // left the tab mid-rebind
        }
        ImGui::EndTabBar();
    }
    st.input->take_message();
}

bool options_panel_handle_event(const SDL_Event& event) {
    auto& st = state();
    return st.input ? st.input->handle_event(event) : false;
}

void register_options_tab(const char* name, std::function<void(knid::Config&)> draw) {
    auto& tabs = state().extra_tabs;
    for (auto& t : tabs)
        if (t.first == name) {
            t.second = std::move(draw);
            return;
        }
    tabs.emplace_back(name, std::move(draw));
}

void options_menu_toggle() { options_menu_set_open(!state().menu_open); }

void options_menu_set_open(bool open) {
    auto& st = state();
    if (!open && st.input && st.input->remapping()) st.input->cancel_remap();
    st.menu_open = open;
}

bool options_menu_open() { return state().menu_open; }

void set_options_menu_quit_handler(std::function<void()> on_quit) { state().on_quit = std::move(on_quit); }

void draw_options_menu(knid::Config& config) {
    auto& st = state();
    if (!st.menu_open) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // Dim the paused game behind the menu.
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                  IM_COL32(10, 6, 30, 150));
    const ImVec2 size(std::min(vp->Size.x * 0.9f, 640.0f * ImGui::GetFontSize() / 13.0f),
                      std::min(vp->Size.y * 0.9f, 560.0f * ImGui::GetFontSize() / 13.0f));
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    bool open = true;
    if (ImGui::Begin("Settings", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("tabs", ImVec2(0, -footer))) draw_options_panel(config);
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button("Resume")) open = false;
        if (st.on_quit) {
            ImGui::SameLine();
            if (ImGui::Button("Quit to launcher")) {
                open = false;
                st.on_quit();
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F1 or Esc closes this menu");
    }
    ImGui::End();
    if (!open) options_menu_set_open(false);
}
