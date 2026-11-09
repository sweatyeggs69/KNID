#include "app/platform.h"

#include <cstdlib>
#include <fstream>
#include <system_error>

#include "app/config.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace knid {
namespace fs = std::filesystem;

namespace {
constexpr const char* kShortcutName = "Kirby Nightmare in Dream Land";

fs::path home_dir() {
    if (const char* h = std::getenv("HOME")) return h;
#if defined(_WIN32)
    if (const char* h = std::getenv("USERPROFILE")) return h;
#endif
    return {};
}

std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

std::string xml_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}
}  // namespace

VerifiedRom verify_rom(const fs::path& path) {
    VerifiedRom v;
    std::error_code ec;
    v.path = fs::absolute(path, ec);
    if (ec) v.path = path;
    if (!fs::exists(v.path, ec)) {
        v.error = "The ROM file could not be found:\n" + v.path.string();
        return v;
    }
    auto loaded = load_rom(v.path);
    if (!loaded.ok) {
        v.error = "That file is not a Game Boy Advance ROM.";
        return v;
    }
    if (!loaded.rom.hash_matches) {
        const auto& h = loaded.rom.header;
        v.error = "That ROM is not the supported dump.\n"
                  "Found: " + h.title + " [" + h.game_code + "] rev " + std::to_string(h.version) + "\n"
                  "Needed: Kirby: Nightmare in Dream Land (USA) [A7KE] rev 0\n"
                  "SHA-1 " + loaded.rom.sha1;
        return v;
    }
    v.rom = std::move(loaded.rom);
    v.ok = true;
    return v;
}

fs::path save_path_for(const Config& config, const VerifiedRom& rom) {
    fs::path dir = config.pref_dir() / "saves";
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path save = dir / (rom.rom.sha1 + ".sav");
    if (!fs::exists(save, ec)) {
        fs::path legacy = rom.path;
        legacy.replace_extension(".sav");
        if (fs::exists(legacy, ec)) fs::copy_file(legacy, save, ec);
    }
    return save;
}

fs::path executable_path() {
#if defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::error_code ec;
        fs::path p = fs::canonical(buf, ec);
        return ec ? fs::path(buf) : p;
    }
    return {};
#elif defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? fs::path(std::wstring(buf, n)) : fs::path();
#else
    std::error_code ec;
    return fs::read_symlink("/proc/self/exe", ec);
#endif
}

namespace {
constexpr const char* kShortcutId = "tech.mcclain.knid.shortcut";
constexpr const char* kOldShortcutId = "tech.mcclain.knid.play";  // earlier builds
}  // namespace

fs::path shortcut_path(ShortcutPlace place) {
#if defined(__APPLE__)
    fs::path dir = home_dir() / (place == ShortcutPlace::Desktop ? "Desktop" : "Applications");
    return dir / (std::string(kShortcutName) + ".app");
#elif defined(_WIN32)
    (void)place;
    return {};  // TODO: "<Start menu or Desktop>/Kirby Nightmare in Dream Land.lnk"
#else
    fs::path dir = place == ShortcutPlace::Desktop ? home_dir() / "Desktop"
                                                   : home_dir() / ".local" / "share" / "applications";
    return dir / "knid-play.desktop";
#endif
}

ShortcutState shortcut_state(ShortcutPlace place) {
    fs::path p = shortcut_path(place);
    std::error_code ec;
    if (p.empty() || !fs::exists(fs::symlink_status(p, ec)) ) return ShortcutState::None;
#if defined(__APPLE__)
    std::ifstream f(p / "Contents" / "Info.plist");
    std::string plist((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    bool ours = plist.find(kShortcutId) != std::string::npos || plist.find(kOldShortcutId) != std::string::npos;
#else
    std::ifstream f(p);
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    bool ours = text.find("X-KNID-Shortcut=true") != std::string::npos;
#endif
    return ours ? ShortcutState::Ours : ShortcutState::Foreign;
}

bool move_to_trash(const fs::path& p, fs::path* where) {
    std::error_code ec;
#if defined(__APPLE__) || !defined(_WIN32)
#if defined(__APPLE__)
    fs::path trash = home_dir() / ".Trash";
#else
    fs::path trash = home_dir() / ".local" / "share" / "Trash" / "files";
#endif
    fs::create_directories(trash, ec);
    fs::path dst = trash / p.filename();
    for (int n = 2; fs::exists(fs::symlink_status(dst, ec)); ++n)
        dst = trash / (p.stem().string() + " " + std::to_string(n) + p.extension().string());
    fs::rename(p, dst, ec);
    if (ec) return false;
    if (where) *where = dst;
    return true;
#else
    (void)p; (void)where;
    return false;  // TODO: SHFileOperationW with FOF_ALLOWUNDO
#endif
}

ShortcutResult remove_shortcut(ShortcutPlace place) {
    ShortcutResult r;
    fs::path p = shortcut_path(place);
    switch (shortcut_state(place)) {
        case ShortcutState::None: r.ok = true; r.message = "There is no shortcut to remove."; return r;
        case ShortcutState::Foreign: r.message = p.string() + " was not created by KNID, so it was left alone."; return r;
        case ShortcutState::Ours: break;
    }
    fs::path where;
    if (!move_to_trash(p, &where)) { r.message = "Could not move " + p.string() + " to the Trash."; return r; }
    r.ok = true;
    r.path = where;
    r.message = "Shortcut moved to the Trash.";
    return r;
}

ShortcutResult create_shortcut(ShortcutPlace place, bool replace_foreign) {
    ShortcutResult r;
    fs::path exe = executable_path();
    if (exe.empty()) {
        r.message = "Could not find the KNID program on disk.";
        return r;
    }
    fs::path target = shortcut_path(place);
    if (target.empty()) {
        r.message = "Creating shortcuts is not available on this system yet.";
        return r;
    }
    ShortcutState state = shortcut_state(place);
    if (state == ShortcutState::Foreign) {
        if (!replace_foreign) {
            r.needs_confirm = true;
            r.path = target;
            r.message = target.string() + " already exists and was not created by KNID. Replace it? "
                        "(It will be moved to the Trash.)";
            return r;
        }
        if (!move_to_trash(target)) {
            r.message = "Could not move the existing " + target.string() + " to the Trash.";
            return r;
        }
    }
    bool updating = state == ShortcutState::Ours;
    std::error_code ec;
#if defined(__APPLE__)
    fs::path contents = target / "Contents";
    // Ours: rewrite the files in place (same bundle, so the Dock/Finder entry stays put).
    fs::create_directories(contents / "MacOS", ec);
    fs::create_directories(contents / "Resources", ec);
    if (ec) {
        r.message = "Could not create " + target.string() + ": " + ec.message();
        return r;
    }
    std::string launch;
    fs::path bundle;
    for (fs::path p = exe.parent_path(); p.has_relative_path(); p = p.parent_path()) {
        if (p.extension() == ".app") { bundle = p; break; }
    }
    if (!bundle.empty()) {
        launch = "exec /usr/bin/open -a " + shell_quote(bundle.string()) + " --args --play\n";
        fs::path icon = bundle / "Contents" / "Resources" / "AppIcon.icns";
        if (fs::exists(icon, ec))
            fs::copy_file(icon, contents / "Resources" / "AppIcon.icns", fs::copy_options::overwrite_existing, ec);
    } else {
        launch = "exec " + shell_quote(exe.string()) + " --play \"$@\"\n";
    }
    {
        std::ofstream s(contents / "MacOS" / "play", std::ios::trunc);
        s << "#!/bin/sh\n# Created by KNID: starts the game directly.\n" << launch;
    }
    chmod((contents / "MacOS" / "play").c_str(), 0755);
    {
        std::ofstream p(contents / "Info.plist", std::ios::trunc);
        p << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\"><dict>\n"
             "  <key>CFBundleName</key><string>" << xml_escape(kShortcutName) << "</string>\n"
             "  <key>CFBundleDisplayName</key><string>" << xml_escape(kShortcutName) << "</string>\n"
             "  <key>CFBundleIdentifier</key><string>" << kShortcutId << "</string>\n"
             "  <key>CFBundleExecutable</key><string>play</string>\n"
             "  <key>CFBundleIconFile</key><string>AppIcon</string>\n"
             "  <key>CFBundlePackageType</key><string>APPL</string>\n"
             "  <key>CFBundleShortVersionString</key><string>1.0</string>\n"
             "  <key>LSMinimumSystemVersion</key><string>11.0</string>\n"
             "</dict></plist>\n";
    }
    // Let Finder/Launch Services notice the updated bundle.
    fs::last_write_time(target, fs::file_time_type::clock::now(), ec);
#else
    fs::create_directories(target.parent_path(), ec);
    {
        std::ofstream d(target, std::ios::trunc);
        d << "[Desktop Entry]\nType=Application\nName=" << kShortcutName << "\nExec=\"" << exe.string()
          << "\" --play\nTerminal=false\nCategories=Game;\nX-KNID-Shortcut=true\n";
        if (!d) {
            r.message = "Could not write " + target.string();
            return r;
        }
    }
    chmod(target.c_str(), 0755);
#endif
    r.ok = true;
    r.path = target;
    r.message = std::string(updating ? "Shortcut updated:\n" : "Shortcut created:\n") + target.string();
    return r;
}

}  // namespace knid
