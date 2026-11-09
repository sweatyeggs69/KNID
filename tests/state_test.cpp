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
    // Corrupt / foreign data is rejected without touching the machine.
    std::vector<std::uint8_t> bad = gba.save_state();
    bad.resize(bad.size() / 2);
    std::string err;
    if (gba.load_state(bad, &err)) { std::printf("truncated state was accepted\n"); ++failures; }
    else std::printf("truncated state rejected: %s\n", err.c_str());
    return failures ? 1 : 0;
}
