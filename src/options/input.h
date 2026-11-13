#pragma once
// Keyboard and gamepad input mapped to GBA buttons, with user-editable bindings that are
// saved in the app config and an in-game remapping mode.
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "options/settings_io.h"
#include "options/turbo.h"

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

// Which column of the bindings table an input belongs to: keyboard, or controller (gamepad
// buttons and sticks). Any means either.
enum class Source { Any, Keyboard, Controller };
inline bool is_keyboard(const Binding& b) { return b.kind == Binding::Kind::Key; }
inline bool matches(Source s, const Binding& b) {
    return s == Source::Any || (s == Source::Keyboard) == is_keyboard(b);
}

// "key:X", "pad:a", "axis:leftx-", "axis:righttrigger+"
std::string to_string(const Binding& b);
bool from_string(const std::string& s, Binding& out);
// Friendly name for menus and messages: "Up Arrow", "Left D-Pad", "A Button", "Left Stick Up".
std::string display_name(const Binding& b);

// Emulator shortcuts, stored as input.hotkey.<id> (id from key_name). Each shortcut is a list of
// chords. A chord is one keyboard key, or one or two controller buttons held together.
// FastForward and Rewind work while held (see Manager::hotkey_held); FastForwardToggle turns fast
// forward on and off with each press.
enum class Hotkey { SaveState, SaveNewSlot, LoadState, SlotPrev, SlotNext, FastForward, FastForwardToggle, Rewind, Count };
inline constexpr int kHotkeyCount = static_cast<int>(Hotkey::Count);
const char* name(Hotkey h);      // "Save state"
const char* key_name(Hotkey h);  // "save_state"

using Chord = std::vector<Binding>;
// "key:PageUp", "pad:back+pad:rightshoulder"
std::string to_string(const Chord& c);
bool chord_from_string(const std::string& s, Chord& out);
std::string join_chords(const std::vector<Chord>& list);
std::vector<Chord> split_chords(const std::string& text);

struct Config {
    std::array<std::vector<Binding>, kButtonCount> buttons;
    std::array<std::vector<Chord>, kHotkeyCount> hotkeys;
    int deadzone_percent = 40;  // stick and trigger threshold
    TurboSettings turbo;
};

Config default_config();
// Stored as input.deadzone and input.<Button> = "key:X | pad:a | axis:leftx+" ("none" = unbound).
// Turbo: input.turbo = true/false, input.turbo.rate = 1..5 presses per second,
// input.turbo.buttons = "A, B" (any GBA button names, or "none").
// Missing or unreadable entries keep their defaults.
Config load(const SettingsIO& io);
void store(const Config& c, const SettingsIO& io);
std::string join(const std::vector<Binding>& list);
std::vector<Binding> split(const std::string& text);
std::string turbo_buttons_to_string(std::uint16_t buttons);
std::uint16_t turbo_buttons_from_string(const std::string& text);

// Owns the open gamepads, the bindings and the remap state machine.
class Manager {
public:
    explicit Manager(SettingsIO io);
    ~Manager();

    // Gamepad hotplug, remap capture and shortcuts. Returns true when the event was consumed
    // by remapping or fired a shortcut (knid::states::quick_save and friends), so the caller
    // should not use it again.
    bool handle_event(const SDL_Event& e);

    // Current GBA KEYINPUT-style mask (1 = pressed). Empty while remapping.
    std::uint16_t poll() const;
    // poll() with turbo applied. Call once per emulated frame; turbo timing counts these calls.
    std::uint16_t poll_frame();
    // True while every input of one of the shortcut's chords is held (for the hold shortcuts:
    // fast forward and rewind). Always false while remapping or with shortcuts off.
    bool hotkey_held(Hotkey h) const;
    // Fast forward latched on by the toggle shortcut.
    bool fast_forward_latched() const { return ff_latched_; }
    void set_fast_forward_latched(bool on) { ff_latched_ = on; }

    // Remapping walks through every button in turn. The first key, pad button or stick
    // direction pressed replaces that button's bindings for that device type.
    void start_remap();
    // Rebinds just one button (used by the options screen's click-to-rebind). With a source,
    // only that device's input binds; pressing anything on the other device (or Esc) keeps
    // the current binding.
    void start_remap(Button only, Source source = Source::Any);
    // Button currently waiting for input, or Button::Count when not remapping.
    Button remap_target() const;
    // Rebinds one shortcut: the next key, or controller button(s) pressed together and released.
    void start_remap(Hotkey only, Source source = Source::Any);
    Hotkey hotkey_remap_target() const { return hotkey_remap_; }
    void clear(Hotkey h, Source source = Source::Any);  // unbinds a shortcut
    void clear(Button b, Source source);  // drops one device's bindings for a GBA button
    Source remap_source() const { return remap_source_; }
    // Off for the settings menu's own instance, so shortcuts never fire while it is open.
    void set_shortcuts_enabled(bool on) { shortcuts_enabled_ = on; }
    void cancel_remap();
    void reload();  // re-read bindings from the config
    bool remapping() const { return remap_index_ >= 0 || hotkey_remap_ != Hotkey::Count; }
    std::string remap_prompt() const;  // empty when not remapping

    void reset_to_defaults();
    const Config& config() const { return config_; }
    // "X, A Button" style summaries for menus and messages, optionally one device's only.
    std::string describe(Button b, Source source = Source::Any) const;
    std::string describe(Hotkey h, Source source = Source::Any) const;  // "Page Up, Back Button + Right Bumper"
    std::string take_message();           // one-shot status text for the on-screen display
    int gamepad_count() const { return int(pads_.size()); }

private:
    void save() const;
    void assign(const Binding& b);
    void advance();
    void assign_hotkey(const Chord& c);
    void fire(Hotkey h);

    SettingsIO io_;
    Config config_;
    std::vector<SDL_Gamepad*> pads_;
    int remap_index_ = -1;
    bool remap_single_ = false;
    Source remap_source_ = Source::Any;  // device the current single rebind listens to
    std::array<bool, SDL_GAMEPAD_AXIS_COUNT> axis_armed_{};  // axis returned to center since prompt
    std::string message_;
    Hotkey hotkey_remap_ = Hotkey::Count;
    bool shortcuts_enabled_ = true;
    bool ff_latched_ = false;
    Chord capture_;  // controller buttons held while rebinding a shortcut
    // Controller buttons that just fired a shortcut; kept away from the game until released.
    mutable std::vector<int> suppressed_;
    TurboState turbo_;
};

}  // namespace knid::input
