// knid_icon: writes the KNID app icon (original art from src/app/icon_art.cpp) as a macOS
// .iconset folder, a Windows .ico, or a single PNG.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "app/icon_art.h"

namespace {

bool write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    return bool(f);
}

std::vector<std::uint8_t> png(int size) { return knid::icon::encode_png(size, size, knid::icon::render_rgba(size)); }

// Windows .ico with PNG-compressed entries (supported since Windows Vista).
bool write_ico(const std::string& path) {
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    std::vector<std::vector<std::uint8_t>> imgs;
    for (int s : sizes) imgs.push_back(png(s));
    std::vector<std::uint8_t> out;
    auto le16 = [&](unsigned v) { out.push_back(std::uint8_t(v)); out.push_back(std::uint8_t(v >> 8)); };
    auto le32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(std::uint8_t(v >> (8 * i))); };
    le16(0);
    le16(1);
    le16(unsigned(imgs.size()));
    std::uint32_t offset = 6 + 16 * std::uint32_t(imgs.size());
    for (size_t i = 0; i < imgs.size(); ++i) {
        out.push_back(std::uint8_t(sizes[i] >= 256 ? 0 : sizes[i]));
        out.push_back(std::uint8_t(sizes[i] >= 256 ? 0 : sizes[i]));
        out.push_back(0);
        out.push_back(0);
        le16(1);
        le16(32);
        le32(std::uint32_t(imgs[i].size()));
        le32(offset);
        offset += std::uint32_t(imgs[i].size());
    }
    for (auto& img : imgs) out.insert(out.end(), img.begin(), img.end());
    return write_file(path, out);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--ico") return write_ico(argv[2]) ? 0 : 1;
    if (argc == 4 && std::string(argv[1]) == "--png") {
        int px = std::atoi(argv[2]);
        return px > 0 && write_file(argv[3], png(px)) ? 0 : 1;
    }
    if (argc < 2 || argv[1][0] == '-') {
        std::fprintf(stderr, "usage: knid_icon <out.iconset> | --ico <out.ico> | --png <size> <out.png>\n");
        return 2;
    }
    std::string dir = argv[1];
    const int sizes[] = {16, 32, 128, 256, 512};
    for (int s : sizes)
        for (int scale = 1; scale <= 2; ++scale) {
            std::string name = dir + "/icon_" + std::to_string(s) + "x" + std::to_string(s) + (scale == 2 ? "@2x" : "") + ".png";
            if (!write_file(name, png(s * scale))) {
                std::fprintf(stderr, "could not write %s\n", name.c_str());
                return 1;
            }
        }
    return 0;
}
