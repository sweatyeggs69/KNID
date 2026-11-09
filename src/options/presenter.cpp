#include "options/presenter.h"

#include <algorithm>
#include <sstream>

namespace knid {

using namespace display;

namespace {
template <typename E>
E cycle(E v, bool back) {
    const int n = static_cast<int>(E::Count);
    return static_cast<E>((static_cast<int>(v) + (back ? n - 1 : 1)) % n);
}

const char* const kHelp[] = {
    "F2  Scaling       F3  Aspect ratio",
    "F4  Overlay       F5  Overlay strength",
    "F6  GBA colors    F7  Frame blending",
    "F9  Remap controls (Shift+F9: defaults)",
    "F11 Fullscreen    Shift reverses a cycle",
};
}  // namespace

Presenter::Presenter(SDL_Renderer* renderer, SettingsIO io) : renderer_(renderer), io_(std::move(io)) {
    reload();
    rgb_.resize(kPixels);
    screen_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    SDL_SetTextureScaleMode(screen_, SDL_SCALEMODE_NEAREST);
    // We place the frame ourselves, in physical pixels.
    SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
}

Presenter::~Presenter() {
    if (overlay_) SDL_DestroyTexture(overlay_);
    if (screen_) SDL_DestroyTexture(screen_);
}

void Presenter::reload() {
    if (io_.get) settings_ = load(io_.get);
}

void Presenter::save() const {
    if (io_.set) store(settings_, io_.set);
    if (io_.save) io_.save();
}

void Presenter::changed(const std::string& what) {
    save();
    show_message(what);
}

void Presenter::show_message(const std::string& text, double seconds) {
    message_.clear();
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) message_.push_back(line);
    message_until_ns_ = SDL_GetTicksNS() + std::uint64_t(seconds * 1e9);
}

bool Presenter::handle_key(const SDL_KeyboardEvent& key) {
    if (key.repeat) return false;
    const bool back = (key.mod & SDL_KMOD_SHIFT) != 0;
    auto& s = settings_;
    switch (key.key) {
        case SDLK_F1: {
            std::string all;
            for (const char* l : kHelp) all += std::string(l) + "\n";
            show_message(all, 6.0);
            return true;
        }
        case SDLK_F2:
            s.scale = cycle(s.scale, back);
            changed(std::string("Scaling: ") + name(s.scale) +
                    (s.scale == ScaleMode::Integer ? " (pixel perfect)" : ""));
            return true;
        case SDLK_F3:
            s.aspect = cycle(s.aspect, back);
            changed(std::string("Aspect ratio: ") + name(s.aspect));
            return true;
        case SDLK_F4:
            s.overlay = cycle(s.overlay, back);
            changed(std::string("Overlay: ") + name(s.overlay));
            return true;
        case SDLK_F5: {
            static constexpr int kSteps[] = {20, 40, 60, 80, 100};
            int i = 0;
            while (i < 5 && kSteps[i] < s.overlay_strength) ++i;  // first step >= current
            if (i < 5 && kSteps[i] == s.overlay_strength) i = back ? i + 4 : i + 1;
            else i = back ? i + 4 : i;
            s.overlay_strength = kSteps[i % 5];
            changed("Overlay strength: " + std::to_string(s.overlay_strength) + "%");
            return true;
        }
        case SDLK_F6:
            s.color = cycle(s.color, back);
            changed(std::string("Colors: ") + name(s.color));
            return true;
        case SDLK_F7:
            s.blend = cycle(s.blend, back);
            changed(std::string("Frame blending: ") + name(s.blend));
            return true;
        default:
            return false;
    }
}

void Presenter::present(const std::uint16_t* frame, const std::string& prompt) {
    colors_.convert(frame, rgb_.data(), settings_.color);
    blender_.process(rgb_.data(), settings_.blend);
    SDL_UpdateTexture(screen_, nullptr, rgb_.data(), kWidth * sizeof(std::uint32_t));

    int out_w = 0, out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &out_w, &out_h);
    const Layout layout = compute_layout(out_w, out_h, settings_.scale, settings_.aspect);
    const SDL_FRect dst{float(layout.dest.x), float(layout.dest.y), float(layout.dest.w), float(layout.dest.h)};

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, screen_, nullptr, &dst);

    // The overlay is a multiply mask at the exact output size, rebuilt only when it changes.
    if (layout.dest.w != overlay_w_ || layout.dest.h != overlay_h_ || settings_.overlay != overlay_kind_ ||
        settings_.overlay_strength != overlay_strength_) {
        overlay_w_ = layout.dest.w;
        overlay_h_ = layout.dest.h;
        overlay_kind_ = settings_.overlay;
        overlay_strength_ = settings_.overlay_strength;
        if (overlay_) SDL_DestroyTexture(overlay_);
        overlay_ = nullptr;
        if (build_overlay(overlay_kind_, overlay_strength_, overlay_w_, overlay_h_, overlay_pixels_)) {
            overlay_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                         overlay_w_, overlay_h_);
            if (overlay_) {
                SDL_UpdateTexture(overlay_, nullptr, overlay_pixels_.data(), overlay_w_ * 4);
                SDL_SetTextureBlendMode(overlay_, SDL_BLENDMODE_MOD);
                SDL_SetTextureScaleMode(overlay_, SDL_SCALEMODE_NEAREST);
            }
        }
        overlay_pixels_.clear();
        overlay_pixels_.shrink_to_fit();
    }
    if (overlay_) SDL_RenderTexture(renderer_, overlay_, nullptr, &dst);

    if (!prompt.empty()) draw_text_lines({prompt}, true);
    if (!message_.empty()) {
        if (SDL_GetTicksNS() < message_until_ns_) draw_text_lines(message_, false);
        else message_.clear();
    }
    if (!capture_path_.empty()) {
        if (SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr)) {
            if (!SDL_SaveBMP(shot, capture_path_.c_str()))
                SDL_Log("knid: could not save %s: %s", capture_path_.c_str(), SDL_GetError());
            SDL_DestroySurface(shot);
        }
        capture_path_.clear();
    }
    SDL_RenderPresent(renderer_);
}

// Small on-screen text using SDL's built-in 8x8 debug font, scaled for high-DPI screens.
void Presenter::draw_text_lines(const std::vector<std::string>& lines, bool top) {
    int out_w = 0, out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &out_w, &out_h);
    const float scale = float(std::max(1, std::min(out_w / 480, out_h / 320)));
    const float w = float(out_w) / scale, h = float(out_h) / scale;
    const float line_h = 11.0f, pad = 6.0f;
    size_t longest = 0;
    for (const auto& l : lines) longest = std::max(longest, l.size());
    const float box_w = std::min(w - 2 * pad, float(longest) * 8.0f + 2 * pad);
    const float box_h = float(lines.size()) * line_h + 2 * pad - 3.0f;
    const float x = pad, y = top ? pad : h - box_h - pad;

    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 20, 12, 40, 200);  // night purple
    const SDL_FRect box{x, y, box_w, box_h};
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 214, 92, 255);  // star yellow
    for (size_t i = 0; i < lines.size(); ++i)
        SDL_RenderDebugText(renderer_, x + pad, y + pad + float(i) * line_h, lines[i].c_str());
    SDL_SetRenderScale(renderer_, 1.0f, 1.0f);
}

}  // namespace knid
