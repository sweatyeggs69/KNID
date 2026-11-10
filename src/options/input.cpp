#include "options/input.h"

#include <algorithm>
#include <cstdlib>

#include "app/states.h"

namespace knid::input {

namespace {
// Order the remap walks through, D-pad first like most setup screens.
constexpr Button kRemapOrder[] = {Button::Up, Button::Down, Button::Left, Button::Right, Button::A,
                                  Button::B,  Button::L,    Button::R,    Button::Start, Button::Select};

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    auto e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Keys the shell uses itself; they can never be bound to a GBA button.
bool reserved_key(SDL_Scancode sc) {
    return sc == SDL_SCANCODE_ESCAPE || (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12);
}
// Shortcuts may use function keys, except the ones the shell keeps (F1 menu, F11 fullscreen).
bool reserved_hotkey_key(SDL_Scancode sc) {
    return sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_F1 || sc == SDL_SCANCODE_F11;
}

Binding key(SDL_Scancode sc) { return {Binding::Kind::Key, int(sc), 0}; }
Binding pad(SDL_GamepadButton b) { return {Binding::Kind::PadButton, int(b), 0}; }
Binding axis(SDL_GamepadAxis a, int dir) { return {Binding::Kind::PadAxis, int(a), dir}; }
}  // namespace

const char* name(Button b) {
    switch (b) {
        case Button::A: return "A";
        case Button::B: return "B";
        case Button::Select: return "Select";
        case Button::Start: return "Start";
        case Button::Right: return "Right";
        case Button::Left: return "Left";
        case Button::Up: return "Up";
        case Button::Down: return "Down";
        case Button::R: return "R";
        case Button::L: return "L";
        default: return "?";
    }
}

std::string to_string(const Binding& b) {
    switch (b.kind) {
        case Binding::Kind::Key: {
            const char* n = SDL_GetScancodeName(SDL_Scancode(b.code));
            return std::string("key:") + (n && *n ? n : std::to_string(b.code).c_str());
        }
        case Binding::Kind::PadButton: {
            const char* n = SDL_GetGamepadStringForButton(SDL_GamepadButton(b.code));
            return std::string("pad:") + (n ? n : "?");
        }
        case Binding::Kind::PadAxis: {
            const char* n = SDL_GetGamepadStringForAxis(SDL_GamepadAxis(b.code));
            return std::string("axis:") + (n ? n : "?") + (b.dir < 0 ? "-" : "+");
        }
    }
    return "?";
}

std::string display_name(const Binding& b) {
    switch (b.kind) {
        case Binding::Kind::Key: {
            switch (SDL_Scancode(b.code)) {
                case SDL_SCANCODE_UP: return "Up Arrow";
                case SDL_SCANCODE_DOWN: return "Down Arrow";
                case SDL_SCANCODE_LEFT: return "Left Arrow";
                case SDL_SCANCODE_RIGHT: return "Right Arrow";
                case SDL_SCANCODE_RETURN: return "Enter";
                case SDL_SCANCODE_KP_ENTER: return "Keypad Enter";
                case SDL_SCANCODE_SPACE: return "Space";
#ifdef __APPLE__
                case SDL_SCANCODE_LGUI: return "Left Command";
                case SDL_SCANCODE_RGUI: return "Right Command";
                case SDL_SCANCODE_LALT: return "Left Option";
                case SDL_SCANCODE_RALT: return "Right Option";
#elif defined(_WIN32)
                case SDL_SCANCODE_LGUI: return "Left Windows";
                case SDL_SCANCODE_RGUI: return "Right Windows";
#endif
                default: break;
            }
            const char* n = SDL_GetScancodeName(SDL_Scancode(b.code));
            return n && *n ? n : "Key " + std::to_string(b.code);
        }
        case Binding::Kind::PadButton:
            switch (SDL_GamepadButton(b.code)) {
                case SDL_GAMEPAD_BUTTON_SOUTH: return "A Button";
                case SDL_GAMEPAD_BUTTON_EAST: return "B Button";
                case SDL_GAMEPAD_BUTTON_WEST: return "X Button";
                case SDL_GAMEPAD_BUTTON_NORTH: return "Y Button";
                case SDL_GAMEPAD_BUTTON_BACK: return "Back Button";
                case SDL_GAMEPAD_BUTTON_GUIDE: return "Home Button";
                case SDL_GAMEPAD_BUTTON_START: return "Start Button";
                case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Press";
                case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Press";
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "Left Bumper";
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "Right Bumper";
                case SDL_GAMEPAD_BUTTON_DPAD_UP: return "Up D-Pad";
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "Down D-Pad";
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "Left D-Pad";
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "Right D-Pad";
                case SDL_GAMEPAD_BUTTON_MISC1: return "Share Button";
                case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Right Paddle 1";
                case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Left Paddle 1";
                case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Right Paddle 2";
                case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Left Paddle 2";
                case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Press";
                default: return "Controller Button " + std::to_string(b.code);
            }
        case Binding::Kind::PadAxis: {
            const bool neg = b.dir < 0;
            switch (SDL_GamepadAxis(b.code)) {
                case SDL_GAMEPAD_AXIS_LEFTX: return neg ? "Left Stick Left" : "Left Stick Right";
                case SDL_GAMEPAD_AXIS_LEFTY: return neg ? "Left Stick Up" : "Left Stick Down";
                case SDL_GAMEPAD_AXIS_RIGHTX: return neg ? "Right Stick Left" : "Right Stick Right";
                case SDL_GAMEPAD_AXIS_RIGHTY: return neg ? "Right Stick Up" : "Right Stick Down";
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return "Left Trigger";
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return "Right Trigger";
                default: return "Controller Axis " + std::to_string(b.code) + (neg ? "-" : "+");
            }
        }
    }
    return "?";
}

namespace {
bool parse(const std::string& text, Binding& out, bool hotkey);
}

bool from_string(const std::string& text, Binding& out) { return parse(text, out, false); }

namespace {
bool parse(const std::string& text, Binding& out, bool hotkey) {
    const std::string s = trim(text);
    auto colon = s.find(':');
    if (colon == std::string::npos) return false;
    const std::string kind = lower(trim(s.substr(0, colon)));
    const std::string value = trim(s.substr(colon + 1));
    if (kind == "key") {
        SDL_Scancode sc = SDL_GetScancodeFromName(value.c_str());
        if (sc == SDL_SCANCODE_UNKNOWN) {
            char* end = nullptr;
            long n = std::strtol(value.c_str(), &end, 10);
            if (end && *end == 0 && n > 0 && n < SDL_SCANCODE_COUNT) sc = SDL_Scancode(n);
        }
        if (sc == SDL_SCANCODE_UNKNOWN || (hotkey ? reserved_hotkey_key(sc) : reserved_key(sc))) return false;
        out = key(sc);
        return true;
    }
    if (kind == "pad") {
        SDL_GamepadButton b = SDL_GetGamepadButtonFromString(lower(value).c_str());
        if (b == SDL_GAMEPAD_BUTTON_INVALID) return false;
        out = pad(b);
        return true;
    }
    if (kind == "axis") {
        if (value.size() < 2) return false;
        const char sign = value.back();
        if (sign != '+' && sign != '-') return false;
        SDL_GamepadAxis a = SDL_GetGamepadAxisFromString(lower(value.substr(0, value.size() - 1)).c_str());
        if (a == SDL_GAMEPAD_AXIS_INVALID) return false;
        out = axis(a, sign == '-' ? -1 : 1);
        return true;
    }
    return false;
}
}  // namespace

const char* name(Hotkey h) {
    switch (h) {
        case Hotkey::SaveState: return "Save state";
        case Hotkey::SaveNewSlot: return "Save to new slot";
        case Hotkey::LoadState: return "Load state";
        case Hotkey::SlotPrev: return "Previous slot";
        case Hotkey::SlotNext: return "Next slot";
        default: return "?";
    }
}

const char* key_name(Hotkey h) {
    switch (h) {
        case Hotkey::SaveState: return "save_state";
        case Hotkey::SaveNewSlot: return "save_new_slot";
        case Hotkey::LoadState: return "load_state";
        case Hotkey::SlotPrev: return "slot_prev";
        case Hotkey::SlotNext: return "slot_next";
        default: return "?";
    }
}

std::string to_string(const Chord& c) {
    std::string s;
    for (const auto& b : c) {
        if (!s.empty()) s += "+";
        s += to_string(b);
    }
    return s;
}

bool chord_from_string(const std::string& text, Chord& out) {
    // Split before each "+key:" / "+pad:" so key names containing '+' ("Keypad +") survive.
    const std::string s = trim(text);
    Chord c;
    size_t p = 0;
    while (p < s.size()) {
        size_t next = std::string::npos;
        for (const char* tag : {"+key:", "+pad:", "+axis:"}) {
            size_t f = lower(s).find(tag, p + 1);
            if (f != std::string::npos) next = std::min(next, f);
        }
        Binding b;
        if (!parse(s.substr(p, next == std::string::npos ? std::string::npos : next - p), b, true)) return false;
        if (std::find(c.begin(), c.end(), b) == c.end()) c.push_back(b);
        if (next == std::string::npos) break;
        p = next + 1;
    }
    // One key on its own, or one or two controller buttons.
    if (c.empty() || c.size() > 2) return false;
    for (const auto& b : c) {
        if (b.kind == Binding::Kind::PadAxis) return false;
        if (b.kind == Binding::Kind::Key && c.size() != 1) return false;
    }
    out = std::move(c);
    return true;
}

std::string join_chords(const std::vector<Chord>& list) {
    if (list.empty()) return "none";
    std::string s;
    for (const auto& c : list) {
        if (!s.empty()) s += " | ";
        s += to_string(c);
    }
    return s;
}

std::vector<Chord> split_chords(const std::string& text) {
    std::vector<Chord> out;
    size_t p = 0;
    while (p <= text.size()) {
        size_t bar = text.find(" | ", p);
        std::string item = trim(text.substr(p, bar == std::string::npos ? std::string::npos : bar - p));
        Chord c;
        if (!item.empty() && chord_from_string(item, c) && std::find(out.begin(), out.end(), c) == out.end())
            out.push_back(std::move(c));
        if (bar == std::string::npos) break;
        p = bar + 3;
    }
    return out;
}

Config default_config() {
    Config c;
    auto& b = c.buttons;
    auto at = [&](Button x) -> std::vector<Binding>& { return b[int(x)]; };
    at(Button::A) = {key(SDL_SCANCODE_X), pad(SDL_GAMEPAD_BUTTON_SOUTH), pad(SDL_GAMEPAD_BUTTON_NORTH)};
    at(Button::B) = {key(SDL_SCANCODE_Z), pad(SDL_GAMEPAD_BUTTON_EAST), pad(SDL_GAMEPAD_BUTTON_WEST)};
    at(Button::Select) = {key(SDL_SCANCODE_BACKSPACE), key(SDL_SCANCODE_RSHIFT), pad(SDL_GAMEPAD_BUTTON_BACK)};
    at(Button::Start) = {key(SDL_SCANCODE_RETURN), pad(SDL_GAMEPAD_BUTTON_START)};
    at(Button::Right) = {key(SDL_SCANCODE_RIGHT), pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT), axis(SDL_GAMEPAD_AXIS_LEFTX, 1)};
    at(Button::Left) = {key(SDL_SCANCODE_LEFT), pad(SDL_GAMEPAD_BUTTON_DPAD_LEFT), axis(SDL_GAMEPAD_AXIS_LEFTX, -1)};
    at(Button::Up) = {key(SDL_SCANCODE_UP), pad(SDL_GAMEPAD_BUTTON_DPAD_UP), axis(SDL_GAMEPAD_AXIS_LEFTY, -1)};
    at(Button::Down) = {key(SDL_SCANCODE_DOWN), pad(SDL_GAMEPAD_BUTTON_DPAD_DOWN), axis(SDL_GAMEPAD_AXIS_LEFTY, 1)};
    at(Button::R) = {key(SDL_SCANCODE_S), pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1)};
    at(Button::L) = {key(SDL_SCANCODE_A), pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1)};
    // Shortcuts: F-keys, and Back (Select) held with a shoulder or D-pad side on a controller.
    auto hk = [&](Hotkey h) -> std::vector<Chord>& { return c.hotkeys[int(h)]; };
    const Binding back = pad(SDL_GAMEPAD_BUTTON_BACK);
    hk(Hotkey::SaveState) = {{key(SDL_SCANCODE_F5)}, {back, pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)}};
    hk(Hotkey::SaveNewSlot) = {{key(SDL_SCANCODE_F9)}, {back, pad(SDL_GAMEPAD_BUTTON_DPAD_UP)}};
    hk(Hotkey::LoadState) = {{key(SDL_SCANCODE_F8)}, {back, pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)}};
    hk(Hotkey::SlotPrev) = {{key(SDL_SCANCODE_F6)}, {back, pad(SDL_GAMEPAD_BUTTON_DPAD_LEFT)}};
    hk(Hotkey::SlotNext) = {{key(SDL_SCANCODE_F7)}, {back, pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)}};
    return c;
}

std::string join(const std::vector<Binding>& list) {
    if (list.empty()) return "none";
    std::string s;
    for (const auto& b : list) {
        if (!s.empty()) s += " | ";
        s += to_string(b);
    }
    return s;
}

std::vector<Binding> split(const std::string& text) {
    // Split on " | " (with spaces) so a key literally named "|" could never be misread.
    std::vector<Binding> out;
    size_t p = 0;
    while (p <= text.size()) {
        size_t bar = text.find(" | ", p);
        std::string item = trim(text.substr(p, bar == std::string::npos ? std::string::npos : bar - p));
        Binding b;
        if (!item.empty() && from_string(item, b) && std::find(out.begin(), out.end(), b) == out.end())
            out.push_back(b);
        if (bar == std::string::npos) break;
        p = bar + 3;
    }
    return out;
}

std::string turbo_buttons_to_string(std::uint16_t buttons) {
    std::string out;
    for (int i = 0; i < kButtonCount; ++i) {
        if (!(buttons & (1u << i))) continue;
        if (!out.empty()) out += ", ";
        out += name(Button(i));
    }
    return out.empty() ? "none" : out;
}

std::uint16_t turbo_buttons_from_string(const std::string& text) {
    std::uint16_t out = 0;
    std::size_t p = 0;
    while (p <= text.size()) {
        std::size_t comma = text.find(',', p);
        if (comma == std::string::npos) comma = text.size();
        const std::string item = lower(trim(text.substr(p, comma - p)));
        for (int i = 0; i < kButtonCount; ++i)
            if (item == lower(name(Button(i)))) out |= std::uint16_t(1u << i);
        p = comma + 1;
    }
    return out;
}

Config load(const SettingsIO& io) {
    Config c = default_config();
    if (!io.get) return c;
    c.deadzone_percent = std::clamp(std::atoi(io.get("input.deadzone", "40").c_str()), 5, 95);
    for (int i = 0; i < kButtonCount; ++i) {
        const std::string v = trim(io.get(std::string("input.") + name(Button(i)), ""));
        if (v.empty()) continue;  // keep default
        if (lower(v) == "none") c.buttons[i].clear();
        else if (auto list = split(v); !list.empty()) c.buttons[i] = std::move(list);
    }
    {
        const std::string on = lower(trim(io.get("input.turbo", "")));
        if (on == "1" || on == "true" || on == "yes" || on == "on") c.turbo.enabled = true;
        else if (on == "0" || on == "false" || on == "no" || on == "off") c.turbo.enabled = false;
        const std::string rate = trim(io.get("input.turbo.rate", ""));
        if (!rate.empty())
            c.turbo.rate = std::clamp(std::atoi(rate.c_str()), kTurboMinRate, kTurboMaxRate);
        const std::string buttons = trim(io.get("input.turbo.buttons", ""));
        if (!buttons.empty()) c.turbo.buttons = turbo_buttons_from_string(buttons);
    }
    for (int i = 0; i < kHotkeyCount; ++i) {
        const std::string v = trim(io.get(std::string("input.hotkey.") + key_name(Hotkey(i)), ""));
        if (v.empty()) continue;
        if (lower(v) == "none") c.hotkeys[i].clear();
        else if (auto list = split_chords(v); !list.empty()) c.hotkeys[i] = std::move(list);
    }
    return c;
}

void store(const Config& c, const SettingsIO& io) {
    if (!io.set) return;
    io.set("input.deadzone", std::to_string(c.deadzone_percent));
    io.set("input.turbo", c.turbo.enabled ? "true" : "false");
    io.set("input.turbo.rate", std::to_string(std::clamp(c.turbo.rate, kTurboMinRate, kTurboMaxRate)));
    io.set("input.turbo.buttons", turbo_buttons_to_string(c.turbo.buttons));
    for (Button b : kRemapOrder) io.set(std::string("input.") + name(b), join(c.buttons[int(b)]));
    for (int i = 0; i < kHotkeyCount; ++i)
        io.set(std::string("input.hotkey.") + key_name(Hotkey(i)), join_chords(c.hotkeys[i]));
}

// ---------------------------------------------------------------------------------------

Manager::Manager(SettingsIO io) : io_(std::move(io)) {
    config_ = load(io_);
    // First run (or keys added since): write the full control map so users can edit settings.ini by hand.
    if (io_.get && io_.set) {
        const std::string missing = "\x01";
        bool complete = io_.get("input.deadzone", missing) != missing &&
                        io_.get("input.turbo", missing) != missing &&
                        io_.get("input.turbo.rate", missing) != missing &&
                        io_.get("input.turbo.buttons", missing) != missing;
        for (int i = 0; complete && i < kButtonCount; ++i)
            complete = io_.get(std::string("input.") + name(Button(i)), missing) != missing;
        for (int i = 0; complete && i < kHotkeyCount; ++i)
            complete = io_.get(std::string("input.hotkey.") + key_name(Hotkey(i)), missing) != missing;
        if (!complete) save();
    }
    // Pads connected before this object existed already sent their ADDED events.
    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; ++i)
            if (SDL_Gamepad* p = SDL_OpenGamepad(ids[i])) pads_.push_back(p);
        SDL_free(ids);
    }
}

Manager::~Manager() {
    for (auto* p : pads_) SDL_CloseGamepad(p);
}

void Manager::save() const {
    store(config_, io_);
    if (io_.save) io_.save();
}

std::uint16_t Manager::poll() const {
    if (remapping()) return 0;
    const bool* k = SDL_GetKeyboardState(nullptr);
    const int threshold = 32767 * config_.deadzone_percent / 100;
    // Buttons that fired a shortcut stay away from the game until every pad releases them.
    std::erase_if(suppressed_, [&](int code) {
        for (auto* p : pads_)
            if (SDL_GetGamepadButton(p, SDL_GamepadButton(code))) return false;
        return true;
    });
    std::uint16_t mask = 0;
    for (int i = 0; i < kButtonCount; ++i) {
        bool down = false;
        for (const auto& b : config_.buttons[i]) {
            if (b.kind == Binding::Kind::Key) {
                down = k && k[b.code];
            } else {
                for (auto* p : pads_) {
                    if (b.kind == Binding::Kind::PadButton) {
                        if (std::find(suppressed_.begin(), suppressed_.end(), b.code) != suppressed_.end()) break;
                        down = SDL_GetGamepadButton(p, SDL_GamepadButton(b.code));
                    } else {
                        const int v = SDL_GetGamepadAxis(p, SDL_GamepadAxis(b.code));
                        down = b.dir < 0 ? v < -threshold : v > threshold;
                    }
                    if (down) break;
                }
            }
            if (down) break;
        }
        if (down) mask |= std::uint16_t(1u << i);
    }
    // Opposite directions cannot be pressed at once on real hardware.
    auto bit = [](Button b) { return std::uint16_t(1u << int(b)); };
    if ((mask & bit(Button::Left)) && (mask & bit(Button::Right))) mask &= ~(bit(Button::Left) | bit(Button::Right));
    if ((mask & bit(Button::Up)) && (mask & bit(Button::Down))) mask &= ~(bit(Button::Up) | bit(Button::Down));
    return mask;
}

std::uint16_t Manager::poll_frame() { return turbo_.step(poll(), config_.turbo); }

void Manager::start_remap(Button only) {
    start_remap();
    for (int i = 0; i < int(std::size(kRemapOrder)); ++i)
        if (kRemapOrder[i] == only) remap_index_ = i;
    remap_single_ = true;
    message_.clear();
}

Button Manager::remap_target() const { return remapping() ? kRemapOrder[remap_index_] : Button::Count; }

void Manager::start_remap(Hotkey only) {
    if (only == Hotkey::Count) return;
    remap_index_ = -1;
    remap_single_ = false;
    hotkey_remap_ = only;
    capture_.clear();
    message_.clear();
}

void Manager::clear(Hotkey h) {
    if (h == Hotkey::Count) return;
    config_.hotkeys[int(h)].clear();
    save();
    message_ = std::string(name(h)) + " unbound";
}

void Manager::cancel_remap() {
    if (!remapping()) return;
    remap_index_ = -1;
    remap_single_ = false;
    hotkey_remap_ = Hotkey::Count;
    capture_.clear();
    config_ = load(io_);  // drop unsaved changes
    message_ = "Remap cancelled";
}

void Manager::reload() {
    if (!remapping()) config_ = load(io_);
}

void Manager::start_remap() {
    remap_index_ = 0;
    remap_single_ = false;
    const int threshold = 32767 * config_.deadzone_percent / 100;
    for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a) {
        bool centered = true;
        for (auto* p : pads_)
            if (std::abs(int(SDL_GetGamepadAxis(p, SDL_GamepadAxis(a)))) > threshold) centered = false;
        axis_armed_[a] = centered;
    }
    message_ = "Rebinding controls. Esc keeps the current binding.";
}

std::string Manager::remap_prompt() const {
    if (!remapping()) return {};
    if (hotkey_remap_ != Hotkey::Count)
        return std::string("Press a key or controller button(s) for ") + name(hotkey_remap_) +
               "  (now: " + describe(hotkey_remap_) + ")";
    const Button b = kRemapOrder[remap_index_];
    return std::string("Press a key or pad button for ") + name(b) + "  (now: " + describe(b) + ")";
}

void Manager::assign(const Binding& bind) {
    const Button target = kRemapOrder[remap_index_];
    // An input drives one GBA button only, and a key is either a button or a shortcut.
    for (auto& list : config_.buttons) list.erase(std::remove(list.begin(), list.end(), bind), list.end());
    for (auto& list : config_.hotkeys) list.erase(std::remove(list.begin(), list.end(), Chord{bind}), list.end());
    // Replace this button's bindings of the same device type (keyboard vs gamepad).
    auto& list = config_.buttons[int(target)];
    const bool is_key = bind.kind == Binding::Kind::Key;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Binding& b) { return (b.kind == Binding::Kind::Key) == is_key; }),
               list.end());
    list.insert(list.begin(), bind);
    advance();
}

void Manager::advance() {
    ++remap_index_;
    if (remap_single_ || remap_index_ >= int(std::size(kRemapOrder))) {
        remap_index_ = -1;
        remap_single_ = false;
        save();
        message_ = "Controls saved";
    }
}

void Manager::assign_hotkey(const Chord& chord) {
    const Hotkey target = hotkey_remap_;
    hotkey_remap_ = Hotkey::Count;
    capture_.clear();
    if (target == Hotkey::Count || chord.empty()) return;
    // A chord triggers one shortcut, and a key used for a shortcut stops driving a GBA button.
    for (auto& list : config_.hotkeys) list.erase(std::remove(list.begin(), list.end(), chord), list.end());
    const bool is_key = chord.front().kind == Binding::Kind::Key;
    if (is_key)
        for (auto& list : config_.buttons)
            list.erase(std::remove(list.begin(), list.end(), chord.front()), list.end());
    // Replace this shortcut's binding of the same device type, like the button rebinds do.
    auto& list = config_.hotkeys[int(target)];
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Chord& c) { return (c.front().kind == Binding::Kind::Key) == is_key; }),
               list.end());
    list.insert(list.begin(), chord);
    save();
    message_ = std::string(name(target)) + ": " + describe(target);
}

void Manager::fire(Hotkey h) {
    switch (h) {
        case Hotkey::SaveState: states::quick_save(); break;
        case Hotkey::SaveNewSlot: states::save_new_slot(); break;
        case Hotkey::LoadState: states::quick_load(); break;
        case Hotkey::SlotPrev: states::slot_step(-1); break;
        case Hotkey::SlotNext: states::slot_step(+1); break;
        default: break;
    }
}

void Manager::reset_to_defaults() {
    remap_index_ = -1;
    hotkey_remap_ = Hotkey::Count;
    capture_.clear();
    config_ = default_config();
    save();
    message_ = "Controls reset to defaults";
}

std::string Manager::describe(Button b) const {
    std::string s;
    for (const auto& bind : config_.buttons[int(b)]) {
        if (!s.empty()) s += ", ";
        s += display_name(bind);
    }
    return s.empty() ? "None" : s;
}

std::string Manager::describe(Hotkey h) const {
    std::string s;
    for (const auto& c : config_.hotkeys[int(h)]) {
        if (!s.empty()) s += ", ";
        std::string part;
        for (const auto& b : c) part += (part.empty() ? "" : " + ") + display_name(b);  // "Back Button + Right Bumper"
        s += part;
    }
    return s.empty() ? "None" : s;
}

std::string Manager::take_message() {
    std::string m;
    m.swap(message_);
    return m;
}

bool Manager::handle_event(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            for (auto* p : pads_)
                if (SDL_GetGamepadID(p) == e.gdevice.which) return false;  // already open
            if (SDL_Gamepad* p = SDL_OpenGamepad(e.gdevice.which)) {
                pads_.push_back(p);
                const char* n = SDL_GetGamepadName(p);
                message_ = std::string("Controller connected: ") + (n ? n : "gamepad");
            }
            return false;
        case SDL_EVENT_GAMEPAD_REMOVED:
            for (auto it = pads_.begin(); it != pads_.end(); ++it)
                if (SDL_GetGamepadID(*it) == e.gdevice.which) {
                    SDL_CloseGamepad(*it);
                    pads_.erase(it);
                    message_ = "Controller disconnected";
                    break;
                }
            return false;
        default: break;
    }
    if (hotkey_remap_ != Hotkey::Count) {
        switch (e.type) {
            case SDL_EVENT_KEY_DOWN: {
                if (e.key.repeat) return true;
                const SDL_Scancode sc = e.key.scancode;
                if (sc == SDL_SCANCODE_ESCAPE) {
                    hotkey_remap_ = Hotkey::Count;  // keep the current binding
                    capture_.clear();
                } else if (!reserved_hotkey_key(sc)) {
                    assign_hotkey({key(sc)});
                }
                return true;
            }
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                return true;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                const Binding b = pad(SDL_GamepadButton(e.gbutton.button));
                if (capture_.size() < 2 && std::find(capture_.begin(), capture_.end(), b) == capture_.end())
                    capture_.push_back(b);
                return true;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                // The chord is whatever was held together before the first release.
                if (!capture_.empty()) {
                    for (const auto& b : capture_) suppressed_.push_back(b.code);
                    assign_hotkey(capture_);
                }
                return true;
            default:
                return false;
        }
    }

    if (!remapping()) {
        if (!shortcuts_enabled_) return false;
        // Shortcuts. A controller chord fires when its last button goes down; the longest
        // matching chord wins so Back+R beats a lone Back.
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            for (int h = 0; h < kHotkeyCount; ++h)
                for (const auto& c : config_.hotkeys[h])
                    if (c.size() == 1 && c[0].kind == Binding::Kind::Key && c[0].code == int(e.key.scancode)) {
                        fire(Hotkey(h));
                        return true;
                    }
        } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_Gamepad* p = SDL_GetGamepadFromID(e.gbutton.which);
            int best = -1;
            const Chord* best_chord = nullptr;
            for (int h = 0; h < kHotkeyCount && p; ++h)
                for (const auto& c : config_.hotkeys[h]) {
                    bool has = false, all = true;
                    for (const auto& b : c) {
                        if (b.kind != Binding::Kind::PadButton) { all = false; break; }
                        if (b.code == int(e.gbutton.button)) has = true;
                        if (!SDL_GetGamepadButton(p, SDL_GamepadButton(b.code))) all = false;
                    }
                    if (has && all && (!best_chord || c.size() > best_chord->size())) {
                        best = h;
                        best_chord = &c;
                    }
                }
            if (best_chord) {
                for (const auto& b : *best_chord)
                    if (std::find(suppressed_.begin(), suppressed_.end(), b.code) == suppressed_.end())
                        suppressed_.push_back(b.code);
                fire(Hotkey(best));
                return true;
            }
        }
        return false;
    }

    const int threshold = 32767 * config_.deadzone_percent / 100;
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN: {
            if (e.key.repeat) return true;
            const SDL_Scancode sc = e.key.scancode;
            if (sc == SDL_SCANCODE_ESCAPE) {
                advance();
            } else if (!reserved_key(sc)) {
                assign(key(sc));
            }
            return true;
        }
        case SDL_EVENT_KEY_UP:
            return true;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            assign(pad(SDL_GamepadButton(e.gbutton.button)));
            return true;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const int a = e.gaxis.axis, v = e.gaxis.value;
            if (a < 0 || a >= SDL_GAMEPAD_AXIS_COUNT) return true;
            if (std::abs(v) < threshold / 2) {
                axis_armed_[a] = true;
            } else if (axis_armed_[a] && std::abs(v) > threshold) {
                axis_armed_[a] = false;  // must return to center before it can bind again
                assign(axis(SDL_GamepadAxis(a), v < 0 ? -1 : 1));
            }
            return true;
        }
        default:
            return false;
    }
}

}  // namespace knid::input
