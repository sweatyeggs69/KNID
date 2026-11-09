#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace knid {

// Known-good dump of Kirby: Nightmare in Dream Land (USA), game code A7KE, rev 0.
// This is only a fingerprint of the user's own dump; the ROM itself is never shipped.
inline constexpr const char* kExpectedSha1 = "37a476567d133c146fee6b5e2eb0b07a215da6b0";
inline constexpr std::size_t kExpectedSize = 8 * 1024 * 1024;

struct RomHeader {
    std::string title;      // 0xA0, 12 bytes
    std::string game_code;  // 0xAC, 4 bytes
    std::string maker;      // 0xB0, 2 bytes
    std::uint8_t version = 0;          // 0xBC
    std::uint8_t checksum = 0;         // 0xBD
    bool checksum_ok = false;
};

struct Rom {
    std::vector<std::uint8_t> data;
    RomHeader header;
    std::string sha1;
    bool hash_matches = false;
};

struct RomLoadResult {
    bool ok = false;
    std::string error;
    Rom rom;
};

// Reads the file, parses the GBA cartridge header and hashes it.
// ok is true only when the file is a valid GBA ROM; check rom.hash_matches
// to know whether it is the exact dump this project targets.
RomLoadResult load_rom(const std::filesystem::path& path);

// Looks for a ROM next to the executable, in the working directory and in ./rom.
std::filesystem::path find_default_rom(const std::filesystem::path& exe_dir);

}  // namespace knid
