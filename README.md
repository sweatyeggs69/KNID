<p align="center">
  <img src="docs/knid-icon.png" width="128" height="128" alt="KNID icon: a yellow star on a night sky">
</p>

<h1 align="center">KNID</h1>

<p align="center">
  Play your own copy of <b>Kirby: Nightmare in Dream Land</b> natively on macOS, Windows and Linux.
</p>

<p align="center">
  <a href="https://github.com/sweatyeggs69/KNID/actions/workflows/build.yml"><img alt="Build" src="https://github.com/sweatyeggs69/KNID/actions/workflows/build.yml/badge.svg"></a>
  <a href="https://github.com/sweatyeggs69/KNID/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/sweatyeggs69/KNID?include_prereleases&label=release"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux-ff8fba">
</p>

> [!IMPORTANT]
> KNID does not include the game. You need your own legally dumped ROM of
> **Kirby: Nightmare in Dream Land (USA)**, which KNID checks before playing.
> KNID is an unofficial fan project. It is not affiliated with, endorsed or sponsored by Nintendo
> or HAL Laboratory. Kirby and all related names are trademarks of their respective owners.

## Download

Get the latest version from the [Releases page](https://github.com/sweatyeggs69/KNID/releases/latest).

| System | File | First time you open it |
| --- | --- | --- |
| macOS 11 or newer (Apple Silicon and Intel) | `KNID-<version>-macOS.dmg` | Drag KNID into Applications. Right-click KNID and choose **Open**, then **Open** again. |
| Windows 10 or 11 (64-bit) | `KNID-<version>-Windows-x64.exe` | One self-contained file: put it in a folder you like and run it. If SmartScreen appears, choose **More info**, then **Run anyway**. |
| Linux (x86_64) | `KNID-<version>-Linux-x64.AppImage` | Run `chmod +x KNID-*.AppImage`, then start it. |

### Verify your download

Every release lists a **VirusTotal** scan link for each download and a `SHA256SUMS.txt` file with
SHA-256 checksums. See the notes of the [latest release](https://github.com/sweatyeggs69/KNID/releases/latest).

[![Scanned on VirusTotal](https://img.shields.io/badge/VirusTotal-scan%20links%20in%20release%20notes-394eff)](https://github.com/sweatyeggs69/KNID/releases/latest)

## Getting started

1. **Download** KNID for your system and open it.
2. **Choose your ROM.** Click **Choose ROM...** and pick your dump. KNID only remembers where the
   file is; it never copies, uploads or changes it.
3. **Play.** Press **Play**. Use **Create Shortcut** to start the game directly next time.

## Features

- **Native on macOS, Windows and Linux**, with smooth, tear-free video and sound.
- **Themed launcher** with your game's title screen behind it and **Continue** to pick up where you left off.
- **One settings menu (F1)** for display (scaling, widescreen, screen effects) and controls.
- **Controllers and remapping** for keyboard and gamepads.
- **Save states**: ten slots plus Auto, with thumbnails, quick save/load, save to a new slot, and
  optional auto-load on launch.
- **Import saves** from RetroArch, mGBA, VBA-M and others.
- **RetroAchievements**, optional: sign in to earn achievements.
- **Automatic updates** with your OK, without touching your saves.
- **Optional BIOS** with the original boot animation (not required).
- **One-click shortcut** to start the game directly.
- **Works with [RecompLauncher](https://recomplauncher.com)**.

### Works with RecompLauncher

KNID works with [RecompLauncher](https://recomplauncher.com), which installs and updates it from
these releases. Pick your ROM there and it is checked against the verified USA dump, set as KNID's
game, and started with Play. The display settings show on the game's page too.

## Controls

Defaults; change them in Settings > Controls.

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| D-pad | Arrow keys | D-pad or left stick |
| A / B | X / Z | South / East (also North / West) |
| L / R | A / S | Shoulders or triggers |
| Start / Select | Enter / Backspace | Start / Back |
| Settings menu | F1 or Esc | |
| Quick save / load | F5 / F8 | Back + R / Back + L |
| Save to new slot | F9 | |
| Previous / next slot | F6 / F7 | Back + D-pad left / right |
| Fullscreen | F11 | |

## FAQ

**Which version of the game is supported?**
Kirby: Nightmare in Dream Land, USA release (game code `A7KE`, revision 0).

**KNID says my ROM isn't the supported dump.**
Other regions, revisions, hacks and damaged dumps are refused so saves and settings always work.
Dump your USA cartridge again and pick that file.

**Do I need a GBA BIOS?**
No. KNID has a built-in replacement. If you own a BIOS dump, Settings > Game > BIOS shows the
original boot animation.

**Where are my saves?**
In your user folder, never inside the app: `~/Library/Application Support/KNID/KNID/` (macOS),
`%APPDATA%\KNID\KNID\` (Windows) or `~/.local/share/KNID/KNID/` (Linux). Settings > Game >
Saves > **Show save folder** opens it.

<details>
<summary><b>Building from source</b></summary>

You need CMake 3.24+, a C++20 compiler and Git. SDL3 and Dear ImGui are fetched automatically.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKNID_VENDOR_SDL=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/knid
```

- **macOS app and DMG:** add `-DKNID_MACOS_BUNDLE=ON "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"`
  and build the `dmg` target.
- **Windows:** Visual Studio 2022; `vcpkg install zlib:x64-windows-static` and pass the vcpkg
  toolchain with `-DVCPKG_TARGET_TRIPLET=x64-windows-static -DKNID_PACKAGE=ON`, then build
  `package_exe`.
- **Linux:** install the SDL build dependencies (X11/Wayland, ALSA/PulseAudio, `zlib1g-dev`); see
  `.github/workflows/build.yml` for the full list.

Tests that need the game look for your ROM in the source folder and skip themselves otherwise.
Never commit ROMs, BIOS files, saves, states or screenshots of the game.

</details>

<details>
<summary><b>For maintainers</b></summary>

- **Releases:** bump `VERSION` in `project(knid ...)` in `CMakeLists.txt`, then push a matching tag
  like `v1.0.0`. GitHub Actions builds all three platforms and publishes the release. Keep the file
  names (`KNID-<version>-macOS.dmg`, `KNID-<version>-Windows-x64.exe`,
  `KNID-<version>-Linux-x64.AppImage`, `SHA256SUMS.txt`, plus `recomplauncher.json` when the repository
  has it); the in-app updater looks for them.
- **VirusTotal links:** add a repository secret `VT_API_KEY` (free VirusTotal account, Profile >
  API key) under Settings > Secrets and variables > Actions.

</details>

## Contributing

Bug reports and pull requests are welcome. Keep game content out of the repository: no ROMs, BIOS
files, saves, save states, extracted assets or screenshots of the game.

## License

[MIT](LICENSE). Bundled open-source components and fonts are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
