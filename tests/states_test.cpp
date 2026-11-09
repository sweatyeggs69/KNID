// Save state slots without the game: Store save/load/rename/remove on a blank machine, auto-load
// decision, and fallback when the Auto state is unusable. No ROM or game data needed.
#include <cstdio>
#include <filesystem>
#include <memory>
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
    // Startup decision table: rom_ok, --play, autoload, auto exists, auto usable, shift.
    CHECK(choose_startup(true, false, false, true, true, false) == Startup::Launcher);
    CHECK(choose_startup(true, true, false, true, true, false) == Startup::BootGame);       // --play only
    CHECK(choose_startup(true, false, true, true, true, false) == Startup::ResumeAuto);     // autoload
    CHECK(choose_startup(true, true, true, true, true, false) == Startup::ResumeAuto);      // shortcut + autoload
    CHECK(choose_startup(true, false, true, false, false, false) == Startup::BootGame);     // no Auto state yet
    CHECK(choose_startup(true, false, true, true, false, false) == Startup::LauncherWithError);  // unusable
    CHECK(choose_startup(true, false, true, true, true, true) == Startup::Launcher);        // Shift bypass
    CHECK(choose_startup(false, false, true, true, true, false) == Startup::LauncherWithError);  // bad ROM
    CHECK(choose_startup(false, false, false, false, false, false) == Startup::Launcher);

    fs::path dir = fs::temp_directory_path() / "knid_states_test";
    fs::remove_all(dir);
    const std::string sha = "0123456789abcdef0123456789abcdef01234567";
    auto machine_heap = std::make_unique<knid::gba::Gba>(std::vector<std::uint8_t>(), std::filesystem::path{});  // >500 KB: never on the stack
    auto& machine = *machine_heap;  // blank machine, nothing executed
    {
        Store store(dir / "states", sha, dir / "saves" / "x.sav");
        CHECK(!store.info(kAutoSlot).exists);
        std::string err;
        CHECK(store.save(kAutoSlot, machine, &err));
        CHECK(store.save(3, machine, &err, "boss"));
        CHECK(store.info(kAutoSlot).exists && store.info(kAutoSlot).error.empty());
        CHECK(store.info(3).label == "boss");
        CHECK(store.rename(3, "before boss", &err) && store.info(3).label == "before boss");
        auto other_heap = std::make_unique<knid::gba::Gba>(std::vector<std::uint8_t>(), std::filesystem::path{});  // >500 KB: never on the stack
        auto& other = *other_heap;
        CHECK(store.load(kAutoSlot, other, &err));
        CHECK(store.can_undo_load());
    }
    {
        // Same files seen from another ROM: refused, so auto-load falls back to a normal boot.
        Store foreign(dir / "states", "ffffffffffffffffffffffffffffffffffffffff", dir / "saves" / "x.sav");
        CHECK(!foreign.info(kAutoSlot).error.empty());
        bool usable = foreign.info(kAutoSlot).exists && foreign.info(kAutoSlot).error.empty();
        CHECK(choose_startup(true, false, true, foreign.info(kAutoSlot).exists, usable, false) ==
              Startup::LauncherWithError);
    }
    {
        // A damaged Auto state is reported as unusable.
        Store store(dir / "states", sha, dir / "saves" / "x.sav");
        std::ofstream(store.file(kAutoSlot), std::ios::binary | std::ios::trunc) << "KNIDSTAT garbage";
        store.refresh();
        CHECK(store.info(kAutoSlot).exists && !store.info(kAutoSlot).error.empty());
        auto m2_heap = std::make_unique<knid::gba::Gba>(std::vector<std::uint8_t>(), std::filesystem::path{});  // >500 KB: never on the stack
        auto& m2 = *m2_heap;
        std::string err;
        CHECK(!store.load(kAutoSlot, m2, &err) && !err.empty());
    }
    fs::remove_all(dir);
    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    std::printf("states tests passed\n");
    return 0;
}
