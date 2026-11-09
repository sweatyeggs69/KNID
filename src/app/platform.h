#pragma once
// Platform helpers for the launcher: ROM verification, save location and shortcuts.

#include <filesystem>
#include <string>

#include "rom.h"

namespace knid {

class Config;

struct VerifiedRom {
    bool ok = false;
    std::string error;  // user-facing reason when !ok
    std::filesystem::path path;
    Rom rom;
};

// Loads and verifies a ROM file (must be the expected Kirby: Nightmare in Dream Land dump).
VerifiedRom verify_rom(const std::filesystem::path& path);

// <pref dir>/saves/<sha1>.sav. On first use, copies an existing "<rom>.sav" next to the ROM
// (the location older builds used) so progress carries over; the original is left in place.
std::filesystem::path save_path_for(const Config& config, const VerifiedRom& rom);

// Absolute path of the running executable.
std::filesystem::path executable_path();

enum class ShortcutPlace { Applications, Desktop };
struct ShortcutResult {
    bool ok = false;
    std::string message;
    std::filesystem::path path;
};
// Creates a launcher that starts this app with --play.
//   macOS:  "<place>/Kirby Nightmare in Dream Land.app" (a tiny bundle with a shell script)
//   Linux:  a .desktop entry
//   Windows: not yet implemented
ShortcutResult create_shortcut(ShortcutPlace place);

}  // namespace knid
