#pragma once
// Keyboard and gamepad input mapped to GBA buttons, with user-editable bindings that are
// saved in the app config and an in-game remapping mode.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "options/settings_io.h"

namespace knid::input {

// GBA buttons in KEYINPUT bit order (bit = index).
enum class Button { A, B, Select, Start, Right, Left, Up, Down, R, L, Count };
inline constexpr int kButtonCount = static_cast<int>(Button::Count);
const char* name(Button b);

struct Binding {
    enum class Kind { Key, PadButton, PadAxis } kind = Kind::Key;
    int code = 0;      // SDL_Scancode, SDL_GamepadButton or SDL_GamepadAxis
    int dir = 0;       // PadAxis only: -1 or +1
    bool operator==(const Binding&) const = default;
};

// "key:X", "pad:a", "axis:leftx-", "axis:righttrigger+"
std::string to_string(const Binding& b);
bool from_string(const std::string& s, Binding& out);

struct Config {
    std::array<std::vector<Binding>, kButtonCount> buttons;
    int deadzone_percent = 40;  // stick and trigger threshold
};

Config default_config();
// Stored as input.deadzone and input.<Button> = "key:X | pad:a | axis:leftx+" ("none" = unbound).
// Missing or unreadable entries keep their defaults.
Config load(const SettingsIO& io);
void store(const Config& c, const SettingsIO& io);
std::string join(const std::vector<Binding>& list);
std::vector<Binding> split(const std::string& text);

// Owns the open gamepads, the bindings and the remap state machine.
class Manager {
public:
    explicit Manager(SettingsIO io);
    ~Manager();

    // Gamepad hotplug and remap capture. Returns true when the event was consumed by remapping
    // (so the caller should not treat it as a hotkey).
    bool handle_event(const SDL_Event& e);

    // Current GBA KEYINPUT-style mask (1 = pressed). Empty while remapping.
    std::uint16_t poll() const;

    // Remapping walks through every button in turn. The first key, pad button or stick
    // direction pressed replaces that button's bindings for that device type.
    void start_remap();
    // Rebinds just one button (used by the options screen's click-to-rebind).
    void start_remap(Button only);
    // Button currently waiting for input, or Button::Count when not remapping.
    Button remap_target() const;
    void cancel_remap();
    void reload();  // re-read bindings from the config
    bool remapping() const { return remap_index_ >= 0; }
    std::string remap_prompt() const;  // empty when not remapping

    void reset_to_defaults();
    const Config& config() const { return config_; }
    std::string describe(Button b) const;  // "X, pad a" style summary for messages
    std::string take_message();           // one-shot status text for the on-screen display
    int gamepad_count() const { return int(pads_.size()); }

private:
    void save() const;
    void assign(const Binding& b);
    void advance();

    SettingsIO io_;
    Config config_;
    std::vector<SDL_Gamepad*> pads_;
    int remap_index_ = -1;
    bool remap_single_ = false;
    std::array<bool, SDL_GAMEPAD_AXIS_COUNT> axis_armed_{};  // axis returned to center since prompt
    std::string message_;
};

}  // namespace knid::input
