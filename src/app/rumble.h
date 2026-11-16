#pragma once
// Controller rumble (Settings > Controls > Rumble). The GBA has no rumble, so Watcher reads the
// game's RAM after every frame and turns what just happened into a Pulse; the host plays it on
// every connected controller (input::Manager::rumble), scaled by the strength setting.
//
// Events, read from Kirby: Nightmare in Dream Land's RAM (the addresses the cheats use):
//   - Kirby takes damage: EWRAM 0x02005588 s16 health drops while a level is loaded (maximum,
//     EWRAM 0x02005580, unchanged). Stronger for bigger hits.
//   - Kirby loses a life: health reaches 0, or the lives count (EWRAM 0x02007D48) drops without
//     that (a fall into a pit).
//
// Settings (knid::Config): input.rumble (on by default), input.rumble_strength (10-100,
// default 75).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "app/cheats.h"

namespace knid::rumble {

inline constexpr bool kDefaultOn = true;
inline constexpr int kDefaultStrength = 75;
inline constexpr int kMinStrength = 10;
// Shown next to the Controller rumble toggle.
inline constexpr const char* kEventsHelp =
    "Shakes your controller when Kirby is hurt, harder for bigger hits, and when Kirby loses a "
    "life. Works with controllers that have rumble motors.";

// One rumble: motor strengths 0..1 (low = the big, slow motor, high = the small, fast one).
struct Pulse {
    float low = 0, high = 0;
    int ms = 0;
    explicit operator bool() const { return ms > 0 && (low > 0 || high > 0); }
    float strength() const { return std::max(low, high); }
};

// The stronger of two pulses; on a tie the longer one.
inline Pulse stronger(const Pulse& a, const Pulse& b) {
    if (!a) return b;
    if (!b) return a;
    if (a.strength() != b.strength()) return a.strength() > b.strength() ? a : b;
    return a.ms >= b.ms ? a : b;
}

// Decides which pulses reach the controllers: a new pulse replaces the one still playing only
// when it is at least as strong, so a light tick never cuts a big hit short. Counts time in
// game frames (step once per emulated frame).
class Mixer {
public:
    // True when p should be played now.
    bool take(const Pulse& p) {
        if (!p) return false;
        if (left_ms_ > 0 && p.strength() < playing_.strength()) return false;
        playing_ = p;
        left_ms_ = p.ms;
        return true;
    }
    void step() { left_ms_ = std::max(0, left_ms_ - kFrameMs); }
    bool playing() const { return left_ms_ > 0; }
    void clear() { left_ms_ = 0; }

private:
    static constexpr int kFrameMs = 17;  // one GBA frame (16.74 ms)
    Pulse playing_;
    int left_ms_ = 0;
};

// Strength setting (percent) -> motor value for SDL (0..0xFFFF).
inline std::uint16_t motor(float level, int strength_percent) {
    const float v = std::clamp(level, 0.0f, 1.0f) * std::clamp(strength_percent, 0, 100) / 100.0f;
    return std::uint16_t(v * 65535.0f + 0.5f);
}

// A hit that took `lost` of `max` health: stronger and longer for bigger hits.
inline Pulse hit(int lost, int max) {
    const float share = max > 0 ? float(lost) / float(max) : 0.0f;
    const float f = std::min(1.0f, 0.45f + share * 2.0f);
    return {f, f * 0.6f, 120 + int(std::min(share, 0.5f) * 400.0f)};
}
inline constexpr Pulse kDeath{1.0f, 1.0f, 700};

namespace detail {
inline std::uint16_t get16(std::span<const std::uint8_t> m, std::size_t at) {
    return std::uint16_t(m[at] | (m[at + 1] << 8));
}
}  // namespace detail

// ---- Kirby: Nightmare in Dream Land ----

class Watcher {
public:
    // Forgets the last frame, so a jump in the numbers (a loaded state, rewind, a new game) is
    // not taken for an event.
    void reset() { primed_ = false; }

    // Call after each emulated frame with the 32 KiB IWRAM (0x03000000) and 256 KiB EWRAM.
    Pulse step(std::span<const std::uint8_t> /*iwram*/, std::span<const std::uint8_t> ewram) {
        using detail::get16;
        if (ewram.size() < cheats::kLives - 0x02000000u + 1) return {};
        Frame now;
        now.health = std::int16_t(get16(ewram, cheats::kHealth - 0x02000000u));
        now.max_health = std::int16_t(get16(ewram, cheats::kMaxHealth - 0x02000000u));
        now.lives = ewram[cheats::kLives - 0x02000000u];
        now.in_level = cheats::in_level(ewram);

        const Frame before = last_;
        const bool primed = primed_;
        last_ = now;
        primed_ = true;
        if (since_death_ < 1000) ++since_death_;
        if (!primed) return {};

        Pulse out;
        if (now.in_level && before.in_level && now.max_health == before.max_health &&
            now.health < before.health && before.health > 0) {
            if (now.health <= 0) {
                out = kDeath;
                since_death_ = 0;
            } else {
                out = hit(before.health - now.health, now.max_health);
            }
        }
        // A life lost without the health running out first (a pit): one death rumble, not two.
        if (now.lives + 1 == before.lives && since_death_ > 300) {
            out = stronger(out, kDeath);
            since_death_ = 0;
        }
        return out;
    }

private:
    struct Frame {
        std::int16_t health = 0, max_health = 0;
        std::uint8_t lives = 0;
        bool in_level = false;
    };
    Frame last_;
    bool primed_ = false;
    int since_death_ = 1000;  // frames since the last death rumble
};

}  // namespace knid::rumble
