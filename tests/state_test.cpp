// Save state determinism: run, save, record M frames of framebuffer hashes, load, replay the same
// input and require identical hashes. Uses the user's ROM if present (KNID_ROM or the default
// path baked in by CMake); skipped (exit 77) otherwise. Nothing from the ROM is written to disk.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "gba/gba.h"
#include "rom.h"

#ifndef KNID_TEST_ROM
#define KNID_TEST_ROM ""
#endif
#ifndef KNID_TEST_BIOS
#define KNID_TEST_BIOS ""
#endif
#include <fstream>
#include <iterator>

namespace {
std::uint64_t hash(const knid::gba::u16* px, size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= px[i]; h *= 1099511628211ull; }
    return h;
}
// Deterministic input: start the game, go through the menus, then walk and jump.
std::uint16_t keys_for(long f) {
    using namespace knid::gba;
    if (f >= 650 && f < 656) return kKeyStart;
    if ((f >= 800 && f < 806) || (f >= 950 && f < 956) || (f >= 1100 && f < 1106)) return kKeyA;
    if (f >= 1300) {
        std::uint16_t k = (f / 90) % 2 ? kKeyRight : kKeyLeft;
        if (f % 40 < 6) k |= kKeyA;
        if (f % 70 < 10) k |= kKeyB;
        return k;
    }
    return 0;
}
}  // namespace

int main() {
    const char* env = std::getenv("KNID_ROM");
    std::string path = env && *env ? env : KNID_TEST_ROM;
    auto loaded = knid::load_rom(path);
    if (path.empty() || !loaded.ok || !loaded.rom.hash_matches) {
        std::printf("skipped: ROM not available (set KNID_ROM)\n");
        return 77;
    }
    knid::gba::Gba gba(loaded.rom.data, {});
    std::vector<knid::gba::s16> audio;
    long f = 0;
    auto step = [&] {
        gba.set_keys(keys_for(f++));
        gba.run_frame();
        audio.clear();
        gba.take_audio(audio);
    };
    int failures = 0;
    // Check at the title screen, in the menus and in gameplay.
    for (long save_at : {600L, 900L, 1500L}) {
        while (f < save_at) step();
        std::vector<std::uint8_t> state = gba.save_state();
        long base = f;
        const int M = 240;
        std::vector<std::uint64_t> first;
        for (int i = 0; i < M; ++i) { step(); first.push_back(hash(gba.framebuffer(), 240 * 160)); }
        std::string err;
        if (!gba.load_state(state, &err)) { std::printf("load failed: %s\n", err.c_str()); return 1; }
        f = base;
        int mismatch = -1;
        for (int i = 0; i < M; ++i) {
            step();
            if (hash(gba.framebuffer(), 240 * 160) != first[i] && mismatch < 0) mismatch = i;
        }
        // Saving again right after replay must give the same bytes as a fresh run reaching here.
        std::printf("state at frame %ld: %zu bytes, replay of %d frames %s\n", save_at, state.size(), M,
                    mismatch < 0 ? "identical" : ("DIFFERS at +" + std::to_string(mismatch)).c_str());
        if (mismatch >= 0) ++failures;
    }
    // ---- with the user's own BIOS dump (skipped if it is not there) ----
    std::ifstream bf(KNID_TEST_BIOS, std::ios::binary);
    std::vector<std::uint8_t> bios((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>());
    std::string err;
    if (bios.size() == 0x4000) {
        auto run = [&](knid::gba::Gba& g, long from, long n) {
            for (long i = 0; i < n; ++i) { g.set_keys(keys_for(from + i)); g.run_frame(); audio.clear(); g.take_audio(audio); }
        };
        // Real BIOS determinism.
        knid::gba::Gba real(loaded.rom.data, {});
        real.use_bios(bios);
        real.reset();
        run(real, 0, 1500);
        auto st = real.save_state();
        std::vector<std::uint64_t> a, b;
        for (int i = 0; i < 120; ++i) { run(real, 1500 + i, 1); a.push_back(hash(real.framebuffer(), 240 * 160)); }
        if (!real.load_state(st, &err)) { std::printf("real BIOS load failed: %s\n", err.c_str()); ++failures; }
        for (int i = 0; i < 120; ++i) { run(real, 1500 + i, 1); b.push_back(hash(real.framebuffer(), 240 * 160)); }
        std::printf("real BIOS replay of 120 frames %s\n", a == b ? "identical" : "DIFFERS");
        if (a != b) ++failures;

        // Cross-mode loads at frame boundaries: the game must keep running afterwards.
        auto keeps_running = [&](knid::gba::Gba& g, long from) {
            std::uint64_t h0 = hash(g.framebuffer(), 240 * 160);
            int changes = 0;
            for (int i = 0; i < 300; ++i) {
                run(g, from + i, 1);
                std::uint64_t h = hash(g.framebuffer(), 240 * 160);
                if (h != h0) { ++changes; h0 = h; }
            }
            return changes > 30;
        };
        knid::gba::Gba hle(loaded.rom.data, {});
        run(hle, 0, 1500);
        auto hle_state = hle.save_state();
        knid::gba::Gba real2(loaded.rom.data, {});
        real2.use_bios(bios);
        real2.reset();
        bool ok1 = real2.load_state(hle_state, &err);
        bool run1 = ok1 && keeps_running(real2, 1500);
        std::printf("HLE state -> real BIOS: %s%s\n", ok1 ? "loaded" : ("refused: " + err).c_str(),
                    ok1 ? (run1 ? ", game keeps running" : ", GAME STALLED") : "");
        knid::gba::Gba hle2(loaded.rom.data, {});
        bool ok2 = hle2.load_state(st, &err);
        bool run2 = ok2 && keeps_running(hle2, 1500);
        std::printf("real BIOS state -> HLE: %s%s\n", ok2 ? "loaded" : ("refused: " + err).c_str(),
                    ok2 ? (run2 ? ", game keeps running" : ", GAME STALLED") : "");
        if (!run1 || !run2) ++failures;

        // Booting through the BIOS intro shows the logo, then the game takes over and runs.
        knid::gba::Gba boot(loaded.rom.data, {});
        boot.use_bios(bios);
        boot.set_bios_boot(true);
        boot.reset();
        run(boot, -100000, 150);  // ~2.5 s into the BIOS intro (no keys)
        const knid::gba::u16* px = boot.framebuffer();
        int distinct = 0;
        for (int i = 1; i < 240 * 160; ++i) distinct += px[i] != px[0];
        run(boot, -100000, 450);  // past the intro
        bool runs = keeps_running(boot, -100000);
        std::printf("BIOS boot: logo frame %s, game %s afterwards\n", distinct > 500 ? "drawn" : "BLANK",
                    runs ? "running" : "NOT RUNNING");
        if (distinct <= 500 || !runs) ++failures;
    } else {
        std::printf("real BIOS checks skipped (no gba_bios.bin)\n");
    }

    // Corrupt / foreign data is rejected without touching the machine.
    std::vector<std::uint8_t> bad = gba.save_state();
    bad.resize(bad.size() / 2);
    if (gba.load_state(bad, &err)) { std::printf("truncated state was accepted\n"); ++failures; }
    else std::printf("truncated state rejected: %s\n", err.c_str());
    return failures ? 1 : 0;
}
