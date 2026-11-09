#include "options/options_panel.h"

#include <imgui.h>

#include <memory>
#include <string>

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
using namespace knid::display;
using knid::input::Button;

// The rebind state lives across frames; it is created on first draw for this Config.
struct PanelState {
    Config* config = nullptr;
    std::unique_ptr<input::Manager> input;
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
        case Aspect::Native: return "3:2 (original)";
        case Aspect::Fill: return "Fill window";
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

    if (ImGui::BeginCombo("Aspect ratio", aspect_label(s.aspect))) {
        for (int i = 0; i < int(Aspect::Count); ++i) {
            const Aspect a = Aspect(i);
            if (ImGui::Selectable(aspect_label(a), a == s.aspect)) {
                s.aspect = a;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    help_marker("Wider shapes stretch the 240x160 picture sideways to fill modern screens. "
                "With pixel perfect on, pixels stay uniform: each one becomes, for example, "
                "7 screen pixels wide and 6 tall.");

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

    ImGui::Spacing();
    ImGui::TextDisabled("In game: F2 scaling, F3 aspect, F4 overlay, F5 strength,");
    ImGui::TextDisabled("F6 colors, F7 blending, F11 fullscreen, F1 help.");
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
            ImGui::BeginDisabled(waiting != Button::Count);
            if (ImGui::SmallButton("Rebind")) in.start_remap(b);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(waiting != Button::Count);
    if (ImGui::Button("Rebind all")) in.start_remap();
    ImGui::SameLine();
    if (ImGui::Button("Reset to defaults")) in.reset_to_defaults();
    ImGui::EndDisabled();
    if (waiting != Button::Count) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) in.cancel_remap();
    }

    int dz = config.get_int("input.deadzone", in.config().deadzone_percent);
    if (ImGui::SliderInt("Stick dead zone", &dz, 5, 95, "%d%%")) {
        config.set("input.deadzone", std::to_string(dz));
        in.reload();
    }
    help_marker("How far a stick or trigger must move before it counts as a press.");

    ImGui::Spacing();
    ImGui::TextDisabled("In game: F9 rebinds every button in turn, Shift+F9 restores defaults.");
}
}  // namespace

void draw_options_panel(knid::Config& config) {
    auto& st = state();
    if (st.config != &config || !st.input) {
        st.config = &config;
        st.input = std::make_unique<input::Manager>(settings_io(config));
    }
    if (ImGui::BeginTabBar("options")) {
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
