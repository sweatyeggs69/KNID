# KNID: Kirby: Nightmare in Dream Land, native PC port

Goal: run the GBA game natively on macOS first, then Windows and Linux, via static recompilation.
You must supply your own legally dumped ROM. The ROM and anything extracted from it never go into git.

## Current state

- `knid_core`: loads a `.gba` file, validates the cartridge header and checks the SHA-1 against the
  expected USA dump (`A7KE`, rev 0, SHA-1 `37a476567d133c146fee6b5e2eb0b07a215da6b0`).
- `knid`: SDL3 shell that verifies the ROM and opens a resizable 240x160 window (4x, integer scaled)
  showing a test pattern. Esc quits, F11 toggles fullscreen. The recompiled game will draw here later.
- `knid_romcheck`: headless tool, `knid_romcheck --selftest` or `knid_romcheck path/to/rom.gba`.

## Build on macOS

```sh
xcode-select --install          # once, for the Apple compiler
brew install cmake ninja sdl3   # sdl3 is optional, CMake downloads it if missing

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/knid_romcheck "Kirby - Nightmare in Dream Land.gba"
./build/knid "Kirby - Nightmare in Dream Land.gba"
```

With no argument, `knid` looks for a `.gba` in the working directory, in `./rom`, and next to the binary.

Windows and Linux use the same CMake commands once we get there.
