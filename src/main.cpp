// KNID host shell: verifies the user's ROM, runs it on the knid_gba runtime and presents
// video, audio and input through SDL3.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "gba/gba.h"
#include "rom.h"
#include "sha1.h"

namespace {
constexpr int kGbaWidth = knid::gba::kScreenWidth;
constexpr int kGbaHeight = knid::gba::kScreenHeight;
constexpr int kDefaultScale = 4;
constexpr const char* kRetailBiosSha1 = "300c20df6731a33952ded8c436f7f186d25d3492";
constexpr double kFrameSeconds = double(knid::gba::kCyclesPerFrame) / knid::gba::kCpuHz;  // ~1/59.73

void fail(const std::string& msg, bool gui) {
    std::fprintf(stderr, "knid: %s\n", msg.c_str());
    if (gui) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "KNID", msg.c_str(), nullptr);
}

void draw_test_pattern(std::array<std::uint16_t, kGbaWidth * kGbaHeight>& fb, std::uint32_t frame) {
    for (int y = 0; y < kGbaHeight; ++y)
        for (int x = 0; x < kGbaWidth; ++x) {
            std::uint16_t r = std::uint16_t((x * 31) / (kGbaWidth - 1));
            std::uint16_t g = std::uint16_t((y * 31) / (kGbaHeight - 1));
            std::uint16_t b = std::uint16_t((frame / 2 + x + y) & 31);
            fb[y * kGbaWidth + x] = std::uint16_t(r | (g << 5) | (b << 10));
        }
}

// Writes a 24-bit BMP of a BGR555 frame (used by --dump-frame for headless checks).
bool write_bmp(const std::string& path, const std::uint16_t* px) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const int row = kGbaWidth * 3;
    const std::uint32_t data = std::uint32_t(row * kGbaHeight), size = 54 + data;
    std::uint8_t h[54] = {'B', 'M'};
    auto put = [&](int off, std::uint32_t v, int n) { for (int i = 0; i < n; ++i) h[off + i] = std::uint8_t(v >> (8 * i)); };
    put(2, size, 4); put(10, 54, 4); put(14, 40, 4); put(18, kGbaWidth, 4);
    put(22, std::uint32_t(-kGbaHeight), 4); put(26, 1, 2); put(28, 24, 2); put(34, data, 4);
    f.write(reinterpret_cast<char*>(h), 54);
    std::vector<std::uint8_t> line(row);
    for (int y = 0; y < kGbaHeight; ++y) {
        for (int x = 0; x < kGbaWidth; ++x) {
            std::uint16_t c = px[y * kGbaWidth + x];
            auto ex = [](std::uint16_t v) { return std::uint8_t((v << 3) | (v >> 2)); };
            line[x * 3 + 0] = ex((c >> 10) & 31);
            line[x * 3 + 1] = ex((c >> 5) & 31);
            line[x * 3 + 2] = ex(c & 31);
        }
        f.write(reinterpret_cast<char*>(line.data()), row);
    }
    return bool(f);
}

std::uint16_t read_keys(SDL_Gamepad* pad) {
    using namespace knid::gba;
    const bool* k = SDL_GetKeyboardState(nullptr);
    std::uint16_t keys = 0;
    if (k[SDL_SCANCODE_X]) keys |= kKeyA;
    if (k[SDL_SCANCODE_Z]) keys |= kKeyB;
    if (k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_RSHIFT]) keys |= kKeySelect;
    if (k[SDL_SCANCODE_RETURN]) keys |= kKeyStart;
    if (k[SDL_SCANCODE_RIGHT]) keys |= kKeyRight;
    if (k[SDL_SCANCODE_LEFT]) keys |= kKeyLeft;
    if (k[SDL_SCANCODE_UP]) keys |= kKeyUp;
    if (k[SDL_SCANCODE_DOWN]) keys |= kKeyDown;
    if (k[SDL_SCANCODE_S]) keys |= kKeyR;
    if (k[SDL_SCANCODE_A]) keys |= kKeyL;
    if (pad) {
        auto b = [&](SDL_GamepadButton btn) { return SDL_GetGamepadButton(pad, btn); };
        const std::int16_t dead = 12000;
        std::int16_t ax = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        std::int16_t ay = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (b(SDL_GAMEPAD_BUTTON_SOUTH)) keys |= kKeyA;
        if (b(SDL_GAMEPAD_BUTTON_EAST)) keys |= kKeyB;
        if (b(SDL_GAMEPAD_BUTTON_WEST)) keys |= kKeyB;
        if (b(SDL_GAMEPAD_BUTTON_NORTH)) keys |= kKeyA;
        if (b(SDL_GAMEPAD_BUTTON_BACK)) keys |= kKeySelect;
        if (b(SDL_GAMEPAD_BUTTON_START)) keys |= kKeyStart;
        if (b(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) keys |= kKeyL;
        if (b(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) keys |= kKeyR;
        if (b(SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -dead) keys |= kKeyUp;
        if (b(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > dead) keys |= kKeyDown;
        if (b(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -dead) keys |= kKeyLeft;
        if (b(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > dead) keys |= kKeyRight;
    }
    // Opposite directions cannot be pressed at once on real hardware.
    if ((keys & kKeyLeft) && (keys & kKeyRight)) keys &= ~(kKeyLeft | kKeyRight);
    if ((keys & kKeyUp) && (keys & kKeyDown)) keys &= ~(kKeyUp | kKeyDown);
    return keys;
}

struct Options {
    std::filesystem::path rom, bios, dump_frame, dump_audio;
    bool test_pattern = false, headless = false, verbose = false;
    long frames = -1;  // stop after this many frames (-1: run until closed)
    std::string script;  // headless input script: "frame:keys,frame:keys" (keys as hex mask)
};

void usage() {
    std::printf(
        "usage: knid [rom.gba] [options]\n"
        "  --test-pattern     show the SDL test pattern instead of running the game\n"
        "  --bios <file>      use your own GBA BIOS dump instead of the built-in BIOS HLE\n"
        "  --frames <n>       quit after n frames\n"
        "  --headless         run without a window (with --frames / --dump-frame)\n"
        "  --dump-frame <bmp> write the last frame to a BMP file on exit\n"
        "  --dump-audio <wav> write headless audio to a WAV file\n"
        "  --input <script>   headless input, e.g. 300:8,310:0 (frame:hex key mask)\n"
        "  --verbose          log unimplemented BIOS calls\n"
        "keys: arrows = D-pad, X = A, Z = B, A = L, S = R, Enter = Start,\n"
        "      Backspace/Right Shift = Select, F11 = fullscreen, Esc = quit\n");
}
}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--test-pattern") opt.test_pattern = true;
        else if (a == "--bios") opt.bios = next();
        else if (a == "--frames") opt.frames = std::strtol(next().c_str(), nullptr, 10);
        else if (a == "--headless") opt.headless = true;
        else if (a == "--dump-frame") opt.dump_frame = next();
        else if (a == "--input") opt.script = next();
        else if (a == "--dump-audio") opt.dump_audio = next();
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else opt.rom = a;
    }
    bool gui = !opt.headless;

    SDL_InitFlags flags = gui ? (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) : 0;
    if (!SDL_Init(flags)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* base = SDL_GetBasePath();
    std::filesystem::path rom_path = !opt.rom.empty() ? opt.rom : knid::find_default_rom(base ? base : ".");
    if (rom_path.empty()) {
        fail("No ROM found. Pass the path to your own Kirby: Nightmare in Dream Land dump, "
             "or put the .gba file in a 'rom' folder next to the game.", gui);
        SDL_Quit();
        return 1;
    }

    auto loaded = knid::load_rom(rom_path);
    if (!loaded.ok) {
        fail("Could not load " + rom_path.string() + ": " + loaded.error, gui);
        SDL_Quit();
        return 1;
    }
    const auto& rom = loaded.rom;
    std::printf("ROM: %s [%s] rev %u\nSHA-1: %s (%s)\n", rom.header.title.c_str(),
                rom.header.game_code.c_str(), rom.header.version, rom.sha1.c_str(),
                rom.hash_matches ? "verified" : "UNKNOWN DUMP");
    if (!rom.hash_matches) {
        fail("This ROM is not the expected dump of Kirby: Nightmare in Dream Land (USA).\n"
             "Expected SHA-1 " + std::string(knid::kExpectedSha1) + "\nGot " + rom.sha1, gui);
        SDL_Quit();
        return 1;
    }

    std::filesystem::path save_path = rom_path;
    save_path.replace_extension(".sav");
    auto gba = std::make_unique<knid::gba::Gba>(rom.data, save_path);
    gba->verbose = opt.verbose;
    if (!opt.bios.empty()) {
        std::ifstream f(opt.bios, std::ios::binary);
        std::vector<std::uint8_t> bios((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string hash = knid::to_hex(knid::sha1(bios.data(), bios.size()));
        if (bios.size() != 0x4000 || hash != kRetailBiosSha1) {
            fail("BIOS " + opt.bios.string() + " is not the expected GBA BIOS (SHA-1 " + hash + ")", gui);
            SDL_Quit();
            return 1;
        }
        gba->use_bios(std::move(bios));
        gba->reset();
        std::printf("BIOS: real BIOS dump (verified)\n");
    } else {
        std::printf("BIOS: built-in HLE\n");
    }

    // Parse the headless input script.
    std::vector<std::pair<long, std::uint16_t>> script;
    for (size_t p = 0; p < opt.script.size();) {
        size_t comma = opt.script.find(',', p);
        std::string item = opt.script.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
        size_t colon = item.find(':');
        if (colon != std::string::npos)
            script.emplace_back(std::strtol(item.c_str(), nullptr, 10),
                                std::uint16_t(std::strtol(item.c_str() + colon + 1, nullptr, 16)));
        if (comma == std::string::npos) break;
        p = comma + 1;
    }

    if (!gui) {
        long frames = opt.frames < 0 ? 600 : opt.frames;
        size_t si = 0;
        std::vector<std::int16_t> audio, all_audio;
        Uint64 t0 = SDL_GetTicksNS();
        for (long f = 0; f < frames; ++f) {
            while (si < script.size() && script[si].first <= f) gba->set_keys(script[si++].second);
            gba->run_frame();
            audio.clear();
            gba->take_audio(audio);
            if (!opt.dump_audio.empty()) all_audio.insert(all_audio.end(), audio.begin(), audio.end());
        }
        if (!opt.dump_audio.empty()) {
            std::ofstream w(opt.dump_audio, std::ios::binary);
            std::uint32_t bytes = std::uint32_t(all_audio.size() * 2), rate = knid::gba::kAudioRate;
            auto u32 = [&](std::uint32_t v) { w.write(reinterpret_cast<char*>(&v), 4); };
            auto u16 = [&](std::uint16_t v) { w.write(reinterpret_cast<char*>(&v), 2); };
            w.write("RIFF", 4); u32(36 + bytes); w.write("WAVEfmt ", 8); u32(16); u16(1); u16(2);
            u32(rate); u32(rate * 4); u16(4); u16(16); w.write("data", 4); u32(bytes);
            w.write(reinterpret_cast<const char*>(all_audio.data()), bytes);
        }
        double secs = double(SDL_GetTicksNS() - t0) / 1e9;
        std::printf("ran %ld frames in %.2f s (%.0f fps)\n%s\n", frames, secs, frames / secs,
                    gba->debug_state().c_str());
        if (!opt.dump_frame.empty() && !write_bmp(opt.dump_frame.string(), gba->framebuffer()))
            fail("could not write " + opt.dump_frame.string(), false);
        gba->flush_save();
        SDL_Quit();
        return 0;
    }

    SDL_Window* window = SDL_CreateWindow("Kirby: Nightmare in Dream Land (KNID)",
                                          kGbaWidth * kDefaultScale, kGbaHeight * kDefaultScale,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        fail(std::string("Could not create window: ") + SDL_GetError(), gui);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, kGbaWidth, kGbaHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_Texture* screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR1555, SDL_TEXTUREACCESS_STREAMING,
                                            kGbaWidth, kGbaHeight);
    SDL_SetTextureScaleMode(screen, SDL_SCALEMODE_NEAREST);

    SDL_AudioSpec spec{SDL_AUDIO_S16, 2, knid::gba::kAudioRate};
    SDL_AudioStream* audio_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);
    else std::fprintf(stderr, "knid: no audio (%s)\n", SDL_GetError());

    SDL_Gamepad* pad = nullptr;
    std::array<std::uint16_t, kGbaWidth * kGbaHeight> pattern{};
    std::vector<std::int16_t> audio;
    bool running = true;
    long frame = 0;
    Uint64 next_frame = SDL_GetTicksNS();
    const Uint64 frame_ns = Uint64(kFrameSeconds * 1e9);
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11)
                SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            if (e.type == SDL_EVENT_GAMEPAD_ADDED && !pad) pad = SDL_OpenGamepad(e.gdevice.which);
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED && pad && SDL_GetGamepadID(pad) == e.gdevice.which) {
                SDL_CloseGamepad(pad);
                pad = nullptr;
            }
        }

        const std::uint16_t* pixels;
        if (opt.test_pattern) {
            draw_test_pattern(pattern, std::uint32_t(frame));
            pixels = pattern.data();
        } else {
            gba->set_keys(read_keys(pad));
            gba->run_frame();
            audio.clear();
            gba->take_audio(audio);
            // Keep audio latency bounded (~100 ms): drop samples if the device falls behind.
            if (audio_stream && !audio.empty() &&
                SDL_GetAudioStreamQueued(audio_stream) < int(knid::gba::kAudioRate / 10 * 4))
                SDL_PutAudioStreamData(audio_stream, audio.data(), int(audio.size() * sizeof(std::int16_t)));
            pixels = gba->framebuffer();
        }
        SDL_UpdateTexture(screen, nullptr, pixels, kGbaWidth * sizeof(std::uint16_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        ++frame;
        if (opt.frames >= 0 && frame >= opt.frames) running = false;
        next_frame += frame_ns;
        Uint64 now = SDL_GetTicksNS();
        if (next_frame > now) SDL_DelayPrecise(next_frame - now);
        else if (now - next_frame > frame_ns * 5) next_frame = now;  // fell behind: resync
    }

    if (!opt.dump_frame.empty()) write_bmp(opt.dump_frame.string(), gba->framebuffer());
    gba.reset();  // flushes the save
    if (pad) SDL_CloseGamepad(pad);
    if (audio_stream) SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
