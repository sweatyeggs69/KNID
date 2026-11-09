#pragma once
// Importing and exporting battery saves from other emulators, plus save backups.
// Kirby: Nightmare in Dream Land uses 32 KiB of SRAM (SRAM_V112). No SDL dependency.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace knid {

inline constexpr std::size_t kSramSize = 32 * 1024;

struct NormalizedSave {
    bool ok = false;
    std::string error;          // user-facing reason when !ok
    std::string format;         // what was recognized, e.g. "raw 32 KiB SRAM", "64 KiB (padded)"
    std::string warning;        // non-fatal note (e.g. the save is empty)
    std::vector<std::uint8_t> data;  // exactly kSramSize bytes when ok
};

// Converts a save file from mGBA, VBA-M, RetroArch (.srm), NO$GBA (raw), gpSP-style padded
// saves or DeSmuME (.dsv) into raw 32 KiB SRAM. Rejects save states and other save types
// with a clear message. `filename` is only used to recognize the format (extension).
NormalizedSave normalize_save(const std::vector<std::uint8_t>& file, const std::string& filename);

// True if the SRAM starts with the header this game writes on first boot.
bool has_kirby_header(const std::vector<std::uint8_t>& sram);

// Copies `save` to <backups>/<stem>-YYYYmmdd-HHMMSS.sav and keeps the newest `keep` backups for
// that stem. Returns the backup path, or empty if there was nothing to back up / it failed.
std::filesystem::path backup_save(const std::filesystem::path& save, const std::filesystem::path& backups,
                                  std::size_t keep = 10);
// Backups for `stem`, newest first.
std::vector<std::filesystem::path> list_backups(const std::filesystem::path& backups, const std::string& stem);

bool read_file(const std::filesystem::path& p, std::vector<std::uint8_t>& out);
bool write_file_atomic(const std::filesystem::path& p, const std::vector<std::uint8_t>& data);

}  // namespace knid
