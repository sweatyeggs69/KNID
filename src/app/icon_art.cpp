#include "app/icon_art.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace knid::icon {

namespace {

struct Rgba { float r, g, b, a; };

Rgba over(Rgba dst, Rgba src) {
    float a = src.a + dst.a * (1 - src.a);
    if (a <= 0) return {0, 0, 0, 0};
    return {(src.r * src.a + dst.r * dst.a * (1 - src.a)) / a, (src.g * src.a + dst.g * dst.a * (1 - src.a)) / a,
            (src.b * src.a + dst.b * dst.a * (1 - src.a)) / a, a};
}

Rgba hex(unsigned rgb, float a = 1) {
    return {float((rgb >> 16) & 255) / 255, float((rgb >> 8) & 255) / 255, float(rgb & 255) / 255, a};
}

bool in_star(float x, float y, float cx, float cy, float r) {
    std::array<std::pair<float, float>, 10> p;
    for (int i = 0; i < 10; ++i) {
        float a = -3.14159265f / 2 + float(i) * 3.14159265f / 5;
        float rr = (i & 1) ? r * 0.48f : r;
        p[size_t(i)] = {cx + std::cos(a) * rr, cy + std::sin(a) * rr};
    }
    bool in = false;
    for (size_t i = 0, j = 9; i < 10; j = i++) {
        if (((p[i].second > y) != (p[j].second > y)) &&
            (x < (p[j].first - p[i].first) * (y - p[i].second) / (p[j].second - p[i].second) + p[i].first))
            in = !in;
    }
    return in;
}

// Pixel star on a g x g grid: 0 = empty, 1 = outline (plum), 2 = fill (yellow), 3 = highlight.
std::vector<int> star_grid(int g) {
    std::vector<int> m(size_t(g * g), 0);
    float c = (g - 1) * 0.5f, r = g * 0.49f;
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x)
            if (in_star(float(x), float(y) + g * 0.04f, c, c, r)) m[size_t(y * g + x)] = 2;
    // Trim 1-pixel-wide spikes at the tips so the outline stays chunky.
    for (int y = 0; y < g; ++y) {
        int count = 0;
        for (int x = 0; x < g; ++x) count += m[size_t(y * g + x)] ? 1 : 0;
        if (count <= 2)
            for (int x = 0; x < g; ++x) m[size_t(y * g + x)] = 0;
    }
    std::vector<int> out = m;
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x) {
            if (!m[size_t(y * g + x)]) continue;
            bool edge = x == 0 || y == 0 || x == g - 1 || y == g - 1 || !m[size_t(y * g + x - 1)] ||
                        !m[size_t(y * g + x + 1)] || !m[size_t((y - 1) * g + x)] || !m[size_t((y + 1) * g + x)];
            if (edge) out[size_t(y * g + x)] = 1;
        }
    // Small highlight: a few pixels up and to the left of the center.
    int hx = int(c - g * 0.12f), hy = int(c - g * 0.08f);
    for (int dy = 0; dy < std::max(1, g / 12); ++dy)
        for (int dx = 0; dx < std::max(2, g / 6); ++dx) {
            int x = hx + dx - dy, y = hy + dy;
            if (x >= 0 && y >= 0 && x < g && y < g && out[size_t(y * g + x)] == 2) out[size_t(y * g + x)] = 3;
        }
    return out;
}

}  // namespace

std::vector<std::uint8_t> render_rgba(int size) {
    const int ss = 4;
    const float inset = 0.08f, radius = 0.22f;
    const int g = size <= 32 ? 16 : 24;
    std::vector<int> star = star_grid(g);
    const float star_size = 0.64f;  // of the icon
    std::vector<std::uint8_t> px(size_t(size) * size_t(size) * 4);
    auto in_rrect = [&](float u, float v, float off) {
        float x0 = inset, y0 = inset + off, x1 = 1 - inset, y1 = 1 - inset + off;
        float qx = std::max({x0 + radius - u, 0.0f, u - (x1 - radius)});
        float qy = std::max({y0 + radius - v, 0.0f, v - (y1 - radius)});
        return u >= x0 && u <= x1 && v >= y0 && v <= y1 && qx * qx + qy * qy <= radius * radius;
    };
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            Rgba acc{0, 0, 0, 0};
            float n = 0;
            float pr = 0, pg = 0, pb = 0, pa = 0;
            for (int sy = 0; sy < ss; ++sy)
                for (int sx = 0; sx < ss; ++sx) {
                    float u = (x + (sx + 0.5f) / ss) / size, v = (y + (sy + 0.5f) / ss) / size;
                    Rgba c{0, 0, 0, 0};
                    if (in_rrect(u, v, 0.025f)) c = hex(0x3B2A4A, 0.28f);  // soft shadow
                    if (in_rrect(u, v, 0.0f)) {
                        float t = std::clamp((u + v) * 0.5f, 0.0f, 1.0f);  // pink -> sky diagonal
                        Rgba a = hex(0xF7A8C4), b = hex(0x9FD3F5);
                        c = over(c, {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1});
                        // Gentle top light.
                        c = over(c, {1, 1, 1, std::max(0.0f, 0.22f - (v - inset) * 0.6f)});
                    }
                    pr += c.r * c.a; pg += c.g * c.a; pb += c.b * c.a; pa += c.a;
                    n += 1;
                }
            acc = pa > 0 ? Rgba{pr / pa, pg / pa, pb / pa, pa / n} : Rgba{0, 0, 0, 0};
            // Pixel star, nearest-neighbour from its grid (crisp edges).
            float u = (x + 0.5f) / size, v = (y + 0.5f) / size;
            float s0 = (1 - star_size) * 0.5f;
            int gx = int(std::floor((u - s0) / star_size * g)), gy = int(std::floor((v - s0) / star_size * g));
            if (gx >= 0 && gy >= 0 && gx < g && gy < g) {
                int k = star[size_t(gy * g + gx)];
                if (k == 1) acc = over(acc, hex(0x3B2A4A));
                else if (k == 2) acc = over(acc, hex(0xFFD95A));
                else if (k == 3) acc = over(acc, hex(0xFFF8F0));
            }
            size_t i = (size_t(y) * size_t(size) + size_t(x)) * 4;
            px[i + 0] = std::uint8_t(std::lround(std::clamp(acc.r, 0.0f, 1.0f) * 255));
            px[i + 1] = std::uint8_t(std::lround(std::clamp(acc.g, 0.0f, 1.0f) * 255));
            px[i + 2] = std::uint8_t(std::lround(std::clamp(acc.b, 0.0f, 1.0f) * 255));
            px[i + 3] = std::uint8_t(std::lround(std::clamp(acc.a, 0.0f, 1.0f) * 255));
        }
    return px;
}

namespace {
std::uint32_t crc32(const std::uint8_t* d, size_t n, std::uint32_t c = 0xFFFFFFFF) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320 ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
        init = true;
    }
    for (size_t i = 0; i < n; ++i) c = table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c;
}
void be32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int i = 3; i >= 0; --i) v.push_back(std::uint8_t(x >> (8 * i)));
}
}  // namespace

std::vector<std::uint8_t> encode_png(int w, int h, const std::vector<std::uint8_t>& rgba) {
    std::vector<std::uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + long(y) * w * 4, rgba.begin() + long(y + 1) * w * 4);
    }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        z.push_back(off + n >= raw.size() ? 1 : 0);
        z.push_back(std::uint8_t(n));
        z.push_back(std::uint8_t(n >> 8));
        z.push_back(std::uint8_t(~n));
        z.push_back(std::uint8_t(~n >> 8));
        z.insert(z.end(), raw.begin() + long(off), raw.begin() + long(off + n));
    }
    be32(z, (b << 16) | a);
    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    auto chunk = [&](const char* type, const std::vector<std::uint8_t>& data) {
        be32(png, std::uint32_t(data.size()));
        size_t start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        be32(png, ~crc32(&png[start], png.size() - start));
    };
    std::vector<std::uint8_t> ihdr;
    be32(ihdr, std::uint32_t(w));
    be32(ihdr, std::uint32_t(h));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    return png;
}

}  // namespace knid::icon
