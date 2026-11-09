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

ShortcutResult create_shortcut(ShortcutPlace place) {
    ShortcutResult r;
    fs::path exe = executable_path();
    if (exe.empty()) {
        r.message = "Could not find the KNID program on disk.";
        return r;
    }
    std::error_code ec;
#if defined(__APPLE__)
    fs::path dir = home_dir() / (place == ShortcutPlace::Desktop ? "Desktop" : "Applications");
    fs::create_directories(dir, ec);
    fs::path app = dir / (std::string(kShortcutName) + ".app");
    fs::path contents = app / "Contents";
    fs::remove_all(app, ec);
    fs::create_directories(contents / "MacOS", ec);
    fs::create_directories(contents / "Resources", ec);
    if (ec) {
        r.message = "Could not create " + app.string() + ": " + ec.message();
        return r;
    }

    // If we run from inside KNID.app, launch the bundle (keeps its Dock icon and name);
    // otherwise run the executable directly.
    std::string launch;
    fs::path bundle;
    for (fs::path p = exe.parent_path(); p.has_relative_path(); p = p.parent_path()) {
        if (p.extension() == ".app") { bundle = p; break; }
    }
    if (!bundle.empty()) {
        launch = "exec /usr/bin/open -a " + shell_quote(bundle.string()) + " --args --play\n";
        fs::path icon = bundle / "Contents" / "Resources" / "AppIcon.icns";
        if (fs::exists(icon, ec)) fs::copy_file(icon, contents / "Resources" / "AppIcon.icns", ec);
    } else {
        launch = "exec " + shell_quote(exe.string()) + " --play \"$@\"\n";
    }
    {
        std::ofstream s(contents / "MacOS" / "play");
        s << "#!/bin/sh\n# Created by KNID: starts the game directly.\n" << launch;
    }
    chmod((contents / "MacOS" / "play").c_str(), 0755);
    {
        std::ofstream p(contents / "Info.plist");
        p << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\"><dict>\n"
             "  <key>CFBundleName</key><string>" << xml_escape(kShortcutName) << "</string>\n"
             "  <key>CFBundleDisplayName</key><string>" << xml_escape(kShortcutName) << "</string>\n"
             "  <key>CFBundleIdentifier</key><string>tech.mcclain.knid.play</string>\n"
             "  <key>CFBundleExecutable</key><string>play</string>\n"
             "  <key>CFBundleIconFile</key><string>AppIcon</string>\n"
             "  <key>CFBundlePackageType</key><string>APPL</string>\n"
             "  <key>CFBundleShortVersionString</key><string>1.0</string>\n"
             "  <key>LSMinimumSystemVersion</key><string>11.0</string>\n"
             "</dict></plist>\n";
    }
    if (!fs::exists(contents / "MacOS" / "play", ec)) {
        r.message = "Could not write the shortcut.";
        return r;
    }
    r.ok = true;
    r.path = app;
    r.message = "Shortcut created:\n" + app.string();
    return r;
#elif defined(_WIN32)
    (void)place;
    // TODO: create a .lnk via IShellLinkW in the Start menu / on the Desktop with "--play".
    r.message = "Creating shortcuts is not available on Windows yet.";
    return r;
#else
    fs::path dir = place == ShortcutPlace::Desktop ? home_dir() / "Desktop"
                                                   : home_dir() / ".local" / "share" / "applications";
    fs::create_directories(dir, ec);
    fs::path file = dir / "knid-play.desktop";
    {
        std::ofstream d(file);
        d << "[Desktop Entry]\nType=Application\nName=" << kShortcutName << "\nExec=\"" << exe.string()
          << "\" --play\nTerminal=false\nCategories=Game;\n";
        if (!d) {
            r.message = "Could not write " + file.string();
            return r;
        }
    }
    chmod(file.c_str(), 0755);
    r.ok = true;
    r.path = file;
    r.message = "Shortcut created:\n" + file.string();
    return r;
#endif
}

}  // namespace knid
