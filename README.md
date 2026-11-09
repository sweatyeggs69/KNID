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
> **Kirby: Nightmare in Dream Land (USA)**, which KNID checks by its SHA-1 before playing.
> KNID is an unofficial fan project. It is not affiliated with, endorsed or sponsored by Nintendo
> or HAL Laboratory. Kirby and all related names are trademarks of their respective owners.

KNID is a small app built around one game. Pick your ROM once, adjust a few settings, add a
shortcut, and from then on the game starts like any other app on your computer.

## Download

Get the latest version from the [Releases page](https://github.com/sweatyeggs69/KNID/releases/latest).

| System | File | First time you open it |
| --- | --- | --- |
| macOS 11 or newer (Apple Silicon and Intel) | `KNID-<version>-macOS.dmg` | Drag KNID into Applications. Right-click KNID and choose **Open**, then **Open** again (the app is not notarized yet). |
| Windows 10 or 11 (64-bit) | `KNID-<version>-Windows-x64.zip` | Unzip anywhere and run `KNID.exe`. If SmartScreen appears, choose **More info**, then **Run anyway**. |
| Linux (x86_64) | `KNID-<version>-x86_64.AppImage` | Run `chmod +x KNID-*.AppImage`, then start it. |

### Verify your download

Every release lists a **VirusTotal** scan link for each download and a `SHA256SUMS.txt` file with
SHA-256 checksums. See the notes of the [latest release](https://github.com/sweatyeggs69/KNID/releases/latest).

[![Scanned on VirusTotal](https://img.shields.io/badge/VirusTotal-scan%20links%20in%20release%20notes-394eff)](https://github.com/sweatyeggs69/KNID/releases/latest)

To check a checksum: `shasum -a 256 <file>` (macOS), `certutil -hashfile <file> SHA256` (Windows)
or `sha256sum <file>` (Linux), and compare it with the line in `SHA256SUMS.txt`.

## Getting started

1. **Download** KNID for your system and open it.
2. **Choose your ROM.** Click **Choose ROM...** and pick your dump of Kirby: Nightmare in Dream
   Land (USA). KNID verifies it and only remembers where the file is. It never copies, uploads or
   changes it.
3. **Play.** Press **Play**. To skip the launcher next time, use **Create Shortcut** (or
   Settings > Game > Shortcut) to add a shortcut that starts the game directly.

## Features

- **Themed launcher.** An original night-sky theme with your own game's title screen playing
  softly behind the menu. **Continue** picks up where you left off.
- **One settings menu (F1).** Everything in one place, in game or from the launcher: game, save
  states, display and controls.
- **Display options.** Pixel-perfect integer scaling or fit to window, widescreen views (16:10,
  16:9, 21:9), original GBA screen colors, frame blending, and grid, LCD or scanline overlays.
- **Smooth video.** Synced to your display's refresh rate for tear-free motion, with audio kept
  in step.
- **Controllers.** Keyboard and gamepads work out of the box; every button can be rebound,
  including stick dead zone.
- **Save states.** Ten slots plus an automatic one, with thumbnails, play time, labels, export
  and import, and an "undo last load" safety net. Quick save can overwrite one slot over and
  over, or start a new slot each time.
- **Bring your saves.** Import in-game saves from RetroArch (`.srm`), mGBA, VBA-M and others, or
  export yours. Your current save is backed up first.
- **Optional BIOS.** No BIOS file is needed. If you have your own GBA BIOS dump, KNID can use it
  and play the original boot animation.
- **Shortcuts.** One shortcut per place (Applications or Start menu, and Desktop), kept up to
  date instead of duplicated.
- **Updates built in.** KNID checks this repository's releases when it opens, shows what's new,
  and on your OK downloads the new version, checks it against the release's SHA-256 checksums
  and restarts into it. Your saves, save states and settings are never touched. Turn the check
  off in Settings > Updates.

## Controls

These are the defaults. Change them in Settings > Controls.

| GBA button | Keyboard | Gamepad |
| --- | --- | --- |
| D-pad | Arrow keys | D-pad or left stick |
| A | X | A (south) or Y (north) |
| B | Z | B (east) or X (west) |
| L | A | Left shoulder or left trigger |
| R | S | Right shoulder or right trigger |
| Start | Enter | Start |
| Select | Backspace or Right Shift | Back / Select |

| Key | Action |
| --- | --- |
| F1 | Open or close the settings menu |
| Esc | Open the settings menu in game (close it when open) |
| F5 | Save state to the current slot (overwrites it) |
| F9 | Save state to the next empty slot |
| F8 | Load state from the current slot |
| F6 / F7 | Previous / next save state slot |
| F11 | Toggle fullscreen |

## Where your files live

KNID keeps settings, in-game saves and save states in your user folder, never inside the app:

| System | Folder |
| --- | --- |
| macOS | `~/Library/Application Support/KNID/KNID/` |
| Windows | `%APPDATA%\KNID\KNID\` |
| Linux | `~/.local/share/KNID/KNID/` |

Inside it: `settings.ini`, `saves/` (in-game saves, with automatic backups in `saves/backups/`)
and `states/` (save states and their thumbnails).

## FAQ

**KNID says my ROM is not the supported dump.**
KNID supports the USA release of Kirby: Nightmare in Dream Land (game code `A7KE`, revision 0,
SHA-1 `37a476567d133c146fee6b5e2eb0b07a215da6b0`). Other regions, revisions, hacks or damaged
dumps are refused so that saves and settings always behave as expected.

**Can I use my save from RetroArch or another emulator?**
Yes. Open Settings > Game > Saves > **Import save...** and pick your `.sav` or `.srm` file. KNID
recognizes common formats, backs up your current save, and restarts the game with the imported
one. Save states from other emulators can't be imported; use the in-game save instead.

**Do I need a GBA BIOS?**
No. KNID has its own built-in replacement. If you own a GBA BIOS dump, you can select it in
Settings > Game > BIOS to see the original boot animation.

**Is this an emulator?**
KNID contains a GBA runtime that runs only this one game, wrapped in an app made for it. It is
not a general-purpose emulator.

<details>
<summary><b>Building from source</b></summary>

You need CMake 3.24 or newer, a C++20 compiler and Git. SDL3 and Dear ImGui are downloaded and
built automatically.

**macOS** (Xcode command line tools and Homebrew):

```sh
xcode-select --install
brew install cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKNID_VENDOR_SDL=ON
cmake --build build
./build/knid
```

App bundle and disk image (universal):

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DKNID_MACOS_BUNDLE=ON \
  -DKNID_VENDOR_SDL=ON -DKNID_BUILD_TOOLS=OFF "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build-release --target dmg
```

**Windows** (Visual Studio 2022 with C++, CMake, and vcpkg for zlib):

```bat
vcpkg install zlib:x64-windows-static
cmake -S . -B build -A x64 -DKNID_VENDOR_SDL=ON -DKNID_PACKAGE=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --target package_zip
```

Without zlib the build still works; save states are then stored uncompressed.

**Linux** (Debian/Ubuntu package names):

```sh
sudo apt install build-essential cmake ninja-build zlib1g-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev libwayland-dev \
  wayland-protocols libegl1-mesa-dev libgl1-mesa-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKNID_VENDOR_SDL=ON
cmake --build build
./build/knid
```

**Tests:**

```sh
ctest --test-dir build --output-on-failure
```

Tests that need the game look for `Kirby - Nightmare in Dream Land.gba` (and optionally
`gba_bios.bin`) in the source folder and are skipped when they are not there. Never commit
these files; `.gitignore` already excludes them, along with saves, states and screenshots.

Useful command-line options: `knid --help`. For example `--play` starts the game directly and
`--headless --frames N` runs without a window.

</details>

## Contributing

Bug reports and pull requests are welcome. Please keep game content out of the repository:
no ROMs, BIOS files, saves, save states, extracted assets or screenshots of the game. Only
original artwork (like the procedural icon in `docs/`) belongs here.

### For maintainers

Releases are built by GitHub Actions when a tag like `v0.1.0` is pushed. Bump `VERSION` in
`project(knid ...)` in `CMakeLists.txt` first: the tag must match it (the build checks), because
the in-app updater compares that version with the latest release. Keep the release file names
(`KNID-<version>-macOS.dmg`, `KNID-<version>-Windows-x64.zip`, `KNID-<version>-x86_64.AppImage`,
`SHA256SUMS.txt`), since the updater looks for them. To get VirusTotal links
in the release notes, add a repository secret named `VT_API_KEY` (a free VirusTotal account works:
Profile > API key) under **Settings > Secrets and variables > Actions**. Without it, releases are
still published, just without the scan links.

## License

KNID's source code is released under the [MIT License](LICENSE). Bundled open-source components
are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The Fredoka and
Pixelify Sans fonts are licensed under the SIL Open Font License 1.1
([Fredoka](assets/fonts/Fredoka-OFL.txt), [Pixelify Sans](assets/fonts/PixelifySans-OFL.txt)).
