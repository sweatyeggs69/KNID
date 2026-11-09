// Widescreen support specific to Kirby: Nightmare in Dream Land (USA, A7KE).
//
// The game keeps whole level layers in RAM, but only copies the part around the 240-pixel
// view into the 512x256 VRAM tilemaps of BG2 (foreground) and BG3 (background), which act as
// ring buffers. For the extra columns of a widescreen view we rebuild tilemap entries
// straight from RAM, the same way the game's own tile writers do:
//
// BG2 (sub_802AFC8): 16x16 metatiles
//   meta  = u16 at *(u32*)0x03005660 + ((ty >> 1) * width + (tx >> 1)) * 4
//   width = s16 at 0x03005620, height = s16 at 0x0300561C (metatiles)
//   entry = u16 at 0x0200B080 + (meta * 4 + (tx & 1) + (ty & 1) * 2) * 2
// BG3 (sub_802B030): plain tilemap
//   layer = *(u32*)(*(u32*)0x030055EC + 0x30); width = u16 at layer + 2 (tiles),
//   height = u16 at layer + 4; entry = u16 at layer + 6 + (ty * width + tx) * 2
//
// HOFS/VOFS only hold the camera modulo 512/256, so once per frame we find which absolute
// camera position matches what is actually in VRAM by comparing sample tiles of the visible
// area. If nothing matches (title, menus, cutscenes, another layout) that layer falls back
// to plain VRAM rendering.
#include "gba.h"

namespace knid::gba {

namespace {
constexpr u32 kMapPtr = 0x03005660, kWidth = 0x03005620, kHeight = 0x0300561C, kTiles = 0x0200B080;
constexpr u32 kBgStruct = 0x030055EC;
}  // namespace

u16 Gba::wide_ram16(u32 a) const {
    if ((a >> 24) == 0x02) return u16(ewram_[a & 0x3FFFF] | (ewram_[(a + 1) & 0x3FFFF] << 8));
    if ((a >> 24) == 0x03) return u16(iwram_[a & 0x7FFF] | (iwram_[(a + 1) & 0x7FFF] << 8));
    return 0;
}

u32 Gba::wide_ram32(u32 a) const { return u32(wide_ram16(a)) | (u32(wide_ram16(a + 2)) << 16); }

u16 Gba::wide_level_entry(int layer, s32 tx, s32 ty) const {
    const WideLevel& L = wide_level_[layer];
    if (layer == 0) {
        const u32 meta = wide_ram16(L.map + u32(((ty >> 1) * L.width + (tx >> 1)) * 4));
        return wide_ram16(kTiles + (meta * 4 + u32(tx & 1) + u32(ty & 1) * 2) * 2);
    }
    return wide_ram16(L.map + 6 + u32(ty * L.width + tx) * 2);
}

void Gba::wide_level_detect() {
    for (auto& L : wide_level_) L.valid = false;
    if (!wide_margin_) return;
    const u16 dispcnt = u16(io_[0] | (io_[1] << 8));
    if ((dispcnt & 7) != 0) return;

    for (int layer = 0; layer < 2; ++layer) {
        const int bg = 2 + layer;
        WideLevel& L = wide_level_[layer];
        const u16 cnt = u16(io_[0x08 + 2 * bg] | (io_[0x09 + 2 * bg] << 8));
        if (!(dispcnt & (0x100 << bg)) || ((cnt >> 14) & 3) != 1) continue;  // on, 512x256

        s32 tiles_w = 0, tiles_h = 0;
        if (layer == 0) {
            L.map = wide_ram32(kMapPtr);
            L.width = s16(wide_ram16(kWidth));
            L.height = s16(wide_ram16(kHeight));
            if ((L.map >> 24) != 0x02 || L.width <= 0 || L.height <= 0 || L.width > 1024 || L.height > 1024) continue;
            if ((L.map & 0x3FFFF) + u32(L.width * L.height * 4) > 0x40000) continue;
            tiles_w = L.width * 2;
            tiles_h = L.height * 2;
        } else {
            const u32 s = wide_ram32(kBgStruct);
            if ((s >> 24) != 0x02 && (s >> 24) != 0x03) continue;
            L.map = wide_ram32(s + 0x30);
            L.width = s16(wide_ram16(L.map + 2));
            L.height = s16(wide_ram16(L.map + 4));
            if (((L.map >> 24) != 0x02 && (L.map >> 24) != 0x03) || L.width <= 0 || L.height <= 0 ||
                L.width > 2048 || L.height > 2048)
                continue;
            if ((L.map >> 24) == 0x02 && (L.map & 0x3FFFF) + 6 + u32(L.width * L.height * 2) > 0x40000) continue;
            tiles_w = L.width;
            tiles_h = L.height;
        }

        const u32 hofs = u32(io_[0x10 + 4 * bg] | (io_[0x11 + 4 * bg] << 8)) & 0x1FF;
        const u32 vofs = u32(io_[0x12 + 4 * bg] | (io_[0x13 + 4 * bg] << 8)) & 0x1FF;
        const u32 screen_base = ((cnt >> 8) & 31) * 0x800;
        const s32 level_w = tiles_w * 8, level_h = tiles_h * 8;
        // Sample tiles spread over the visible area. The best-matching camera wins if at
        // least 80% of its samples agree (a few tiles may differ, e.g. broken blocks).
        static constexpr int kSx[] = {4, 36, 68, 100, 132, 164, 196, 228};
        static constexpr int kSy[] = {12, 60, 108, 148};
        int best = -1;
        s32 best_x = 0, best_y = 0;
        for (s32 cy = s32(vofs & 0xFF); cy < level_h; cy += 256) {
            for (s32 cx = s32(hofs); cx < level_w; cx += 512) {
                int checked = 0, match = 0;
                for (int sy : kSy)
                    for (int sx : kSx) {
                        const s32 ax = cx + sx, ay = cy + sy;
                        if (ax >= level_w || ay >= level_h) continue;
                        const u32 vx = u32(ax >> 3) & 63, vy = u32(ay >> 3) & 31;
                        const u32 off = screen_base + (vx >> 5) * 0x800 + vy * 64 + (vx & 31) * 2;
                        if (off + 1 >= 0x10000) continue;
                        const u16 vram_e = u16(vram_[off] | (vram_[off + 1] << 8));
                        ++checked;
                        if (vram_e == wide_level_entry(layer, ax >> 3, ay >> 3)) ++match;
                    }
                if (checked >= 16 && match * 5 >= checked * 4 && match > best) {
                    best = match;
                    best_x = cx;
                    best_y = cy;
                }
            }
        }
        if (best < 0) continue;
        L.tiles_w = tiles_w;
        L.tiles_h = tiles_h;
        L.cam_x = best_x - s32(hofs);
        L.cam_y = best_y - s32(vofs);
        L.valid = true;
    }
}

}  // namespace knid::gba
