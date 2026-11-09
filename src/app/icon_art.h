#pragma once
// The KNID app icon, drawn from code (original art): a crisp pixel-art star with a plum outline
// and a small highlight on a smooth pink-to-sky-blue rounded square with a soft shadow.
// Used by tools/knid_icon (.icns/.ico/PNG) and by the app (window icon, Linux shortcut icon).

#include <cstdint>
#include <vector>

namespace knid::icon {

// size x size pixels, RGBA8 (straight alpha), rows top to bottom.
std::vector<std::uint8_t> render_rgba(int size);
// Encodes RGBA8 pixels as a PNG (stored deflate blocks; no compression library needed).
std::vector<std::uint8_t> encode_png(int width, int height, const std::vector<std::uint8_t>& rgba);

}  // namespace knid::icon
