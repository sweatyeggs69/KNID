// Rumble: which changes in blank RAM images turn into controller pulses. No ROM needed.
#include <array>
#include <cstdint>
#include <cstdio>

#include "app/rumble.h"

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
using Iwram = std::array<std::uint8_t, 0x8000>;
using Ewram = std::array<std::uint8_t, 0x40000>;
template <typename M> void set16(M& m, std::size_t at, std::uint16_t v) { m[at] = std::uint8_t(v); m[at + 1] = std::uint8_t(v >> 8); }
constexpr std::size_t ew(unsigned a) { return a - 0x02000000u; }
constexpr std::size_t iw(unsigned a) { return a - 0x03000000u; }
}  // namespace

int main() {
    namespace rumble = knid::rumble;
    namespace cheats = knid::cheats;
    Iwram iwram{};
    Ewram ram{};
    rumble::Watcher w;

    // Not in a level (maximum 0): health changes mean nothing.
    set16(ram, ew(cheats::kHealth), 0x30);
    CHECK(!w.step(iwram, ram));
    set16(ram, ew(cheats::kHealth), 0x10);
    CHECK(!w.step(iwram, ram));

    // In a level: 6 bars (0x30), 3 lives.
    set16(ram, ew(cheats::kMaxHealth), 0x30);
    set16(ram, ew(cheats::kHealth), 0x30);
    ram[ew(cheats::kLives)] = 3;
    w.step(iwram, ram);
    CHECK(!w.step(iwram, ram));

    // A hit for one bar.
    set16(ram, ew(cheats::kHealth), 0x28);
    rumble::Pulse p = w.step(iwram, ram);
    CHECK(p && p.low >= 0.45f && p.strength() < 1.0f);

    // Healing never rumbles.
    set16(ram, ew(cheats::kHealth), 0x30);
    CHECK(!w.step(iwram, ram));

    // Health runs out: death; the life count dropping a bit later does not rumble again.
    set16(ram, ew(cheats::kHealth), 0);
    p = w.step(iwram, ram);
    CHECK(p && p.strength() == 1.0f);
    for (int i = 0; i < 60; ++i) w.step(iwram, ram);
    ram[ew(cheats::kLives)] = 2;
    CHECK(!w.step(iwram, ram));

    // Much later, a fall into a pit: only the life count drops.
    set16(ram, ew(cheats::kHealth), 0x30);
    for (int i = 0; i < 400; ++i) w.step(iwram, ram);
    ram[ew(cheats::kLives)] = 1;
    p = w.step(iwram, ram);
    CHECK(p && p.strength() == 1.0f);

    // reset(): a loaded state with less health is not a hit.
    w.reset();
    set16(ram, ew(cheats::kHealth), 0x08);
    CHECK(!w.step(iwram, ram));

    // Mixer: a weaker pulse never cuts a stronger one short; it plays again once that is over.
    rumble::Mixer mix;
    CHECK(mix.take({1.0f, 1.0f, 100}));
    mix.step();
    CHECK(!mix.take({0.0f, 0.35f, 70}));
    for (int i = 0; i < 10; ++i) mix.step();
    CHECK(!mix.playing());
    CHECK(mix.take({0.0f, 0.35f, 70}));
    CHECK(!mix.take({}));

    // Strength scaling.
    CHECK(rumble::motor(1.0f, 100) == 0xFFFF);
    CHECK(rumble::motor(1.0f, 50) == 0x8000);
    CHECK(rumble::motor(0.0f, 100) == 0);
    CHECK(rumble::motor(2.0f, 100) == 0xFFFF);

    if (failures) return 1;
    std::puts("rumble: ok");
    return 0;
}
