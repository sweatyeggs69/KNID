#pragma once
// Windowed application: first-run ROM setup, themed launcher, options, shortcuts and the game.

#include <filesystem>
#include <string>

namespace knid {

struct AppOptions {
    std::filesystem::path rom;          // play this ROM directly (developer/CLI use)
    std::filesystem::path bios;         // override bios.path from the settings
    std::filesystem::path pref_dir;     // override the settings/saves folder (testing)
    std::filesystem::path select_rom;   // feed this path to the ROM picker (testing, skips the dialog)
    std::filesystem::path dump_frame;   // write the last game frame to a BMP on exit
    std::filesystem::path screenshot;   // save the window (as shown) on the last --frames frame
    bool play = false;                  // --play: skip the launcher, start the configured ROM
    bool test_pattern = false;
    bool verbose = false;
    bool open_settings = false;         // open the settings menu at start (testing)
    long frames = -1;                   // quit after this many frames
    std::string input_script;           // "frame:hexkeys,..." merged with real input in game
};

int run_app(const AppOptions& options);

}  // namespace knid
