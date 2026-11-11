// Cheats: writes Kirby's health, lives, invincibility and ability into blank work RAM images.
// No ROM needed.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "app/cheats.h"

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
std::size_t ew(std::uint32_t a) { return a - 0x02000000u; }
std::size_t iw(std::uint32_t a) { return a - 0x03000000u; }
std::uint16_t get16(const std::vector<std::uint8_t>& m, std::size_t at) { return std::uint16_t(m[at] | (m[at + 1] << 8)); }
void set16(std::vector<std::uint8_t>& m, std::size_t at, std::uint16_t v) { m[at] = std::uint8_t(v); m[at + 1] = std::uint8_t(v >> 8); }
}  // namespace

int main() {
    namespace cheats = knid::cheats;
    using cheats::Flags;
    std::vector<std::uint8_t> ewram(0x40000), iwram(0x8000);
    const Flags all{true, true, true, 0x01};

    // Outside a level (no health maximum): nothing is written, so menus and saves are untouched.
    CHECK(!cheats::apply(ewram, iwram, Flags{}));
    CHECK(!cheats::apply(ewram, iwram, all));
    CHECK(ewram[ew(cheats::kLives)] == 0 && iwram[iw(cheats::kAbility)] == 0 && iwram[iw(cheats::kHitState)] == 0);

    // In a level: 6 bars max, 5 left, 2 lives, no ability.
    set16(ewram, ew(cheats::kMaxHealth), 0x30);
    set16(ewram, ew(cheats::kHealth), 0x28);
    ewram[ew(cheats::kLives)] = 2;
    CHECK(!cheats::apply(ewram, iwram, Flags{}));
    CHECK(cheats::apply(ewram, iwram, Flags{true, false, false, 0}));
    CHECK(get16(ewram, ew(cheats::kHealth)) == 0x30);
    CHECK(ewram[ew(cheats::kLives)] == 2);

    CHECK(cheats::apply(ewram, iwram, Flags{false, true, false, 0}));
    CHECK(ewram[ew(cheats::kLives)] == 9);
    ewram[ew(cheats::kLives)] = 12;  // a 1-up above 9 is kept
    CHECK(!cheats::apply(ewram, iwram, Flags{false, true, false, 0}));
    CHECK(ewram[ew(cheats::kLives)] == 12);

    CHECK(cheats::apply(ewram, iwram, Flags{false, false, true, 0}));
    CHECK(iwram[iw(cheats::kHitState)] == 1 && get16(iwram, iw(cheats::kHitTimer)) == 0x0A);
    set16(iwram, iw(cheats::kHitTimer), 3);  // still counting down: left alone
    CHECK(!cheats::apply(ewram, iwram, Flags{false, false, true, 0}));
    CHECK(get16(iwram, iw(cheats::kHitTimer)) == 3);

    CHECK(cheats::apply(ewram, iwram, Flags{false, false, false, 0x09}));
    CHECK(iwram[iw(cheats::kAbility)] == 0x09);
    CHECK(!cheats::apply(ewram, iwram, Flags{false, false, false, 0x09}));

    CHECK(cheats::known_ability(0x01) && !cheats::known_ability(0x00) && !cheats::known_ability(0x1B));

    if (failures) std::fprintf(stderr, "cheats: %d check(s) failed\n", failures);
    else std::puts("cheats: all checks passed");
    return failures ? 1 : 0;
}
