// Widescreen support specific to Kirby: Nightmare in Dream Land (USA, A7KE).
//
// The game keeps the whole level layout in RAM, but only copies the part around the
// 240-pixel view into BG2's 512x256 VRAM tilemap (a ring buffer). For the extra columns of
// a widescreen view we rebuild BG2 tilemap entries straight from the level layout, the same
// way the game's own tile writer does (sub_802AFC8 in the disassembly):
//
//   meta  = u16 at *(u32*)0x03005660 + ((ty >> 1) * width + (tx >> 1)) * 4
//   width = s16 at 0x03005620 (metatiles), height = s16 at 0x0300561C (metatiles)
//   entry = u16 at 0x0200B080 + (meta * 4 + (tx & 1) + (ty & 1) * 2) * 2
//
// BG2 HOFS/VOFS only hold the camera position modulo 512/256, so once per frame we find
// which absolute camera position matches what is actually in VRAM, by comparing sample
// tiles of the visible area. If nothing matches (another game mode, a cutscene, a level
// that is not built this way) the feature stays off and the PPU uses VRAM as before.
#include "gba.h"

namespace knid::gba {

namespace {
constexpr u32 kMapPtr = 0x03005660, kWidth = 0x03005620, kHeight = 0x0300561C, kTiles = 0x0200B080;
}

u16 Gba::wide_level_entry(s32 tx, s32 ty) const {
    auto ram16 = [&](u32 a) -> u16 {
        if ((a >> 24) == 0x02) return u16(ewram_[a & 0x3FFFF] | (ewram_[(a + 1) & 0x3FFFF] << 8));
        if ((a >> 24) == 0x03) return u16(iwram_[a & 0x7FFF] | (iwram_[(a + 1) & 0x7FFF] << 8));
        return 0;
    };
    const u32 meta = ram16(wide_level_.map + u32(((ty >> 1) * wide_level_.width + (tx >> 1)) * 4));
    return ram16(kTiles + (meta * 4 + u32(tx & 1) + u32(ty & 1) * 2) * 2);
}

void Gba::wide_level_detect() {
    wide_level_.valid = false;
    if (!wide_margin_) return;
    const u16 dispcnt = u16(io_[0] | (io_[1] << 8));
    const u16 cnt = u16(io_[0x0C] | (io_[0x0D] << 8));  // BG2CNT
    if ((dispcnt & 7) > 1 || !(dispcnt & 0x400) || ((cnt >> 14) & 3) != 1) return;  // BG2 on, 512x256

    auto iw16 = [&](u32 a) { return s16(iwram_[a & 0x7FFF] | (iwram_[(a + 1) & 0x7FFF] << 8)); };
    const u32 map = u32(iwram_[kMapPtr & 0x7FFF]) | u32(iwram_[(kMapPtr + 1) & 0x7FFF]) << 8 |
                    u32(iwram_[(kMapPtr + 2) & 0x7FFF]) << 16 | u32(iwram_[(kMapPtr + 3) & 0x7FFF]) << 24;
    const s32 w = iw16(kWidth), h = iw16(kHeight);
    if ((map >> 24) != 0x02 || w <= 0 || h <= 0 || w > 1024 || h > 1024) return;
    if ((map & 0x3FFFF) + u32(w * h * 4) > 0x40000) return;
    wide_level_.map = map;
    wide_level_.width = w;
    wide_level_.height = h;

    const u32 hofs = u32(io_[0x18] | (io_[0x19] << 8)) & 0x1FF;
    const u32 vofs = u32(io_[0x1A] | (io_[0x1B] << 8)) & 0x1FF;
    const u32 screen_base = ((cnt >> 8) & 31) * 0x800;
    const s32 level_w = w * 16, level_h = h * 16;
    // Sample tiles spread over the visible area. The best-matching camera wins if at least
    // 80% of its samples agree (a few tiles may differ, e.g. blocks the player broke).
    static constexpr int kSx[] = {4, 36, 68, 100, 132, 164, 196, 228};
    static constexpr int kSy[] = {12, 60, 108, 148};
    int best = -1, best_checked = 0;
    s32 best_x = 0, best_y = 0;
    for (s32 cy = s32(vofs & 0xFF); cy < level_h; cy += 256) {
        for (s32 cx = s32(hofs); cx < level_w; cx += 512) {
            int checked = 0, match = 0;
            for (int sy : kSy) {
                for (int sx : kSx) {
                    const s32 ax = cx + sx, ay = cy + sy;
                    if (ax >= level_w || ay >= level_h) continue;
                    const u32 vx = u32(ax >> 3) & 63, vy = u32(ay >> 3) & 31;
                    const u32 off = screen_base + (vx >> 5) * 0x800 + vy * 64 + (vx & 31) * 2;
                    const u16 vram_e = u16(vram_[off] | (vram_[off + 1] << 8));
                    ++checked;
                    if (vram_e == wide_level_entry(ax >> 3, ay >> 3)) ++match;
                }
            }
            if (checked >= 16 && match * 5 >= checked * 4 && match > best) {
                best = match;
                best_checked = checked;
                best_x = cx;
                best_y = cy;
            }
        }
    }
    (void)best_checked;
    if (best < 0) return;
    wide_level_.cam_x = best_x - s32(hofs);
    wide_level_.cam_y = best_y - s32(vofs);
    wide_level_.valid = true;
}

}  // namespace knid::gba
