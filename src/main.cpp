// KNID host shell: verifies the user's ROM and opens a 240x160 window.
// The recompiled game will eventually render into `framebuffer` each frame.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "rom.h"

namespace {
constexpr int kGbaWidth = 240;
constexpr int kGbaHeight = 160;
constexpr int kDefaultScale = 4;

void fail(const std::string& msg) {
    std::fprintf(stderr, "knid: %s\n", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KNID", msg.c_str(), nullptr);
}

// Placeholder output until the recompiled game drives the PPU: a BGR555 test pattern.
void draw_test_pattern(std::array<std::uint16_t, kGbaWidth * kGbaHeight>& fb, std::uint32_t frame) {
    for (int y = 0; y < kGbaHeight; ++y)
        for (int x = 0; x < kGbaWidth; ++x) {
            std::uint16_t r = std::uint16_t((x * 31) / (kGbaWidth - 1));
            std::uint16_t g = std::uint16_t((y * 31) / (kGbaHeight - 1));
            std::uint16_t b = std::uint16_t((frame / 2 + x + y) & 31);
            fb[y * kGbaWidth + x] = std::uint16_t(r | (g << 5) | (b << 10));
        }
}
}  // namespace

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* base = SDL_GetBasePath();
    std::filesystem::path rom_path = (argc > 1) ? std::filesystem::path(argv[1])
                                                : knid::find_default_rom(base ? base : ".");
    if (rom_path.empty()) {
        fail("No ROM found. Pass the path to your own Kirby: Nightmare in Dream Land dump, "
             "or put the .gba file in a 'rom' folder next to the game.");
        SDL_Quit();
        return 1;
    }

    auto loaded = knid::load_rom(rom_path);
    if (!loaded.ok) {
        fail("Could not load " + rom_path.string() + ": " + loaded.error);
        SDL_Quit();
        return 1;
    }
    const auto& rom = loaded.rom;
    std::printf("ROM: %s [%s] rev %u\nSHA-1: %s (%s)\n", rom.header.title.c_str(),
                rom.header.game_code.c_str(), rom.header.version, rom.sha1.c_str(),
                rom.hash_matches ? "verified" : "UNKNOWN DUMP");
    if (!rom.hash_matches) {
        fail("This ROM is not the expected dump of Kirby: Nightmare in Dream Land (USA).\n"
             "Expected SHA-1 " + std::string(knid::kExpectedSha1) + "\nGot " + rom.sha1);
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Kirby: Nightmare in Dream Land (KNID)",
                                          kGbaWidth * kDefaultScale, kGbaHeight * kDefaultScale,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        fail(std::string("Could not create window: ") + SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, kGbaWidth, kGbaHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    SDL_Texture* screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR1555,
                                            SDL_TEXTUREACCESS_STREAMING, kGbaWidth, kGbaHeight);
    SDL_SetTextureScaleMode(screen, SDL_SCALEMODE_NEAREST);

    std::array<std::uint16_t, kGbaWidth * kGbaHeight> framebuffer{};
    bool running = true;
    std::uint32_t frame = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11)
                SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
        }

        draw_test_pattern(framebuffer, frame++);
        SDL_UpdateTexture(screen, nullptr, framebuffer.data(), kGbaWidth * sizeof(std::uint16_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
