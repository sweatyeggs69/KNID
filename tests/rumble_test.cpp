// Rumble: which changes in blank RAM images turn into controller rumble patterns. No ROM needed.
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
    rumble::Pattern p = w.step(iwram, ram);
    CHECK(p && p.peak_low() >= 0.45f && p.strength() < 1.0f);

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

    // Power moves: B pressed with Hammer swings, held B does not repeat, no ability does nothing.
    {
        constexpr std::uint16_t B = rumble::kKeyB;
        iwram[iw(cheats::kAbility)] = rumble::kHammer;
        w.step(iwram, ram, 0);
        p = w.step(iwram, ram, B);
        CHECK(p && p.steps[0].low == 0 && p.peak_low() == 1.0f && p.priority < rumble::patterns::kHitPriority);
        CHECK(!w.step(iwram, ram, B));  // still held
        w.step(iwram, ram, 0);
        iwram[iw(cheats::kAbility)] = rumble::kStone;
        w.step(iwram, ram, 0);
        p = w.step(iwram, ram, B);
        CHECK(p && p.peak_low() > 0.5f);
        w.step(iwram, ram, 0);
        iwram[iw(cheats::kAbility)] = 0;  // no ability: B inhales, no rumble
        w.step(iwram, ram, 0);
        CHECK(!w.step(iwram, ram, B));
        w.step(iwram, ram, 0);
        // A hit on the same frame as a swing: the hit wins.
        iwram[iw(cheats::kAbility)] = rumble::kHammer;
        w.step(iwram, ram, 0);
        set16(ram, ew(cheats::kHealth), 0x20);
        p = w.step(iwram, ram, B);
        CHECK(p && p.priority >= rumble::patterns::kHitPriority);
        iwram[iw(cheats::kAbility)] = 0;
    }

    // reset(): a loaded state with less health is not a hit.
    w.reset();
    set16(ram, ew(cheats::kHealth), 0x08);
    CHECK(!w.step(iwram, ram));

    // Player: steps through a pattern a frame at a time, and a lower priority pattern never cuts a
    // higher one short; it plays again once that is over.
    {
        rumble::Player pl;
        CHECK(pl.start(rumble::patterns::kDeath));
        auto s = pl.next();
        CHECK(s && s->low == 1.0f && s->high == 1.0f && s->ms > 250);  // first beat, with overlap
        CHECK(!pl.start(rumble::patterns::kMissile));
        int commands = 1, frames = 0;
        bool paused = false;
        while (pl.playing() && frames < 200) {
            if (auto c = pl.next()) {
                ++commands;
                if (c->low == 0 && c->high == 0 && pl.playing()) paused = true;
            }
            ++frames;
        }
        CHECK(paused);                          // the pauses between beats stop the motors
        CHECK(commands == 7);                   // six steps, then the final stop
        CHECK(frames >= 75 && frames <= 90);    // about 1.4 s of game frames
        CHECK(!pl.playing());
        CHECK(pl.start(rumble::patterns::kMissile));
        CHECK(pl.start(rumble::patterns::kDeath));  // a death interrupts anything
        CHECK(!pl.start({}));
    }

    // The pattern table: different events feel different.
    {
        namespace pt = rumble::patterns;
        CHECK(pt::kMissile.peak_low() == 0 && pt::kMissile.peak_high() > 0);  // small motor only
        CHECK(pt::kDeath.total_ms() > pt::kHeavyHit.total_ms());
        CHECK(pt::kPowerBomb.strength() == 1.0f && pt::kPowerBomb.steps[0].low < 0.5f);  // builds up
        const rumble::Pattern scratch = rumble::hit(1, 100), big = rumble::hit(40, 100);
        CHECK(scratch.count == 1 && big.count == 2 && big.strength() > scratch.strength());
        const rumble::Pattern sh = rumble::shake(600, 0.8f);
        CHECK(sh.count > 4 && sh.total_ms() == 600 && sh.priority == pt::kShakePriority);
        CHECK(rumble::shake(5000, 1.0f).total_ms() == 1200);
    }

    // Strength scaling.
    CHECK(rumble::motor(1.0f, 100) == 0xFFFF);
    CHECK(rumble::motor(1.0f, 50) == 0x8000);
    CHECK(rumble::motor(0.0f, 100) == 0);
    CHECK(rumble::motor(2.0f, 100) == 0xFFFF);

    if (failures) return 1;
    std::puts("rumble: ok");
    return 0;
}
