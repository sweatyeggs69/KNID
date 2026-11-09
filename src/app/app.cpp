#include "app/app.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "app/bmp.h"
#include "app/config.h"
#include "app/platform.h"
#include "app/save_import.h"
#include "app/theme.h"
#include "gba/gba.h"
#include "options/input.h"
#include "options/options_panel.h"
#include "options/display.h"
#include "options/presenter.h"
#include "sha1.h"

namespace knid {
namespace fs = std::filesystem;

namespace {

constexpr int kW = gba::kScreenWidth, kH = gba::kScreenHeight;
constexpr const char* kRetailBiosSha1 = "300c20df6731a33952ded8c436f7f186d25d3492";
constexpr double kFrameSeconds = double(gba::kCyclesPerFrame) / gba::kCpuHz;

void draw_test_pattern(std::array<std::uint16_t, kW * kH>& fb, std::uint32_t frame) {
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            auto r = std::uint16_t((x * 31) / (kW - 1)), g = std::uint16_t((y * 31) / (kH - 1));
            auto b = std::uint16_t((frame / 2 + x + y) & 31);
            fb[y * kW + x] = std::uint16_t(r | (g << 5) | (b << 10));
        }
}

class App {
public:
    explicit App(const AppOptions& opt) : opt_(opt), config_(opt.pref_dir) {}
    int run();

private:
    enum class Screen { FirstRun, Launcher, Game };

    bool init_window();
    void shutdown();
    void load_configured_rom();
    void accept_rom(const fs::path& path);
    void open_rom_dialog();
    void open_bios_dialog();
    std::optional<std::vector<std::uint8_t>> load_bios(std::string* error);
    std::unique_ptr<gba::Gba> make_gba(bool with_save);
    void start_attract();
    void start_game();
    void handle_event(const SDL_Event& e);
    void update_emulation();
    void draw_ui();
    void draw_first_run();
    void draw_launcher();
    void draw_game_tab(Config& config);
    void draw_save_section();
    fs::path current_save_path();
    void prepare_import(const fs::path& file);
    void apply_pending_save();
    void export_save(const fs::path& dst);
    void save_message(std::string text, bool error);
    void set_menu_open(bool open);
    void return_to_launcher();
    void draw_backdrop();
    void begin_card(const char* id, ImVec2 size);
    void capture_window(const std::string& path);
    void end_card();

    AppOptions opt_;
    Config config_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* frame_tex_ = nullptr;
    SDL_AudioStream* audio_ = nullptr;
    std::unique_ptr<Presenter> presenter_;           // in-game video (display.* options)
    std::unique_ptr<input::Manager> controls_;       // keyboard/gamepad (input.* options)

    Screen screen_ = Screen::FirstRun;
    VerifiedRom rom_;
    std::string rom_error_;
    std::string info_;  // transient message (shortcut created, ...)
    double info_until_ = 0;
    std::unique_ptr<gba::Gba> attract_;  // launcher backdrop: the game's own attract loop, never saved
    std::unique_ptr<gba::Gba> game_;
    bool game_paused_ = false;
    std::array<std::uint16_t, kW * kH> pattern_{};
    std::vector<std::int16_t> audio_buf_;
    long frames_ = 0;
    bool running_ = true;
    bool shortcut_popup_ = false;
    double game_started_at_ = 0;
    bool menu_was_open_ = false;
    std::vector<std::pair<long, std::uint16_t>> script_;  // --input, in game frames
    size_t script_pos_ = 0;
    std::uint16_t script_keys_ = 0;
    long game_frames_ = 0;

    std::mutex pick_mutex_;
    std::optional<std::string> picked_rom_, picked_bios_, picked_import_, picked_export_;

    // Save import / restore: data waiting for the user's confirmation.
    struct PendingSave {
        std::vector<std::uint8_t> data;
        std::string source, format, warning;
    };
    std::optional<PendingSave> pending_save_;
    bool open_save_confirm_ = false;
    bool open_backup_list_ = false;
    std::string save_msg_;
    bool save_msg_error_ = false;
    fs::path import_from_cli_;
    std::string pick_error_;
};

// ---- setup ---------------------------------------------------------------------------------

bool App::init_window() {
    window_ = SDL_CreateWindow("Kirby: Nightmare in Dream Land (KNID)", 960, 640,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!window_) return false;
    SDL_SetWindowMinimumSize(window_, 720, 480);
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) return false;
    frame_tex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XBGR1555, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    SDL_SetTextureScaleMode(frame_tex_, SDL_SCALEMODE_NEAREST);

    SDL_AudioSpec spec{SDL_AUDIO_S16, 2, gba::kAudioRate};
    audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (audio_) SDL_ResumeAudioStreamDevice(audio_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // layout is fixed; settings live in settings.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);
    theme::apply(SDL_GetWindowPixelDensity(window_));
    if (config_.get_string("display.fullscreen", "0") == "1") SDL_SetWindowFullscreen(window_, true);
    presenter_ = std::make_unique<Presenter>(renderer_, settings_io(config_));
    controls_ = std::make_unique<input::Manager>(settings_io(config_));
    register_options_tab("Game", [this](Config& c) { draw_game_tab(c); });
    SDL_ShowWindow(window_);
    return true;
}

void App::shutdown() {
    if (game_) game_->flush_save();
    game_.reset();
    attract_.reset();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    controls_.reset();
    presenter_.reset();
    if (audio_) SDL_DestroyAudioStream(audio_);
    if (frame_tex_) SDL_DestroyTexture(frame_tex_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
}

void App::load_configured_rom() {
    std::string path = config_.get_string("rom.path");
    if (path.empty()) return;
    VerifiedRom v = verify_rom(path);
    if (v.ok && v.rom.sha1 == config_.get_string("rom.sha1", v.rom.sha1)) {
        rom_ = std::move(v);
    } else {
        rom_error_ = v.ok ? "The saved ROM changed since it was chosen. Please choose it again." : v.error;
    }
}

void App::accept_rom(const fs::path& path) {
    VerifiedRom v = verify_rom(path);
    if (!v.ok) {
        rom_error_ = v.error;
        return;
    }
    rom_error_.clear();
    config_.set_string("rom.path", v.path.string());
    config_.set_string("rom.sha1", v.rom.sha1);
    rom_ = std::move(v);
    game_.reset();
    screen_ = Screen::Launcher;
    start_attract();
}

void App::open_rom_dialog() {
    static const SDL_DialogFileFilter filters[] = {{"Game Boy Advance ROM", "gba"}, {"All files", "*"}};
    std::string start = rom_.ok ? rom_.path.parent_path().string() : std::string();
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
            auto* app = static_cast<App*>(self);
            std::lock_guard<std::mutex> lock(app->pick_mutex_);
            if (!files) app->pick_error_ = SDL_GetError();
            else if (files[0]) app->picked_rom_ = files[0];
        },
        this, window_, filters, 2, start.empty() ? nullptr : start.c_str(), false);
}

void App::open_bios_dialog() {
    static const SDL_DialogFileFilter filters[] = {{"GBA BIOS", "bin;rom"}, {"All files", "*"}};
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
            auto* app = static_cast<App*>(self);
            std::lock_guard<std::mutex> lock(app->pick_mutex_);
            if (files && files[0]) app->picked_bios_ = files[0];
        },
        this, window_, filters, 2, nullptr, false);
}

std::optional<std::vector<std::uint8_t>> App::load_bios(std::string* error) {
    fs::path p = !opt_.bios.empty() ? opt_.bios : fs::path(config_.get_string("bios.path"));
    if (p.empty()) return std::nullopt;
    std::ifstream f(p, std::ios::binary);
    std::vector<std::uint8_t> bios((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string hash = to_hex(sha1(bios.data(), bios.size()));
    if (bios.size() != 0x4000 || hash != kRetailBiosSha1) {
        if (error) *error = "The BIOS file is not the retail GBA BIOS; using the built-in BIOS.";
        return std::nullopt;
    }
    return bios;
}

std::unique_ptr<gba::Gba> App::make_gba(bool with_save) {
    auto g = std::make_unique<gba::Gba>(rom_.rom.data, with_save ? save_path_for(config_, rom_) : fs::path());
    g->verbose = opt_.verbose;
    std::string err;
    if (auto bios = load_bios(&err)) {
        g->use_bios(std::move(*bios));
        g->reset();
    } else if (!err.empty()) {
        info_ = err;
        info_until_ = ImGui::GetTime() + 6;
    }
    return g;
}

void App::start_attract() {
    if (!rom_.ok || opt_.test_pattern) return;
    attract_ = make_gba(false);
}

void App::start_game() {
    if (!rom_.ok && !opt_.test_pattern) return;
    if (!game_ && rom_.ok) game_ = make_gba(true);
    attract_.reset();
    game_paused_ = false;
    screen_ = Screen::Game;
    game_started_at_ = ImGui::GetTime();
    if (audio_) SDL_ClearAudioStream(audio_);
    presenter_->reload();
    controls_->reload();
    presenter_->show_message("F1 or Esc: settings menu", 3.0);
    set_options_menu_quit_handler([this] { return_to_launcher(); });
}

// ---- main loop -----------------------------------------------------------------------------

void App::set_menu_open(bool open) {
    options_menu_set_open(open);
}

void App::return_to_launcher() {
    options_menu_set_open(false);
    game_paused_ = true;
    if (game_) game_->flush_save();
    set_options_menu_quit_handler({});
    screen_ = Screen::Launcher;
}

void App::handle_event(const SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) { running_ = false; return; }
    bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat;
    if (key && e.key.key == SDLK_F11) {
        bool fs_on = !(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN);
        SDL_SetWindowFullscreen(window_, fs_on);
        config_.set_string("display.fullscreen", fs_on ? "1" : "0");  // keep the menu checkbox in sync
        return;
    }
    if (options_menu_open()) {
        // The menu owns input: rebinding captures first, then ImGui. Nothing reaches the game.
        if (options_panel_handle_event(e)) return;
        if (key && (e.key.key == SDLK_F1 || e.key.key == SDLK_ESCAPE)) { set_menu_open(false); return; }
        ImGui_ImplSDL3_ProcessEvent(&e);
        return;
    }
    if (key && e.key.key == SDLK_F1 && screen_ != Screen::FirstRun) { set_menu_open(true); return; }
    if (screen_ == Screen::Game) {
        if (controls_->handle_event(e)) return;  // gamepad hotplug / remap capture
        if (key && e.key.key == SDLK_ESCAPE) {
            if (opt_.test_pattern && !rom_.ok) { running_ = false; return; }
            set_menu_open(true);
        }
        return;
    }
    controls_->handle_event(e);  // gamepad hotplug
    ImGui_ImplSDL3_ProcessEvent(&e);
}

void App::update_emulation() {
    audio_buf_.clear();
    const std::uint16_t* pixels = nullptr;
    if (screen_ == Screen::Game && opt_.test_pattern) {
        draw_test_pattern(pattern_, std::uint32_t(frames_));
        pixels = pattern_.data();
    } else if (screen_ == Screen::Game && game_ && options_menu_open()) {
        pixels = game_->framebuffer();  // paused under the settings menu
    } else if (screen_ == Screen::Game && game_) {
        game_->set_wide_margin(display::wide_margin(presenter_->settings().aspect));
        while (script_pos_ < script_.size() && script_[script_pos_].first <= game_frames_)
            script_keys_ = script_[script_pos_++].second;
        game_->set_keys(std::uint16_t(controls_->poll() | script_keys_));
        game_->run_frame();
        ++game_frames_;
        game_->take_audio(audio_buf_);
        pixels = game_->framebuffer();
    } else if (game_ && game_paused_) {
        pixels = game_->framebuffer();
    } else if (attract_) {
        attract_->run_frame();
        attract_->take_audio(audio_buf_);
        if (!config_.get_bool("launcher.music", false)) audio_buf_.clear();
        pixels = attract_->framebuffer();
    }
    if (pixels) SDL_UpdateTexture(frame_tex_, nullptr, pixels, kW * sizeof(std::uint16_t));
    if (audio_ && !audio_buf_.empty() && SDL_GetAudioStreamQueued(audio_) < gba::kAudioRate / 10 * 4) {
        SDL_SetAudioStreamGain(audio_, screen_ == Screen::Game ? 1.0f : 0.35f);
        SDL_PutAudioStreamData(audio_, audio_buf_.data(), int(audio_buf_.size() * sizeof(std::int16_t)));
    }
}

void App::draw_backdrop() {
    int ow = 0, oh = 0;
    SDL_GetRenderOutputSize(renderer_, &ow, &oh);
    bool have_frame = attract_ || (game_ && game_paused_);
    if (!have_frame) return;
    // Cover the window, keep the aspect ratio, dim it so the UI reads clearly.
    float scale = std::max(float(ow) / kW, float(oh) / kH);
    SDL_FRect dst{(ow - kW * scale) * 0.5f, (oh - kH * scale) * 0.5f, kW * scale, kH * scale};
    SDL_SetTextureColorMod(frame_tex_, 120, 105, 150);
    SDL_SetTextureScaleMode(frame_tex_, SDL_SCALEMODE_LINEAR);
    SDL_RenderTexture(renderer_, frame_tex_, nullptr, &dst);
    SDL_SetTextureColorMod(frame_tex_, 255, 255, 255);
}

void App::capture_window(const std::string& path) {
    SDL_SetRenderScale(renderer_, 1, 1);
    if (SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr)) {
        SDL_SaveBMP(shot, path.c_str());
        SDL_DestroySurface(shot);
    }
}

void App::begin_card(const char* id, ImVec2 size) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
}

void App::end_card() { ImGui::End(); }

void App::draw_first_run() {
    const auto& c = theme::palette();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    begin_card("##firstrun", ImVec2(std::min(620.0f, ds.x - 40), 0));
    ImGui::Dummy(ImVec2(0, 4));
    theme::centered_text("Welcome to KNID", theme::font_title(), c.pink);
    theme::centered_text("Kirby: Nightmare in Dream Land, native on your computer", theme::font_small(), c.text_dim);
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(
        "To play, choose your own copy of the game: a ROM file dumped from your cartridge of "
        "Kirby: Nightmare in Dream Land (USA).");
    ImGui::PushFont(theme::font_small());
    ImGui::TextColored(c.text_dim, "KNID only remembers where the file is. It never copies, uploads or changes it.");
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 10));
    float bw = ImGui::GetContentRegionAvail().x;
    if (theme::big_button("Choose ROM...", ImVec2(bw, 56))) open_rom_dialog();
    if (!rom_error_.empty()) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::Dummy(ImVec2(0, 6));
    if (theme::big_button("Quit", ImVec2(bw, 46), false)) running_ = false;
    end_card();
}

void App::draw_launcher() {
    const auto& c = theme::palette();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    begin_card("##launcher", ImVec2(std::min(560.0f, ds.x - 40), 0));
    ImGui::Dummy(ImVec2(0, 2));
    theme::centered_text("Kirby", theme::font_title(), c.pink);
    theme::centered_text("Nightmare in Dream Land", theme::font_body(), c.yellow);
    ImGui::Dummy(ImVec2(0, 2));
    {
        std::string status = rom_.rom.header.title + "  [" + rom_.rom.header.game_code + "]  verified";
        ImGui::PushFont(theme::font_small());
        float w = ImGui::CalcTextSize(status.c_str()).x + 22;
        float x0 = (ImGui::GetWindowSize().x - w) * 0.5f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        theme::draw_star(ImGui::GetWindowDrawList(), ImVec2(ImGui::GetWindowPos().x + x0 + 7, p.y + 9), 7,
                         theme::u32(c.good), 0);
        ImGui::SetCursorPosX(x0 + 22);
        ImGui::TextColored(c.good, "%s", status.c_str());
        theme::centered_text(rom_.path.filename().string().c_str(), nullptr, c.text_dim);
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0, 8));
    float bw = ImGui::GetContentRegionAvail().x;
    if (game_ && game_paused_) {
        if (theme::big_button("Continue", ImVec2(bw, 58))) start_game();
        if (theme::big_button("Restart", ImVec2(bw, 46), false)) {
            game_->flush_save();
            game_.reset();
            start_game();
        }
    } else {
        if (theme::big_button("Play", ImVec2(bw, 58))) start_game();
    }
    float half = (bw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (theme::big_button("Settings", ImVec2(half, 46), false)) set_menu_open(true);
    ImGui::SameLine();
    if (theme::big_button("Create Shortcut", ImVec2(half, 46), false)) {
        shortcut_popup_ = true;
        ImGui::OpenPopup("Create a shortcut");
    }
    if (theme::big_button("Quit", ImVec2(bw, 46), false)) running_ = false;

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushFont(theme::font_small());
    theme::centered_text("F1: settings (ROM, BIOS, saves, display, controls)", nullptr, c.text_dim);
    if (!rom_error_.empty()) ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
    if (!info_.empty() && ImGui::GetTime() < info_until_) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.yellow, "%s", info_.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::PopFont();

    if (ImGui::BeginPopupModal("Create a shortcut", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted("Add a shortcut that starts the game directly?");
        ImGui::Dummy(ImVec2(0, 4));
        auto make = [&](ShortcutPlace place) {
            ShortcutResult r = create_shortcut(place);
            info_ = r.message;
            info_until_ = ImGui::GetTime() + 10;
            ImGui::CloseCurrentPopup();
        };
#if defined(__APPLE__)
        const char* apps = "Applications folder";
#elif defined(_WIN32)
        const char* apps = "Start menu";
#else
        const char* apps = "Applications menu";
#endif
        if (theme::big_button(apps, ImVec2(320, 46))) make(ShortcutPlace::Applications);
        if (theme::big_button("Desktop", ImVec2(320, 46))) make(ShortcutPlace::Desktop);
        if (theme::big_button("Cancel", ImVec2(320, 42), false)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    end_card();
}

void App::save_message(std::string text, bool error) {
    save_msg_ = std::move(text);
    save_msg_error_ = error;
}

fs::path App::current_save_path() { return rom_.ok ? save_path_for(config_, rom_) : fs::path(); }

void App::prepare_import(const fs::path& file) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(file, bytes)) {
        save_message("Could not read " + file.string(), true);
        return;
    }
    NormalizedSave n = normalize_save(bytes, file.filename().string());
    if (!n.ok) {
        save_message(n.error, true);
        return;
    }
    pending_save_ = PendingSave{std::move(n.data), file.filename().string(), n.format, n.warning};
    open_save_confirm_ = true;
}

void App::apply_pending_save() {
    if (!pending_save_ || !rom_.ok) return;
    fs::path save = current_save_path();
    bool was_playing = game_ != nullptr;
    if (game_) game_->flush_save();  // so the backup has the latest progress
    fs::path backup = backup_save(save, save.parent_path() / "backups");
    if (!write_file_atomic(save, pending_save_->data)) {
        save_message("Could not write the save file " + save.string(), true);
        pending_save_.reset();
        return;
    }
    // The running game would overwrite the imported save on exit, so restart it from the new save.
    game_.reset();
    if (was_playing) {
        game_ = make_gba(true);
        game_paused_ = screen_ != Screen::Game;
    }
    save_message("Imported " + pending_save_->source + (backup.empty() ? "." : ". The previous save was backed up.") +
                     (was_playing ? " The game was restarted." : ""),
                 false);
    pending_save_.reset();
}

void App::export_save(const fs::path& dst) {
    fs::path save = current_save_path();
    if (game_) game_->flush_save();
    std::vector<std::uint8_t> data;
    std::error_code ec;
    if (save.empty() || !fs::exists(save, ec) || !read_file(save, data)) {
        save_message("There is no save to export yet. Play the game first.", true);
        return;
    }
    data.resize(kSramSize, 0xFF);
    fs::path out = dst;
    if (out.extension().empty()) out += ".sav";
    if (!write_file_atomic(out, data)) {
        save_message("Could not write " + out.string(), true);
        return;
    }
    save_message("Exported to " + out.string() + " (raw 32 KiB, works in mGBA, VBA-M and RetroArch).", false);
}

void App::draw_save_section() {
    const auto& c = theme::palette();
    ImGui::SeparatorText("Saves");
    fs::path saves = config_.pref_dir() / "saves";
    ImGui::TextWrapped("%s", saves.string().c_str());
    if (ImGui::Button("Show save folder")) {
        std::error_code ec;
        fs::create_directories(saves, ec);
        SDL_OpenURL(("file://" + saves.string()).c_str());
    }
    ImGui::BeginDisabled(!rom_.ok);
    ImGui::SameLine();
    if (ImGui::Button("Import save...")) {
        static const SDL_DialogFileFilter filters[] = {{"Game saves", "sav;srm;sa1;dsv"}, {"All files", "*"}};
        SDL_ShowOpenFileDialog(
            [](void* self, const char* const* files, int) {
                auto* app = static_cast<App*>(self);
                std::lock_guard<std::mutex> lock(app->pick_mutex_);
                if (files && files[0]) app->picked_import_ = files[0];
            },
            this, window_, filters, 2, nullptr, false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export save...")) {
        static const SDL_DialogFileFilter filters[] = {{"Game save", "sav"}};
        std::string def = (fs::path(rom_.path).parent_path() / "Kirby - Nightmare in Dream Land.sav").string();
        SDL_ShowSaveFileDialog(
            [](void* self, const char* const* files, int) {
                auto* app = static_cast<App*>(self);
                std::lock_guard<std::mutex> lock(app->pick_mutex_);
                if (files && files[0]) app->picked_export_ = files[0];
            },
            this, window_, filters, 1, def.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore backup...")) open_backup_list_ = true;
    ImGui::EndDisabled();
    ImGui::TextDisabled("Imports .sav / .srm battery saves from mGBA, VBA-M, RetroArch and others.");
    if (!save_msg_.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(save_msg_error_ ? c.bad : c.good, "%s", save_msg_.c_str());
        ImGui::PopTextWrapPos();
    }

    if (open_save_confirm_) {
        ImGui::OpenPopup("Replace your save?");
        open_save_confirm_ = false;
    }
    if (ImGui::BeginPopupModal("Replace your save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (pending_save_) {
            ImGui::Text("Use %s as your save?", pending_save_->source.c_str());
            ImGui::TextDisabled("%s", pending_save_->format.c_str());
            ImGui::TextUnformatted("Your current save is backed up first (the last 10 are kept).");
            if (game_)
                ImGui::TextColored(c.yellow, "Importing restarts the game; unsaved progress is lost.");
            if (!pending_save_->warning.empty()) ImGui::TextColored(c.yellow, "%s", pending_save_->warning.c_str());
        }
        ImGui::Dummy(ImVec2(0, 4));
        if (ImGui::Button("Replace save")) {
            apply_pending_save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending_save_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (open_backup_list_) {
        ImGui::OpenPopup("Restore a backup");
        open_backup_list_ = false;
    }
    if (ImGui::BeginPopupModal("Restore a backup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        fs::path save = current_save_path();
        auto backups = list_backups(save.parent_path() / "backups", save.stem().string());
        if (backups.empty()) ImGui::TextUnformatted("No backups yet. One is made every time a save is replaced.");
        for (const auto& b : backups) {
            // <sha1>-YYYYmmdd-HHMMSS.sav -> "YYYY-mm-dd HH:MM:SS"
            std::string n = b.stem().string();
            std::string stamp = n.size() >= 15 ? n.substr(n.size() - 15) : n;
            std::string label = stamp.size() == 15 ? stamp.substr(0, 4) + "-" + stamp.substr(4, 2) + "-" +
                                                         stamp.substr(6, 2) + " " + stamp.substr(9, 2) + ":" +
                                                         stamp.substr(11, 2) + ":" + stamp.substr(13, 2)
                                                   : n;
            if (ImGui::Button(label.c_str(), ImVec2(320, 0))) {
                std::vector<std::uint8_t> data;
                if (read_file(b, data)) {
                    data.resize(kSramSize, 0xFF);
                    pending_save_ = PendingSave{std::move(data), "the backup from " + label, "backup", ""};
                    open_save_confirm_ = true;
                }
                ImGui::CloseCurrentPopup();
            }
        }
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void App::draw_game_tab(Config& config) {
    (void)config;
    const auto& c = theme::palette();
    ImGui::SeparatorText("Game ROM");
    if (rom_.ok) {
        ImGui::TextColored(c.good, "%s [%s] verified", rom_.rom.header.title.c_str(), rom_.rom.header.game_code.c_str());
        ImGui::TextWrapped("%s", rom_.path.string().c_str());
    } else {
        ImGui::TextColored(c.bad, "No ROM chosen");
    }
    if (ImGui::Button("Change ROM...")) open_rom_dialog();
    if (!rom_error_.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
        ImGui::PopTextWrapPos();
    }

    draw_save_section();

    ImGui::SeparatorText("BIOS (optional)");
    std::string bios = config_.get_string("bios.path");
    ImGui::TextWrapped("%s", bios.empty() ? "Built-in BIOS. No file is needed." : bios.c_str());
    if (ImGui::Button(bios.empty() ? "Use a BIOS file..." : "Use the built-in BIOS")) {
        if (bios.empty()) open_bios_dialog();
        else config_.erase("bios.path");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Takes effect when the game is restarted.");

    ImGui::SeparatorText("Shortcut");
    ImGui::TextWrapped("Add a shortcut that starts the game directly, skipping this launcher.");
#if defined(__APPLE__)
    const char* apps = "Add to Applications";
#elif defined(_WIN32)
    const char* apps = "Add to Start menu";
#else
    const char* apps = "Add to applications menu";
#endif
    auto make = [&](ShortcutPlace place) {
        ShortcutResult r = create_shortcut(place);
        info_ = r.message;
        info_until_ = ImGui::GetTime() + 10;
    };
    if (ImGui::Button(apps)) make(ShortcutPlace::Applications);
    ImGui::SameLine();
    if (ImGui::Button("Add to Desktop")) make(ShortcutPlace::Desktop);
    if (!info_.empty() && ImGui::GetTime() < info_until_) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.yellow, "%s", info_.c_str());
        ImGui::PopTextWrapPos();
    }
}

void App::draw_ui() {
    if (screen_ != Screen::Game) {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        bool have_frame = attract_ || (game_ && game_paused_);
        if (have_frame) {
            // Soft night tint over the game backdrop, plus a few stars.
            bg->AddRectFilledMultiColor(ImVec2(0, 0), ds, theme::u32(theme::palette().night, 0.35f),
                                        theme::u32(theme::palette().night, 0.35f),
                                        theme::u32(theme::palette().night, 0.75f),
                                        theme::u32(theme::palette().night, 0.75f));
        } else {
            theme::draw_starfield(bg, ImVec2(0, 0), ds, ImGui::GetTime());
        }
    }
    switch (screen_) {
        case Screen::FirstRun: draw_first_run(); break;
        case Screen::Launcher: if (!options_menu_open()) draw_launcher(); break;
        case Screen::Game: break;
    }
    if (options_menu_open()) draw_options_menu(config_);
}

int App::run() {
    for (size_t p = 0; p < opt_.input_script.size();) {
        size_t comma = opt_.input_script.find(',', p);
        std::string item = opt_.input_script.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
        size_t colon = item.find(':');
        if (colon != std::string::npos)
            script_.emplace_back(std::strtol(item.c_str(), nullptr, 10),
                                 std::uint16_t(std::strtol(item.c_str() + colon + 1, nullptr, 16)));
        if (comma == std::string::npos) break;
        p = comma + 1;
    }
    if (!init_window()) {
        std::fprintf(stderr, "knid: could not create window: %s\n", SDL_GetError());
        return 1;
    }

    if (!opt_.rom.empty()) {
        VerifiedRom v = verify_rom(opt_.rom);
        if (v.ok) {
            rom_ = std::move(v);
            start_game();
        } else {
            rom_error_ = v.error;
        }
    } else {
        load_configured_rom();
        if (rom_.ok && opt_.play) start_game();
    }
    if (opt_.test_pattern && screen_ != Screen::Game) start_game();
    if (screen_ != Screen::Game) {
        screen_ = rom_.ok ? Screen::Launcher : Screen::FirstRun;
        start_attract();
    }
    if (opt_.open_settings && screen_ != Screen::FirstRun) set_menu_open(true);
    if (!opt_.select_rom.empty()) {
        std::lock_guard<std::mutex> lock(pick_mutex_);
        picked_rom_ = opt_.select_rom.string();
    }

    Uint64 next = SDL_GetTicksNS();
    const Uint64 frame_ns = Uint64(kFrameSeconds * 1e9);
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(e);
        {
            std::lock_guard<std::mutex> lock(pick_mutex_);
            if (picked_rom_) { accept_rom(*picked_rom_); picked_rom_.reset(); }
            if (picked_import_) {
                prepare_import(*picked_import_);
                picked_import_.reset();
                if (pending_save_) options_menu_set_open(true);
            }
            if (picked_export_) { export_save(*picked_export_); picked_export_.reset(); }
            if (picked_bios_) {
                config_.set_string("bios.path", *picked_bios_);
                std::string err;
                if (!load_bios(&err)) {
                    config_.erase("bios.path");
                    info_ = err.empty() ? "Could not read that BIOS file." : err;
                } else {
                    info_ = "BIOS set. It is used the next time the game starts.";
                }
                info_until_ = ImGui::GetTime() + 8;
                picked_bios_.reset();
            }
        }

        update_emulation();

        bool menu = options_menu_open();
        if (menu_was_open_ && !menu) {  // apply what changed in the settings menu
            presenter_->reload();
            controls_->reload();
            if (audio_) SDL_ClearAudioStream(audio_);
            if (game_) game_->set_wide_margin(display::wide_margin(presenter_->settings().aspect));
        }
        menu_was_open_ = menu;

        if (screen_ == Screen::Game && !menu) {
            if (!opt_.screenshot.empty() && opt_.frames >= 0 && frames_ + 1 == opt_.frames)
                presenter_->capture_next(opt_.screenshot.string());
            if (opt_.test_pattern || !game_) presenter_->present(pattern_.data(), controls_->remap_prompt());
            else presenter_->present(game_->wide_framebuffer(), game_->wide_width(), controls_->remap_prompt());
            std::string msg = controls_->take_message();
            if (!msg.empty()) presenter_->show_message(msg);
        } else {
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            draw_ui();
            ImGui::Render();
            ImGuiIO& io = ImGui::GetIO();
            SDL_SetRenderScale(renderer_, 1, 1);
            if (screen_ == Screen::Game) {
                // Paused game under the settings menu.
                if (opt_.test_pattern || !game_) presenter_->draw(pattern_.data(), gba::kScreenWidth, controls_->remap_prompt());
                else presenter_->draw(game_->wide_framebuffer(), game_->wide_width(), controls_->remap_prompt());
            } else {
                SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
                SDL_RenderClear(renderer_);
                draw_backdrop();
            }
            SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
            if (!opt_.screenshot.empty() && opt_.frames >= 0 && frames_ + 1 == opt_.frames)
                capture_window(opt_.screenshot.string());
            SDL_RenderPresent(renderer_);
        }

        ++frames_;
        if (opt_.frames >= 0 && frames_ >= opt_.frames) running_ = false;
        next += frame_ns;
        Uint64 now = SDL_GetTicksNS();
        if (next > now) SDL_DelayPrecise(next - now);
        else if (now - next > frame_ns * 5) next = now;
    }
    if (!opt_.dump_frame.empty() && game_) write_bmp(opt_.dump_frame.string(), game_->framebuffer());
    shutdown();
    return 0;
}

}  // namespace

int run_app(const AppOptions& options) {
    App app(options);
    return app.run();
}

}  // namespace knid
