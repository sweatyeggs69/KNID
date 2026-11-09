#include "options/input.h"

#include <algorithm>
#include <cstdlib>

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

bool from_string(const std::string& text, Binding& out) {
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
        if (sc == SDL_SCANCODE_UNKNOWN || reserved_key(sc)) return false;
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
    return c;
}

void store(const Config& c, const SettingsIO& io) {
    if (!io.set) return;
    io.set("input.deadzone", std::to_string(c.deadzone_percent));
    for (Button b : kRemapOrder) io.set(std::string("input.") + name(b), join(c.buttons[int(b)]));
}

// ---------------------------------------------------------------------------------------

Manager::Manager(SettingsIO io) : io_(std::move(io)) {
    config_ = load(io_);
    // First run (or keys added since): write the full control map so users can edit settings.ini by hand.
    if (io_.get && io_.set) {
        const std::string missing = "\x01";
        bool complete = io_.get("input.deadzone", missing) != missing;
        for (int i = 0; complete && i < kButtonCount; ++i)
            complete = io_.get(std::string("input.") + name(Button(i)), missing) != missing;
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
    std::uint16_t mask = 0;
    for (int i = 0; i < kButtonCount; ++i) {
        bool down = false;
        for (const auto& b : config_.buttons[i]) {
            if (b.kind == Binding::Kind::Key) {
                down = k && k[b.code];
            } else {
                for (auto* p : pads_) {
                    if (b.kind == Binding::Kind::PadButton) {
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

void Manager::start_remap(Button only) {
    start_remap();
    for (int i = 0; i < int(std::size(kRemapOrder)); ++i)
        if (kRemapOrder[i] == only) remap_index_ = i;
    remap_single_ = true;
    message_.clear();
}

Button Manager::remap_target() const { return remapping() ? kRemapOrder[remap_index_] : Button::Count; }

void Manager::cancel_remap() {
    if (!remapping()) return;
    remap_index_ = -1;
    remap_single_ = false;
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
    message_ = "Remapping controls. Esc keeps the current binding, F9 cancels.";
}

std::string Manager::remap_prompt() const {
    if (!remapping()) return {};
    const Button b = kRemapOrder[remap_index_];
    return std::string("Press a key or pad button for ") + name(b) + "  (now: " + describe(b) + ")";
}

void Manager::assign(const Binding& bind) {
    const Button target = kRemapOrder[remap_index_];
    // An input drives one GBA button only.
    for (auto& list : config_.buttons) list.erase(std::remove(list.begin(), list.end(), bind), list.end());
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

void Manager::reset_to_defaults() {
    remap_index_ = -1;
    config_ = default_config();
    save();
    message_ = "Controls reset to defaults";
}

std::string Manager::describe(Button b) const {
    std::string s;
    for (const auto& bind : config_.buttons[int(b)]) {
        if (!s.empty()) s += ", ";
        s += to_string(bind);
    }
    return s.empty() ? "none" : s;
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
    if (!remapping()) return false;

    const int threshold = 32767 * config_.deadzone_percent / 100;
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN: {
            if (e.key.repeat) return true;
            const SDL_Scancode sc = e.key.scancode;
            if (sc == SDL_SCANCODE_ESCAPE) {
                advance();
            } else if (sc == SDL_SCANCODE_F9) {
                cancel_remap();
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
