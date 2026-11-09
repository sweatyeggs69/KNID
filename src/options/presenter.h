#pragma once
// Draws the GBA frame into the SDL window with the user's display settings (scaling,
// aspect ratio, GBA LCD colors, interframe blending, overlays) and a small on-screen
// message line. Settings change with hotkeys and are saved in the app config.
#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "options/display.h"
#include "options/settings_io.h"

namespace knid {

class Presenter {
public:
    Presenter(SDL_Renderer* renderer, SettingsIO io);
    ~Presenter();
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;

    // F1 help, F2 scaling, F3 aspect, F4 overlay, F5 overlay strength, F6 colors,
    // F7 blending (Shift reverses the cycle). Returns true when the key was used.
    bool handle_key(const SDL_KeyboardEvent& key);

    // Draws a BGR555 240x160 frame. `prompt` stays on screen while non-empty (used by
    // control remapping); messages from show_message fade after a few seconds.
    void present(const std::uint16_t* frame, const std::string& prompt = {});
    void show_message(const std::string& text, double seconds = 2.5);

    const display::Settings& settings() const { return settings_; }
    // Saves the next presented window image (physical pixels) as a BMP, for headless checks.
    void capture_next(std::string bmp_path) { capture_path_ = std::move(bmp_path); }

    // Re-read settings (after the options screen changed them).
    void reload();

private:
    void save() const;
    void changed(const std::string& what);
    void draw_text_lines(const std::vector<std::string>& lines, bool top);

    SDL_Renderer* renderer_;
    SettingsIO io_;
    display::Settings settings_;
    display::ColorPipeline colors_;
    display::FrameBlender blender_;
    std::vector<std::uint32_t> rgb_;
    SDL_Texture* screen_ = nullptr;

    SDL_Texture* overlay_ = nullptr;
    std::vector<std::uint32_t> overlay_pixels_;
    int overlay_w_ = 0, overlay_h_ = 0;
    display::Overlay overlay_kind_ = display::Overlay::None;
    int overlay_strength_ = -1;

    std::string capture_path_;
    std::vector<std::string> message_;
    std::uint64_t message_until_ns_ = 0;
};

}  // namespace knid
