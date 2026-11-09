// Save state slots without the game: Store save/load/rename/remove on a blank machine, auto-load
// decision, and fallback when the Auto state is unusable. No ROM or game data needed.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/states.h"
#include "gba/gba.h"

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
}  // namespace

int main() {
    using namespace knid::states;
    namespace fs = std::filesystem;
    // Decision table.
    CHECK(choose_start(true, true, false) == StartMode::ResumeAuto);
    CHECK(choose_start(true, true, true) == StartMode::TitleScreen);    // Shift bypass
    CHECK(choose_start(true, false, false) == StartMode::TitleScreen);  // no usable Auto state
    CHECK(choose_start(false, true, false) == StartMode::TitleScreen);  // option off

    fs::path dir = fs::temp_directory_path() / "knid_states_test";
    fs::remove_all(dir);
    const std::string sha = "0123456789abcdef0123456789abcdef01234567";
    knid::gba::Gba machine(std::vector<std::uint8_t>(), {});  // blank machine, nothing executed
    {
        Store store(dir / "states", sha, dir / "saves" / "x.sav");
        CHECK(!store.info(kAutoSlot).exists);
        std::string err;
        CHECK(store.save(kAutoSlot, machine, &err));
        CHECK(store.save(3, machine, &err, "boss"));
        CHECK(store.info(kAutoSlot).exists && store.info(kAutoSlot).error.empty());
        CHECK(store.info(3).label == "boss");
        CHECK(store.rename(3, "before boss", &err) && store.info(3).label == "before boss");
        knid::gba::Gba other(std::vector<std::uint8_t>(), {});
        CHECK(store.load(kAutoSlot, other, &err));
        CHECK(store.can_undo_load());
    }
    {
        // Same files seen from another ROM: refused, so auto-load falls back to a normal boot.
        Store foreign(dir / "states", "ffffffffffffffffffffffffffffffffffffffff", dir / "saves" / "x.sav");
        CHECK(!foreign.info(kAutoSlot).error.empty());
        bool usable = foreign.info(kAutoSlot).exists && foreign.info(kAutoSlot).error.empty();
        CHECK(choose_start(true, usable, false) == StartMode::TitleScreen);
    }
    {
        // A damaged Auto state is reported as unusable.
        Store store(dir / "states", sha, dir / "saves" / "x.sav");
        std::ofstream(store.file(kAutoSlot), std::ios::binary | std::ios::trunc) << "KNIDSTAT garbage";
        store.refresh();
        CHECK(store.info(kAutoSlot).exists && !store.info(kAutoSlot).error.empty());
        knid::gba::Gba m2(std::vector<std::uint8_t>(), {});
        std::string err;
        CHECK(!store.load(kAutoSlot, m2, &err) && !err.empty());
    }
    fs::remove_all(dir);
    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    std::printf("states tests passed\n");
    return 0;
}
