cmake_minimum_required(VERSION 3.24)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(knid LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KNID_FETCH_SDL "Download and build SDL3 if it is not installed" ON)
option(KNID_BUILD_TESTS "Build headless tests" ON)
option(KNID_BUILD_TOOLS "Build analysis tools (knid_disasm, needs Capstone)" ON)

# Core: ROM loading and verification. No SDL dependency, so it can be tested headless.
add_library(knid_core STATIC
    src/rom.cpp
    src/sha1.cpp
)
target_include_directories(knid_core PUBLIC src)
if(MSVC)
    target_compile_options(knid_core PRIVATE /W4)
else()
    target_compile_options(knid_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GBA runtime (interpreter, PPU, APU, BIOS HLE). No SDL dependency.
add_library(knid_gba STATIC
    src/gba/apu.cpp
    src/gba/bios.cpp
    src/gba/bus.cpp
    src/gba/cpu.cpp
    src/gba/ppu.cpp
    src/gba/system.cpp
)
target_include_directories(knid_gba PUBLIC src)
if(MSVC)
    target_compile_options(knid_gba PRIVATE /W4)
else()
    target_compile_options(knid_gba PRIVATE -Wall -Wextra)
endif()

# SDL3: use a system install if present (brew install sdl3, pacman -S sdl3, vcpkg), else fetch it.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND AND KNID_FETCH_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.26
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3)
    # Dear ImGui (core + SDL3 platform + SDL_Renderer3 backends), pinned.
    include(FetchContent)
    FetchContent_Declare(imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui_src)
    add_library(imgui STATIC
        ${imgui_src_SOURCE_DIR}/imgui.cpp
        ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_src_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_src_SOURCE_DIR} ${imgui_src_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    # Options panel (owned by the options work; display.* / input.* settings).
    add_library(knid_options STATIC
        src/options/options_panel.cpp
    )
    target_include_directories(knid_options PUBLIC src)
    target_link_libraries(knid_options PUBLIC imgui)

    # App layer: settings, launcher UI. Uses SDL (pref path, dialogs).
    add_library(knid_app STATIC
        src/app/config.cpp
    )
    target_include_directories(knid_app PUBLIC src)
    target_link_libraries(knid_app PUBLIC SDL3::SDL3)

    add_executable(knid src/main.cpp)
    target_link_libraries(knid PRIVATE knid_core knid_gba knid_app knid_options imgui SDL3::SDL3)
else()
    message(WARNING "SDL3 not found and KNID_FETCH_SDL is OFF: only building knid_core")
endif()

if(KNID_BUILD_TESTS)
    enable_testing()
    add_executable(knid_romcheck tests/romcheck.cpp)
    target_link_libraries(knid_romcheck PRIVATE knid_core)
    add_test(NAME sha1_selftest COMMAND knid_romcheck --selftest)
endif()

# Analysis tools. Their output is derived from the ROM and goes to generated/ (gitignored).
if(KNID_BUILD_TOOLS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CAPSTONE IMPORTED_TARGET capstone>=5)
    endif()
    if(TARGET PkgConfig::CAPSTONE)
        add_executable(knid_disasm tools/knid_disasm/main.cpp)
        target_link_libraries(knid_disasm PRIVATE knid_core PkgConfig::CAPSTONE)
        if(NOT MSVC)
            target_compile_options(knid_disasm PRIVATE -Wall -Wextra)
        endif()
    else()
        message(WARNING "Capstone not found via pkg-config: skipping knid_disasm (brew install capstone pkgconf)")
    endif()
endif()
