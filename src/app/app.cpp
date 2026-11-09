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
#include "app/fsutil.h"
#include "app/icon_art.h"
#include "app/platform.h"
#include "app/save_import.h"
#include "app/states.h"
#include "app/theme.h"
#include "app/toast.h"
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
    void draw_shortcut_controls(bool big);
    // Save states
    void ensure_store();
    SDL_Texture* thumb_texture(int slot);
    bool save_slot(int slot);
    bool load_slot(int slot);
    void auto_save();
    void startup();
    std::string hotkey_hint(bool brief) const;
    void show_toast(const std::string& text, int slot, toast::Style style = toast::Style::Success);
    void draw_states_tab();
    std::unique_ptr<states::Store> store_;
    int slot_ = 1;
    std::array<SDL_Texture*, states::kSlots + 1> thumb_tex_{};
    std::array<std::time_t, states::kSlots + 1> thumb_at_{};
    std::string states_msg_;
    bool states_msg_err_ = false;
    int rename_slot_ = -1, delete_slot_ = -1, file_slot_ = 1;
    char rename_buf_[64] = {};
    std::optional<std::string> picked_state_import_, picked_state_export_;
    struct StateAction { long frame; char op; int slot; };
    std::vector<StateAction> state_script_;
    size_t state_script_pos_ = 0;
    ShortcutPlace pending_shortcut_place_ = ShortcutPlace::Applications;
    bool confirm_shortcut_ = false;
    std::string bios_checked_path_ = "\x01", bios_status_;
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

    // Frame pacing: with vsync on a ~60 Hz display one emulated frame per refresh; otherwise a
    // time accumulator; without vsync, sleep-based pacing.
    bool vsync_ = false;
    double refresh_hz_ = 0;
    bool lockstep_ = false;  // one emulated frame per refresh
    double audio_fill_avg_ = -1;
    Uint64 audio_log_at_ = 0;
    void update_refresh_rate();
    void update_audio_rate();
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
    window_ = SDL_CreateWindow("Kirby: Nightmare in Dream Land (KNID)", opt_.window_w > 0 ? opt_.window_w : 1040,
                               opt_.window_h > 0 ? opt_.window_h : 680,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!window_) return false;
    SDL_SetWindowMinimumSize(window_, 720, 480);
    {
        // Window / taskbar icon (on macOS the bundle's AppIcon.icns is used instead).
        std::vector<std::uint8_t> icon = icon::render_rgba(256);
        if (SDL_Surface* surf = SDL_CreateSurfaceFrom(256, 256, SDL_PIXELFORMAT_RGBA32, icon.data(), 256 * 4)) {
            SDL_SetWindowIcon(window_, surf);
            SDL_DestroySurface(surf);
        }
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) return false;
    vsync_ = SDL_SetRenderVSync(renderer_, 1);
    update_refresh_rate();
    frame_tex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XBGR1555, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    SDL_SetTextureScaleMode(frame_tex_, SDL_SCALEMODE_NEAREST);

    SDL_AudioSpec spec{SDL_AUDIO_S16, 2, gba::kAudioRate};
    if (!opt_.mute)
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
    toast::set_renderer(renderer_);
    presenter_ = std::make_unique<Presenter>(renderer_, settings_io(config_));
    controls_ = std::make_unique<input::Manager>(settings_io(config_));
    if (opt_.open_tab == "states") register_options_tab("States", [this](Config&) { draw_states_tab(); });
    register_options_tab("Game", [this](Config& c) { draw_game_tab(c); });
    register_options_tab("States", [this](Config&) { draw_states_tab(); });
    slot_ = std::clamp(config_.get_int("states.slot", 1), 1, states::kSlots);
    states::set_hotkeys({[this] { if (game_ && screen_ == Screen::Game) save_slot(slot_); },
                         [this] { if (game_ && screen_ == Screen::Game) load_slot(slot_); },
                         [this](int d) {
                             if (!game_ || screen_ != Screen::Game) return;
                             slot_ = (slot_ - 1 + d + states::kSlots * 4) % states::kSlots + 1;
                             config_.set_int("states.slot", slot_);
                             ensure_store();
                             const states::SlotInfo& i = store_ ? store_->info(slot_) : states::SlotInfo{};
                             show_toast("Save state slot " + std::to_string(slot_) + (i.exists ? "" : " (empty)"),
                                        i.exists ? slot_ : -1, toast::Style::Info);
                         }});
    SDL_ShowWindow(window_);
    return true;
}

void App::shutdown() {
    auto_save();
    toast::set_renderer(nullptr);
    for (SDL_Texture*& t : thumb_tex_)
        if (t) { SDL_DestroyTexture(t); t = nullptr; }
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
    VerifiedRom v = verify_rom(utf8_path(path));
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
    config_.set_string("rom.path", path_utf8(v.path));
    config_.set_string("rom.sha1", v.rom.sha1);
    rom_ = std::move(v);
    game_.reset();
    store_.reset();
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
    fs::path p = !opt_.bios.empty() ? opt_.bios : utf8_path(config_.get_string("bios.path"));
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
        // Real games boot through the BIOS intro (logo + chime); the launcher backdrop skips it.
        g->set_bios_boot(with_save);
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
    auto_save();
    options_menu_set_open(false);
    game_paused_ = true;
    if (game_) game_->flush_save();
    set_options_menu_quit_handler({});
    screen_ = Screen::Launcher;
}

void App::handle_event(const SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) { running_ = false; return; }
    if (e.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED || e.type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED)
        update_refresh_rate();
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
        while (state_script_pos_ < state_script_.size() && state_script_[state_script_pos_].frame <= game_frames_) {
            const StateAction& a = state_script_[state_script_pos_++];
            if (a.op == 's') save_slot(a.slot);
            else if (a.op == 'l') load_slot(a.slot);
        }
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
    // Safety cap (~200 ms queued); normal sync comes from update_audio_rate().
    if (audio_ && !audio_buf_.empty() && SDL_GetAudioStreamQueued(audio_) < gba::kAudioRate / 5 * 4) {
        SDL_SetAudioStreamGain(audio_, opt_.silent_audio ? 0.0f : screen_ == Screen::Game ? 1.0f : 0.35f);
        SDL_PutAudioStreamData(audio_, audio_buf_.data(), int(audio_buf_.size() * sizeof(std::int16_t)));
    }
}

void App::update_refresh_rate() {
    refresh_hz_ = 0;
    if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_))) {
        if (m->refresh_rate_numerator > 0 && m->refresh_rate_denominator > 0)
            refresh_hz_ = double(m->refresh_rate_numerator) / m->refresh_rate_denominator;
        else
            refresh_hz_ = m->refresh_rate;
    }
    const double gba_hz = 1.0 / kFrameSeconds;  // 59.7275
    lockstep_ = vsync_ && refresh_hz_ > 0 && std::fabs(refresh_hz_ / gba_hz - 1.0) < 0.015;
    if (opt_.verbose)
        std::fprintf(stderr, "knid: vsync %s, display %.3f Hz, %s\n", vsync_ ? "on" : "off", refresh_hz_,
                     lockstep_ ? "one frame per refresh" : vsync_ ? "accumulator pacing" : "timer pacing");
}

// Dynamic rate control: the game makes 32768 Hz audio per emulated frame. In lockstep the game runs
// at the display rate, so play the audio faster/slower by that ratio, plus at most +/-0.5% to keep
// about 3 frames of audio queued (no underruns, no growing latency).
void App::update_audio_rate() {
    if (!audio_) return;
    const double frame_bytes = gba::kAudioRate * kFrameSeconds * 4;  // stereo s16
    double fill = double(SDL_GetAudioStreamQueued(audio_)) / frame_bytes;  // in emulated frames
    audio_fill_avg_ = audio_fill_avg_ < 0 ? fill : audio_fill_avg_ * 0.95 + fill * 0.05;
    const double target = 3.0;
    double base = lockstep_ ? refresh_hz_ * kFrameSeconds : 1.0;
    double err = std::clamp((audio_fill_avg_ - target) / target, -1.0, 1.0);
    double ratio = base * (1.0 + 0.005 * err);
    SDL_SetAudioStreamFrequencyRatio(audio_, float(ratio));
    if (opt_.verbose) {
        Uint64 now = SDL_GetTicksNS();
        if (now >= audio_log_at_) {
            std::fprintf(stderr, "knid: audio queue %.2f frames (avg %.2f), rate ratio %.5f\n", fill, audio_fill_avg_, ratio);
            audio_log_at_ = now + 1000000000ull;
        }
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
    SDL_SetTextureColorMod(frame_tex_, 200, 190, 210);  // lightly dimmed; the vignette is drawn by draw_ui
    SDL_SetTextureScaleMode(frame_tex_, SDL_SCALEMODE_NEAREST);
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

// The launcher and first-run screens: one simple pixel-art card centered over the background
// (the user's live title screen, or the pixel starfield before a ROM is chosen). Sizes are in art
// pixels (theme::ui_scale() points each), so the layout is identical at every window size.
void App::begin_card(const char* id, ImVec2 size) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(std::floor(ds.x * 0.5f), std::floor(ds.y * 0.5f)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    theme::pixel_window_begin(id, size, nullptr,
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
}

void App::end_card() { theme::pixel_window_end(); }

// B / Backspace move the keyboard/gamepad focus to Quit (they never quit on their own).
static bool back_pressed() {
    return !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) &&
           (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
}

void App::draw_first_run() {
    const auto& c = theme::palette();
    const float s = float(theme::ui_scale());
    begin_card("##firstrun", ImVec2(232 * s, 0));
    ImGui::Dummy(ImVec2(0, 2 * s));
    theme::centered_text("Welcome!", theme::font_title(), ImVec4(0.85f, 0.38f, 0.58f, 1.0f));
    ImGui::Dummy(ImVec2(0, 3 * s));
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted("To play, choose your own copy of Kirby: Nightmare in Dream Land (USA).");
    ImGui::Dummy(ImVec2(0, 2 * s));
    ImGui::PushFont(theme::font_small());
    ImGui::TextColored(c.text_dim, "KNID only remembers where the file is. It never copies, uploads or changes it.");
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 4 * s));
    bool back = back_pressed();
    if (theme::pixel_button("Choose ROM...", ImVec2(0, 0), true, theme::Icon::Folder)) open_rom_dialog();
    if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
    if (!rom_error_.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
        ImGui::PopTextWrapPos();
    }
    if (back) ImGui::SetKeyboardFocusHere();
    if (theme::pixel_button("Quit", ImVec2(0, 0), false, theme::Icon::Quit)) running_ = false;
    ImGui::Dummy(ImVec2(0, 2 * s));
    end_card();
}

void App::draw_launcher() {
    const auto& c = theme::palette();
    const float s = float(theme::ui_scale());
    begin_card("##launcher", ImVec2(216 * s, 0));
    ImGui::Dummy(ImVec2(0, 2 * s));
    theme::centered_text("Kirby", theme::font_title(), ImVec4(0.85f, 0.38f, 0.58f, 1.0f));
    theme::centered_text("Nightmare in Dream Land", theme::font_display(), c.text);
    ImGui::Dummy(ImVec2(0, 2 * s));
    ImGui::Dummy(ImVec2(0, 4 * s));

    ensure_store();
    bool has_auto = store_ && store_->info(states::kAutoSlot).exists && store_->info(states::kAutoSlot).error.empty();
    bool back = back_pressed();
    if (game_ && game_paused_) {
        if (theme::pixel_button("Resume", ImVec2(0, 0), true, theme::Icon::Continue)) start_game();
        if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
        if (theme::pixel_button("Restart", ImVec2(0, 0), false, theme::Icon::Play)) {
            game_->flush_save();
            game_.reset();
            start_game();
        }
    } else if (has_auto) {
        if (theme::pixel_button("Continue", ImVec2(0, 0), true, theme::Icon::Continue)) load_slot(states::kAutoSlot);
        if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
        if (theme::pixel_button("Play", ImVec2(0, 0), false, theme::Icon::Play)) start_game();
    } else {
        if (theme::pixel_button("Play", ImVec2(0, 0), true, theme::Icon::Play)) start_game();
        if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
    }
    if (theme::pixel_button("Settings", ImVec2(0, 0), false, theme::Icon::Gear)) set_menu_open(true);
    bool has_shortcut = shortcut_state(ShortcutPlace::Applications) == ShortcutState::Ours ||
                        shortcut_state(ShortcutPlace::Desktop) == ShortcutState::Ours;
    if (theme::pixel_button(has_shortcut ? "Shortcuts" : "Shortcut", ImVec2(0, 0), false, theme::Icon::Shortcut)) {
        shortcut_popup_ = true;
        ImGui::OpenPopup("Create a shortcut");
    }
    if (back) ImGui::SetKeyboardFocusHere();
    if (theme::pixel_button("Quit", ImVec2(0, 0), false, theme::Icon::Quit)) running_ = false;

    ImGui::Dummy(ImVec2(0, 2 * s));
    ImGui::PushFont(theme::font_small());
    theme::centered_text("F1 settings", nullptr, c.text_dim);
    if (!rom_error_.empty()) ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
    if (!info_.empty() && ImGui::GetTime() < info_until_) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.text, "%s", info_.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::PopFont();

    if (ImGui::BeginPopupModal("Create a shortcut", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted("A shortcut starts the game directly, skipping this menu.");
        ImGui::Dummy(ImVec2(0, 2 * s));
        draw_shortcut_controls(true);
        ImGui::Dummy(ImVec2(0, 2 * s));
        if (theme::pixel_button("Done", ImVec2(0, 0), false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    end_card();
}

void App::ensure_store() {
    if (store_ || !rom_.ok) return;
    store_ = std::make_unique<states::Store>(config_.pref_dir() / "states" / rom_.rom.sha1, rom_.rom.sha1,
                                             save_path_for(config_, rom_));
}

SDL_Texture* App::thumb_texture(int slot) {
    ensure_store();
    if (!store_) return nullptr;
    const states::SlotInfo& i = store_->info(slot);
    size_t k = size_t(slot);
    if (!i.exists || i.thumb.empty() || !i.error.empty()) {
        if (thumb_tex_[k]) { SDL_DestroyTexture(thumb_tex_[k]); thumb_tex_[k] = nullptr; }
        return nullptr;
    }
    if (!thumb_tex_[k] || thumb_at_[k] != i.saved_at) {
        if (!thumb_tex_[k]) {
            thumb_tex_[k] = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XBGR1555, SDL_TEXTUREACCESS_STATIC, kW, kH);
            SDL_SetTextureScaleMode(thumb_tex_[k], SDL_SCALEMODE_LINEAR);
        }
        SDL_UpdateTexture(thumb_tex_[k], nullptr, i.thumb.data(), kW * 2);
        thumb_at_[k] = i.saved_at;
    }
    return thumb_tex_[k];
}

// The user's current save state shortcuts (rebindable in Controls). brief: first binding only.
std::string App::hotkey_hint(bool brief) const {
    auto d = [&](input::Hotkey h) {
        std::string s = controls_ ? controls_->describe(h) : std::string();
        if (brief) s = s.substr(0, s.find(','));
        return s.empty() ? std::string("unbound") : s;
    };
    if (brief)
        return d(input::Hotkey::SaveState) + " save  " + d(input::Hotkey::LoadState) + " load  " +
               d(input::Hotkey::SlotPrev) + "/" + d(input::Hotkey::SlotNext) + " slot";
    return "save " + d(input::Hotkey::SaveState) + "; load " + d(input::Hotkey::LoadState) + "; previous slot " +
           d(input::Hotkey::SlotPrev) + "; next slot " + d(input::Hotkey::SlotNext);
}

void App::show_toast(const std::string& text, int slot, toast::Style style) {
    std::string body = hotkey_hint(true) + "  (" + states::slot_name(slot_) + ")";
    std::vector<std::uint8_t> rgba;
    if (slot >= 0 && store_) {
        const states::SlotInfo& i = store_->info(slot);
        if (!i.thumb.empty()) {
            rgba.resize(size_t(kW) * kH * 4);
            for (size_t p = 0; p < i.thumb.size(); ++p) {
                std::uint16_t c = i.thumb[p];
                auto ex = [](std::uint16_t v) { return std::uint8_t((v << 3) | (v >> 2)); };
                rgba[p * 4 + 0] = ex(c & 31);
                rgba[p * 4 + 1] = ex((c >> 5) & 31);
                rgba[p * 4 + 2] = ex((c >> 10) & 31);
                rgba[p * 4 + 3] = 255;
            }
        }
    }
    toast::show(text, body, rgba.empty() ? nullptr : rgba.data(), rgba.empty() ? 0 : kW, rgba.empty() ? 0 : kH,
                style == toast::Style::Warning ? 4500 : 2500, style);
}

bool App::save_slot(int slot) {
    ensure_store();
    if (!store_ || !game_) return false;
    std::string err;
    bool ok = store_->save(slot, *game_, &err);
    if (ok) show_toast(slot == states::kAutoSlot ? "State saved to the Auto slot"
                                                 : "State saved to slot " + std::to_string(slot), slot);
    else toast::show("Could not save the state", err, nullptr, 0, 0, 4500, toast::Style::Warning);
    states_msg_ = ok ? "Saved to " + states::slot_name(slot) + "." : err;
    states_msg_err_ = !ok;
    return ok;
}

bool App::load_slot(int slot) {
    ensure_store();
    if (!store_) return false;
    const states::SlotInfo& info = store_->info(slot);
    if (!info.exists || !info.error.empty()) {
        std::string why = info.exists ? info.error : states::slot_name(slot) + " is empty.";
        show_toast(why, -1, toast::Style::Warning);
        states_msg_ = why;
        states_msg_err_ = true;
        return false;
    }
    bool created = !game_;
    if (created) game_ = make_gba(true);
    std::string err;
    if (!store_->load(slot, *game_, &err)) {
        if (created) game_.reset();
        toast::show("Could not load the state", err, nullptr, 0, 0, 4500, toast::Style::Warning);
        states_msg_ = err;
        states_msg_err_ = true;
        return false;
    }
    start_game();
    presenter_->reset_history();
    show_toast(slot == states::kAutoSlot ? "State loaded from the Auto slot"
                                         : "State loaded from slot " + std::to_string(slot),
               slot);
    states_msg_ = "Loaded " + states::slot_name(slot) + ".";
    states_msg_err_ = false;
    return true;
}

// Startup (no ROM argument): launcher, or straight into the game for --play / the shortcut and
// for "Auto-load save state on launch". Never shows a launcher frame when going into the game.
void App::startup() {
    load_configured_rom();
    SDL_PumpEvents();  // so a Shift held while launching is seen
    bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    bool autoload = config_.get_bool("states.autoload", false);
    bool exists = false, usable = false;
    std::string why;
    if (rom_.ok) {
        ensure_store();
        const states::SlotInfo& a = store_->info(states::kAutoSlot);
        exists = a.exists;
        usable = a.exists && a.error.empty();
        why = a.error;
    } else {
        why = rom_error_.empty() ? "No ROM is chosen yet." : rom_error_;
    }
    switch (states::choose_startup(rom_.ok, opt_.play, autoload, exists, usable, shift)) {
        case states::Startup::ResumeAuto:
            if (load_slot(states::kAutoSlot)) return;
            why = states_msg_;
            break;
        case states::Startup::BootGame:
            start_game();
            return;
        case states::Startup::LauncherWithError:
            break;
        case states::Startup::Launcher:
            return;
    }
    toast::show(autoload ? "Couldn't auto-load the save state" : "Couldn't start the game", why, nullptr, 0, 0,
                6000, toast::Style::Warning);
}

void App::auto_save() {
    if (!game_ || game_frames_ < 60) return;
    ensure_store();
    if (store_) {
        std::string err;
        if (!store_->save(states::kAutoSlot, *game_, &err)) std::fprintf(stderr, "knid: auto save failed: %s\n", err.c_str());
    }
}

void App::draw_states_tab() {
    const auto& c = theme::palette();
    ensure_store();
    if (!store_) {
        ImGui::TextUnformatted("Choose your ROM first (Game tab).");
        return;
    }
    ImGui::TextWrapped("Shortcuts in game: %s. Current: %s. Change them in Controls.", hotkey_hint(true).c_str(),
                       states::slot_name(slot_).c_str());
    ImGui::BeginDisabled(!store_->can_undo_load() || !game_);
    if (ImGui::Button("Undo last load")) {
        std::string err;
        bool ok = store_->undo_load(*game_, &err);
        if (ok) presenter_->reset_history();
        states_msg_ = ok ? "Went back to before the last load." : err;
        states_msg_err_ = !ok;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Show folder")) SDL_OpenURL(file_url(store_->dir()).c_str());
    if (!states_msg_.empty()) ImGui::TextColored(states_msg_err_ ? c.bad : c.good, "%s", states_msg_.c_str());
    ImGui::Separator();

    // Fixed-size cards with constant padding and border, so nothing moves on hover or click.
    const float pad = 12.0f, gap = 12.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, int((avail + gap) / (220.0f + gap)));
    float card_w = std::floor((avail - gap * float(cols - 1)) / float(cols));
    float thumb_w = card_w - 2 * pad, thumb_h = std::floor(thumb_w * 2.0f / 3.0f);
    ImGui::PushFont(theme::font_small());
    float small_line = ImGui::GetTextLineHeight(), button_h = ImGui::GetFrameHeight();
    ImGui::PopFont();
    float card_h = 2 * pad + thumb_h + 8 + ImGui::GetTextLineHeight() + 4 + small_line + 10 + button_h;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
    int col = 0;
    for (int slot = 0; slot <= states::kSlots; ++slot) {
        const states::SlotInfo& i = store_->info(slot);
        bool usable = i.exists && i.error.empty();
        ImGui::PushID(slot);
        bool current = slot == slot_;
        ImGui::PushStyleColor(ImGuiCol_Border, current ? c.yellow : ImVec4(1, 1, 1, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
        ImGui::BeginChild("##card", ImVec2(card_w, card_h),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !current) {
            ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
            ImGui::GetWindowDrawList()->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), theme::u32(c.pink, 0.6f),
                                                ImGui::GetStyle().ChildRounding, 0, 1.5f);
        }
        ImVec2 tp = ImGui::GetCursorScreenPos();
        if (SDL_Texture* t = thumb_texture(slot)) {
            ImGui::Image((ImTextureID)(intptr_t)t, ImVec2(thumb_w, thumb_h));
        } else {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(tp, ImVec2(tp.x + thumb_w, tp.y + thumb_h), theme::u32(c.night), 10);
            theme::draw_star(dl, ImVec2(tp.x + thumb_w / 2, tp.y + thumb_h / 2 - 8), 16, theme::u32(c.text_dim, 0.4f));
            const char* empty = i.exists ? "Unusable" : "Empty";
            ImVec2 ts = ImGui::CalcTextSize(empty);
            dl->AddText(ImVec2(tp.x + (thumb_w - ts.x) / 2, tp.y + thumb_h / 2 + 14), theme::u32(c.cream, 0.8f), empty);
            ImGui::Dummy(ImVec2(thumb_w, thumb_h));
        }
        if (ImGui::IsItemClicked() && slot != states::kAutoSlot) {
            slot_ = slot;
            config_.set_int("states.slot", slot_);
        }
        ImGui::Dummy(ImVec2(0, 2));
        std::string title = states::slot_name(slot) + (i.label.empty() ? "" : ": " + i.label);
        // One line, cut with "..." so the card never grows.
        while (title.size() > 4 && ImGui::CalcTextSize(title.c_str()).x > thumb_w) title.resize(title.size() - 4), title += "...";
        ImGui::TextColored(current ? c.yellow : c.text, "%s", title.c_str());
        ImGui::PushFont(theme::font_small());
        if (usable) {
            char when[64];
            std::tm tm = local_time(i.saved_at);
            std::strftime(when, sizeof when, "%b %d, %H:%M", &tm);
            ImGui::TextColored(c.text_dim, "%s  |  %s played", when, states::format_playtime(i.playtime_frames).c_str());
        } else {
            ImGui::TextColored(i.exists ? c.bad : c.text_dim, "%s", i.exists ? i.error.c_str() : "No state saved yet");
        }
        ImGui::PopFont();
        if (slot != states::kAutoSlot) {
            ImGui::BeginDisabled(!game_);
            if (ImGui::SmallButton("Save")) save_slot(slot);
            ImGui::EndDisabled();
            ImGui::SameLine();
        }
        ImGui::BeginDisabled(!usable);
        if (ImGui::SmallButton("Load")) {
            if (slot != states::kAutoSlot) { slot_ = slot; config_.set_int("states.slot", slot_); }
            load_slot(slot);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("...")) ImGui::OpenPopup("more");
        if (ImGui::BeginPopup("more")) {
            if (ImGui::MenuItem("Rename...", nullptr, false, usable)) {
                rename_slot_ = slot;
                std::snprintf(rename_buf_, sizeof rename_buf_, "%s", i.label.c_str());
            }
            if (ImGui::MenuItem("Export...", nullptr, false, i.exists)) {
                file_slot_ = slot;
                static const SDL_DialogFileFilter f[] = {{"KNID save state", "knst"}};
                std::string def = (rom_.path.parent_path() / ("Kirby " + states::slot_name(slot) + ".knst")).string();
                SDL_ShowSaveFileDialog(
                    [](void* self, const char* const* files, int) {
                        auto* app = static_cast<App*>(self);
                        std::lock_guard<std::mutex> lock(app->pick_mutex_);
                        if (files && files[0]) app->picked_state_export_ = files[0];
                    },
                    this, window_, f, 1, def.c_str());
            }
            if (ImGui::MenuItem("Import into this slot...")) {
                file_slot_ = slot;
                static const SDL_DialogFileFilter f[] = {{"KNID save state", "knst"}, {"All files", "*"}};
                SDL_ShowOpenFileDialog(
                    [](void* self, const char* const* files, int) {
                        auto* app = static_cast<App*>(self);
                        std::lock_guard<std::mutex> lock(app->pick_mutex_);
                        if (files && files[0]) app->picked_state_import_ = files[0];
                    },
                    this, window_, f, 2, nullptr, false);
            }
            if (ImGui::MenuItem("Delete...", nullptr, false, i.exists)) delete_slot_ = slot;
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        ImGui::PopID();
        if (++col < cols) ImGui::SameLine();
        else col = 0;
    }
    ImGui::PopStyleVar();  // card gap

    if (rename_slot_ >= 0) { ImGui::OpenPopup("Rename state"); }
    if (ImGui::BeginPopupModal("Rename state", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Label for %s:", states::slot_name(rename_slot_).c_str());
        ImGui::SetNextItemWidth(320);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##label", rename_buf_, sizeof rename_buf_, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Save") || enter) {
            std::string err;
            if (!store_->rename(rename_slot_, rename_buf_, &err)) { states_msg_ = err; states_msg_err_ = true; }
            rename_slot_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { rename_slot_ = -1; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (delete_slot_ >= 0) { ImGui::OpenPopup("Delete state?"); }
    if (ImGui::BeginPopupModal("Delete state?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete %s? It is moved to the Trash.", states::slot_name(delete_slot_).c_str());
        if (ImGui::Button("Delete")) {
            std::string err;
            bool ok = store_->remove(delete_slot_, &err);
            states_msg_ = ok ? states::slot_name(delete_slot_) + " moved to the Trash." : err;
            states_msg_err_ = !ok;
            delete_slot_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { delete_slot_ = -1; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
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
        SDL_OpenURL(file_url(saves).c_str());
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
        ImGui::TextColored(c.good, "Kirby: Nightmare in Dream Land (USA), verified");
        ImGui::PushFont(theme::font_small());
        ImGui::TextColored(c.text_dim, "%s [%s] rev %u", rom_.rom.header.title.c_str(), rom_.rom.header.game_code.c_str(),
                           unsigned(rom_.rom.header.version));
        ImGui::TextColored(c.text_dim, "SHA-1 %s", rom_.rom.sha1.c_str());
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.text_dim, "%s", path_utf8(rom_.path).c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    } else {
        ImGui::TextColored(c.bad, "No ROM chosen");
    }
    if (ImGui::Button("Change ROM...")) open_rom_dialog();
    if (!rom_error_.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(c.bad, "%s", rom_error_.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::SeparatorText("Startup");
    bool autoload = config_.get_bool("states.autoload", false);
    if (ImGui::Checkbox("Auto-load save state on launch", &autoload)) config_.set_bool("states.autoload", autoload);
    ImGui::TextDisabled("Opening KNID goes straight back to where you left off (the Auto save state).");
    ImGui::TextDisabled("Hold Shift while opening KNID to see this menu instead.");

    draw_save_section();

    ImGui::SeparatorText("BIOS (optional)");
    std::string bios = config_.get_string("bios.path");
    if (bios != bios_checked_path_) {  // verify once per change, not every frame
        bios_checked_path_ = bios;
        bios_status_.clear();
        if (!bios.empty() && !load_bios(&bios_status_) && bios_status_.empty())
            bios_status_ = "The BIOS file could not be read; using the built-in BIOS.";
    }
    if (bios.empty()) {
        ImGui::TextWrapped("Built-in BIOS. No file is needed.");
    } else {
        ImGui::TextWrapped("%s", bios.c_str());
        if (bios_status_.empty())
            ImGui::TextColored(c.good, "Retail GBA BIOS verified: the Game Boy logo plays when the game starts, "
                                       "and the game runs on this BIOS.");
        else
            ImGui::TextColored(c.bad, "%s", bios_status_.c_str());
    }
    if (ImGui::Button(bios.empty() ? "Use a BIOS file..." : "Use the built-in BIOS")) {
        if (bios.empty()) open_bios_dialog();
        else config_.erase("bios.path");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Takes effect the next time the game starts.");

    ImGui::SeparatorText("Shortcut");
    ImGui::TextWrapped("A shortcut starts the game directly, skipping the launcher.");
    draw_shortcut_controls(false);
}

void App::draw_shortcut_controls(bool big) {
    const auto& c = theme::palette();
#if defined(__APPLE__)
    const char* apps_name = "Applications";
#elif defined(_WIN32)
    const char* apps_name = "Start menu";
#else
    const char* apps_name = "applications menu";
#endif
    auto report = [&](const ShortcutResult& r) {
        info_ = r.message;
        info_until_ = ImGui::GetTime() + 10;
        if (r.needs_confirm) {
            confirm_shortcut_ = true;
        }
    };
    for (ShortcutPlace place : {ShortcutPlace::Applications, ShortcutPlace::Desktop}) {
        ImGui::PushID(int(place));
        std::string where = place == ShortcutPlace::Desktop ? "Desktop" : apps_name;
        ShortcutState st = shortcut_state(place);
        std::string label = (st == ShortcutState::Ours ? "Update " : "Add to ") + where;
        if (st == ShortcutState::Ours) label = "Update " + where + " shortcut";
        const float us = float(theme::ui_scale());
        bool pressed = big ? theme::pixel_button(label.c_str(), ImVec2(st == ShortcutState::Ours ? 150 * us : 220 * us, 0),
                                                 false, theme::Icon::Shortcut)
                           : ImGui::Button(label.c_str());
        if (pressed) {
            pending_shortcut_place_ = place;
            report(create_shortcut(place));
        }
        if (st == ShortcutState::Ours) {
            ImGui::SameLine();
            bool remove = big ? theme::pixel_button("Remove", ImVec2(66 * us, 0), false, theme::Icon::Quit)
                              : ImGui::Button("Remove shortcut");
            if (remove) report(remove_shortcut(place));
        }
        ImGui::PopID();
    }
    if (!info_.empty() && ImGui::GetTime() < info_until_) {
        ImGui::PushTextWrapPos(big ? 220.0f * float(theme::ui_scale()) : 0.0f);
        ImGui::TextColored(c.yellow, "%s", info_.c_str());
        ImGui::PopTextWrapPos();
    }
    if (confirm_shortcut_) {
        ImGui::OpenPopup("Replace existing item?");
        confirm_shortcut_ = false;
    }
    if (ImGui::BeginPopupModal("Replace existing item?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", shortcut_path(pending_shortcut_place_).string().c_str());
        ImGui::TextUnformatted("already exists and was not made by KNID.");
        ImGui::TextUnformatted("Replace it? The existing item is moved to the Trash.");
        if (ImGui::Button("Replace")) {
            report(create_shortcut(pending_shortcut_place_, true));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep it")) {
            info_ = "Left the existing item alone.";
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::draw_ui() {
    if (screen_ != Screen::Game) {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        bool have_frame = attract_ || (game_ && game_paused_);
        if (have_frame) {
            // Soft plum vignette at the edges so the card reads clearly over the game.
            ImU32 clear = theme::u32(theme::palette().night, 0.0f), edge = theme::u32(theme::palette().night, 0.55f);
            float vw = ds.x * 0.22f, vh = ds.y * 0.22f;
            bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(vw, ds.y), edge, clear, clear, edge);
            bg->AddRectFilledMultiColor(ImVec2(ds.x - vw, 0), ds, clear, edge, edge, clear);
            bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ds.x, vh), edge, edge, clear, clear);
            bg->AddRectFilledMultiColor(ImVec2(0, ds.y - vh), ds, clear, clear, edge, edge);
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
    toast::draw();
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
    for (size_t p = 0; p < opt_.state_script.size();) {
        size_t comma = opt_.state_script.find(',', p);
        std::string item = opt_.state_script.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
        size_t colon = item.find(':');
        if (colon != std::string::npos && colon + 2 <= item.size())
            state_script_.push_back({std::strtol(item.c_str(), nullptr, 10), item[colon + 1],
                                     int(std::strtol(item.c_str() + colon + 2, nullptr, 10))});
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
        startup();
    }
    if (opt_.test_pattern && screen_ != Screen::Game) start_game();
    if (screen_ != Screen::Game) {
        screen_ = rom_.ok ? Screen::Launcher : Screen::FirstRun;
        start_attract();
    }
    if (opt_.open_settings && screen_ != Screen::FirstRun) set_menu_open(true);
    if (!opt_.select_rom.empty()) {
        std::lock_guard<std::mutex> lock(pick_mutex_);
        picked_rom_ = path_utf8(opt_.select_rom);
    }

    Uint64 next = SDL_GetTicksNS();
    Uint64 last = next, acc = 0;
    double lock_acc = 0;
    const Uint64 frame_ns = Uint64(kFrameSeconds * 1e9);
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(e);
        {
            std::lock_guard<std::mutex> lock(pick_mutex_);
            if (picked_rom_) { accept_rom(utf8_path(*picked_rom_)); picked_rom_.reset(); }
            if (picked_import_) {
                prepare_import(utf8_path(*picked_import_));
                picked_import_.reset();
                if (pending_save_) options_menu_set_open(true);
            }
            if (picked_export_) { export_save(utf8_path(*picked_export_)); picked_export_.reset(); }
            if (picked_state_export_ && store_) {
                std::string err;
                bool ok = store_->export_slot(file_slot_, utf8_path(*picked_state_export_), &err);
                states_msg_ = ok ? "Exported " + states::slot_name(file_slot_) + "." : err;
                states_msg_err_ = !ok;
                picked_state_export_.reset();
            }
            if (picked_state_import_ && store_) {
                std::string err;
                bool ok = store_->import_into(file_slot_, utf8_path(*picked_state_import_), &err);
                states_msg_ = ok ? "Imported into " + states::slot_name(file_slot_) + "." : err;
                states_msg_err_ = !ok;
                picked_state_import_.reset();
            }
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

        // How many emulated frames this refresh.
        int steps = 1;
        if (lockstep_) {
            // One frame per refresh, but keep real time: if vsync stops blocking (window hidden or
            // occluded) or refreshes are missed, skip or catch up instead of changing game speed.
            const double refresh_ns = 1e9 / refresh_hz_;
            Uint64 now = SDL_GetTicksNS();
            lock_acc += double(now - last) - refresh_ns;
            last = now;
            if (lock_acc < -2 * refresh_ns) {  // running ahead
                steps = 0;
                lock_acc += refresh_ns;
                SDL_DelayNS(Uint64(refresh_ns * 0.5));
            } else if (lock_acc > 2 * refresh_ns) {  // fell behind
                steps = 2;
                lock_acc -= refresh_ns;
            }
            lock_acc = std::clamp(lock_acc, -4 * refresh_ns, 4 * refresh_ns);
        } else if (vsync_) {
            Uint64 now = SDL_GetTicksNS();
            acc += now - last;
            last = now;
            acc = std::min<Uint64>(acc, frame_ns * 4);
            steps = 0;
            while (acc >= frame_ns && steps < 2) {
                acc -= frame_ns;
                ++steps;
            }
        }
        bool reached_end = false;
        for (int i = 0; i < steps; ++i) {
            update_emulation();
            ++frames_;
            if (opt_.frames >= 0 && frames_ >= opt_.frames) { reached_end = true; break; }
        }
        update_audio_rate();

        bool menu = options_menu_open();
        if (menu_was_open_ && !menu) {  // apply what changed in the settings menu
            presenter_->reload();
            controls_->reload();
            if (audio_) SDL_ClearAudioStream(audio_);
            if (game_) game_->set_wide_margin(display::wide_margin(presenter_->settings().aspect));
        }
        menu_was_open_ = menu;

        bool toast = toast::active();
        if (screen_ == Screen::Game && !menu) {
            if (!opt_.screenshot.empty() && reached_end && !toast) presenter_->capture_next(opt_.screenshot.string());
            if (steps == 0) {
                // No new emulated frame this refresh: show the same image again without re-blending.
                presenter_->redraw(controls_->remap_prompt());
            } else if (opt_.test_pattern || !game_) {
                presenter_->draw(pattern_.data(), gba::kScreenWidth, controls_->remap_prompt());
            } else {
                presenter_->draw(game_->wide_framebuffer(), game_->wide_width(), controls_->remap_prompt());
            }
            if (toast) {  // small save-state notice with the slot's thumbnail
                ImGui_ImplSDLRenderer3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                theme::update_scale(ImGui::GetIO().DisplaySize);
                ImGui::NewFrame();
                toast::draw();
                ImGui::Render();
                ImGuiIO& io = ImGui::GetIO();
                SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
                ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
                SDL_SetRenderScale(renderer_, 1, 1);
                if (!opt_.screenshot.empty() && reached_end) capture_window(opt_.screenshot.string());
            }
            SDL_RenderPresent(renderer_);
            std::string msg = controls_->take_message();
            if (!msg.empty()) presenter_->show_message(msg);
        } else {
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            theme::update_scale(ImGui::GetIO().DisplaySize);
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
            if (!opt_.screenshot.empty() && reached_end) capture_window(opt_.screenshot.string());
            SDL_RenderPresent(renderer_);
        }

        if (reached_end) running_ = false;
        if (!vsync_) {  // fallback: sleep-based pacing
            next += frame_ns;
            Uint64 now = SDL_GetTicksNS();
            if (next > now) SDL_DelayPrecise(next - now);
            else if (now - next > frame_ns * 5) next = now;
        }
    }
    if (!opt_.dump_frame.empty() && game_) write_bmp(opt_.dump_frame.string(), game_->framebuffer());
    shutdown();
    return 0;
}

}  // namespace

int run_app(const AppOptions& options) {
    auto app = std::make_unique<App>(options);  // large object: keep it off the stack
    return app->run();
}

}  // namespace knid
