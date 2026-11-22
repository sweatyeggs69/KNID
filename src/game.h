#pragma once
// Identifiers of the game KNID runs, used by the ROM check. Nothing here is game data: these are
// only header values and a fingerprint of the user's own ROM.
//
// Target: Kirby: Nightmare in Dream Land, USA release (AGB-A7KE), revision 0.

#include <cstddef>

namespace knid::game {

inline constexpr const char* kTitle = "Kirby: Nightmare in Dream Land";
inline constexpr const char* kRegion = "USA";
inline constexpr const char* kTitleWithRegion = "Kirby: Nightmare in Dream Land (USA)";

// GBA cartridge header of the supported dump.
inline constexpr const char* kHeaderTitle = "AGB KIRBY DX";  // 0xA0, 12 bytes
inline constexpr const char* kGameCode = "A7KE";             // 0xAC, 4 bytes
inline constexpr const char* kMakerCode = "01";             // 0xB0, 2 bytes (Nintendo)
inline constexpr unsigned kRevision = 0;                     // 0xBC
inline constexpr std::size_t kRomSize = 8 * 1024 * 1024;

// SHA-1 of the known-good dump (lowercase hex).
inline constexpr const char* kExpectedSha1 = "37a476567d133c146fee6b5e2eb0b07a215da6b0";

// Other releases of the game, recognised only so the error message can say what the file is.
// None of them can run: the recompiled code is the USA revision 0 code.
struct OtherRelease {
    const char* sha1;       // exact dump, or nullptr to match on the game code alone
    const char* game_code;  // 0xAC
    const char* name;       // shown to the user
};
inline constexpr OtherRelease kOtherReleases[] = {
    {nullptr, "A7KP", "a European release"},
    {nullptr, "A7KJ", "a Japanese release"},
};

}  // namespace knid::game
