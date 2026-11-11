#pragma once
// Cheats menu (F1 > Cheats). After every game frame the host calls apply() on the game's work RAM.
// Nothing is written until a level is loaded (Kirby's maximum health is 0 on the title screen,
// file select and game select), so the cheats never touch menus or save data. Off while
// RetroAchievements hardcore mode is on.
//
// Addresses (Kirby: Nightmare in Dream Land, USA), each checked against the running game:
//   EWRAM 0x02005580 s16  Kirby's maximum health (0x30 = 6 bars, 8 per bar; 0 outside levels)
//   EWRAM 0x02005588 s16  Kirby's health
//   EWRAM 0x02007D48 u8   lives
//   IWRAM 0x0300217D u8   copy ability (0 = none)
//   IWRAM 0x03002182 u16  post-hit invulnerability timer
//   IWRAM 0x030021AF u8   hit state; with the timer above, enemies and hazards pass through Kirby
//
// Settings (knid::Config): cheats.health, cheats.lives, cheats.invincible (off by default) and
// cheats.ability (an ability id from kAbilities, 0 = off).
#include <cstddef>
#include <cstdint>
#include <span>

namespace knid::cheats {

inline constexpr std::uint32_t kMaxHealth = 0x02005580;
inline constexpr std::uint32_t kHealth = 0x02005588;
inline constexpr std::uint32_t kLives = 0x02007D48;
inline constexpr std::uint32_t kAbility = 0x0300217D;
inline constexpr std::uint32_t kHitTimer = 0x03002182;
inline constexpr std::uint32_t kHitState = 0x030021AF;

inline constexpr std::uint8_t kLivesValue = 9;

struct Ability {
    std::uint8_t id;
    const char* name;
};
// Ability ids confirmed in game. Ids not listed are other powers or glitchy states.
inline constexpr Ability kAbilities[] = {
    {0x01, "Fire"},   {0x02, "Spark"},  {0x03, "Cutter"}, {0x07, "Ball"},   {0x09, "Hammer"},
    {0x0A, "Parasol"}, {0x0C, "Needle"}, {0x10, "Beam"},  {0x11, "Stone"},  {0x13, "Tornado"},
};
inline bool known_ability(int id) {
    for (const Ability& a : kAbilities)
        if (a.id == id) return true;
    return false;
}

struct Flags {
    bool health = false;
    bool lives = false;
    bool invincible = false;
    std::uint8_t ability = 0;  // 0 = off
    bool any() const { return health || lives || invincible || ability != 0; }
};

namespace detail {
inline std::uint16_t get16(std::span<const std::uint8_t> m, std::size_t at) {
    return std::uint16_t(m[at] | (m[at + 1] << 8));
}
inline void set16(std::span<std::uint8_t> m, std::size_t at, std::uint16_t v) {
    m[at] = std::uint8_t(v);
    m[at + 1] = std::uint8_t(v >> 8);
}
inline std::size_t ew(std::uint32_t a) { return a - 0x02000000u; }
inline std::size_t iw(std::uint32_t a) { return a - 0x03000000u; }
}  // namespace detail

// True while a level is loaded (Kirby has a health maximum).
inline bool in_level(std::span<const std::uint8_t> ewram) {
    using namespace detail;
    if (ewram.size() < ew(kLives) + 1) return false;
    const auto max = std::int16_t(get16(ewram, ew(kMaxHealth)));
    return max > 0 && max <= 0x100;
}

// Applies the enabled cheats to ewram (256 KiB from 0x02000000) and iwram (32 KiB from
// 0x03000000). Returns true when anything changed.
inline bool apply(std::span<std::uint8_t> ewram, std::span<std::uint8_t> iwram, const Flags& f) {
    using namespace detail;
    if (!f.any() || iwram.size() < iw(kHitState) + 1 || !in_level(ewram)) return false;
    bool changed = false;
    auto put8 = [&](std::span<std::uint8_t> m, std::size_t at, std::uint8_t v) {
        if (m[at] != v) {
            m[at] = v;
            changed = true;
        }
    };
    if (f.health) {
        const std::uint16_t max = get16(ewram, ew(kMaxHealth));
        if (get16(ewram, ew(kHealth)) != max) {
            set16(ewram, ew(kHealth), max);
            changed = true;
        }
    }
    // Lives stay at 9 or more: a 1-up above 9 is kept.
    if (f.lives && ewram[ew(kLives)] < kLivesValue) put8(ewram, ew(kLives), kLivesValue);
    if (f.invincible) {
        put8(iwram, iw(kHitState), 1);
        if (get16(iwram, iw(kHitTimer)) == 0) {
            set16(iwram, iw(kHitTimer), 0x0A);
            changed = true;
        }
    }
    if (f.ability != 0) put8(iwram, iw(kAbility), f.ability);
    return changed;
}

}  // namespace knid::cheats
