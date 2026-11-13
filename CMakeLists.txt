cmake_minimum_required(VERSION 3.24)
# Windows: static MSVC runtime so KNID.exe runs without installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(knid LANGUAGES C CXX VERSION 1.5.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

option(KNID_FETCH_SDL "Download and build SDL3 if it is not installed" ON)
option(KNID_BUILD_TESTS "Build headless tests" ON)
option(KNID_BUILD_TOOLS "Build analysis tools (knid_disasm, needs Capstone)" ON)
option(KNID_MACOS_BUNDLE "Build KNID.app (and the `dmg` target) on macOS" OFF)
option(KNID_VENDOR_SDL "Always build SDL3 from source (static) instead of using a system install" OFF)
option(KNID_PACKAGE "Add packaging targets (Windows zip, Linux AppDir install)" OFF)

# App icon renderer (original procedural artwork); used for the macOS .icns, Windows .ico and
# the Linux icon.
add_executable(knid_icon tools/knid_icon/main.cpp)

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

# Save import/export and backups (no SDL).
add_library(knid_saves STATIC src/app/save_import.cpp src/app/fsutil.cpp)
target_include_directories(knid_saves PUBLIC src)
if(WIN32)
    target_link_libraries(knid_saves PUBLIC shell32 ole32 uuid)
endif()
if(NOT MSVC)
    target_compile_options(knid_saves PRIVATE -Wall -Wextra)
endif()

# Save state slots (no SDL). Uses the system zlib when present (macOS/Linux); otherwise states
# are stored uncompressed.
add_library(knid_states STATIC src/app/states.cpp)
target_include_directories(knid_states PUBLIC src)
target_link_libraries(knid_states PUBLIC knid_gba knid_saves)
target_compile_definitions(knid_states PRIVATE KNID_VERSION="${PROJECT_VERSION}")
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(knid_states PRIVATE ZLIB::ZLIB)
    target_compile_definitions(knid_states PRIVATE KNID_HAVE_ZLIB=1)
endif()
if(NOT MSVC)
    target_compile_options(knid_states PRIVATE -Wall -Wextra)
endif()

# Updates from GitHub Releases: versions, release JSON, SHA-256, zip unpacking. No SDL.
add_library(knid_update_core STATIC src/update/sha256.cpp src/update/update_core.cpp)
target_include_directories(knid_update_core PUBLIC src)
if(ZLIB_FOUND)
    target_link_libraries(knid_update_core PRIVATE ZLIB::ZLIB)
    target_compile_definitions(knid_update_core PRIVATE KNID_HAVE_ZLIB=1)
endif()
if(MSVC)
    target_compile_options(knid_update_core PRIVATE /W4)
else()
    target_compile_options(knid_update_core PRIVATE -Wall -Wextra)
endif()

# Display post-processing (scaling layout, GBA colors, frame blending, overlays). No SDL.
add_library(knid_display STATIC src/options/display.cpp)
target_include_directories(knid_display PUBLIC src)
if(MSVC)
    target_compile_options(knid_display PRIVATE /W4)
else()
    target_compile_options(knid_display PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GBA runtime (interpreter, PPU, APU, BIOS HLE). No SDL dependency.
add_library(knid_gba STATIC
    src/gba/apu.cpp
    src/gba/bios.cpp
    src/gba/bus.cpp
    src/gba/cpu.cpp
    src/gba/ppu.cpp
    src/gba/state.cpp
    src/gba/system.cpp
    src/gba/wide_level.cpp
)
target_include_directories(knid_gba PUBLIC src)
if(MSVC)
    target_compile_options(knid_gba PRIVATE /W4)
else()
    target_compile_options(knid_gba PRIVATE -Wall -Wextra)
endif()

# SDL3: use a system install if present (brew install sdl3, pacman -S sdl3, vcpkg), else fetch it.
if(NOT KNID_VENDOR_SDL)
    find_package(SDL3 CONFIG QUIET)
endif()
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

    # RetroAchievements: rcheevos (MIT) for the rc_client runtime, and stb_image (public domain)
    # to decode PNG badges. Both pinned; built from source as plain C/C++ files.
    FetchContent_Declare(rcheevos
        GIT_REPOSITORY https://github.com/RetroAchievements/rcheevos.git
        GIT_TAG v12.5.0
        GIT_SHALLOW TRUE)
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 4e9b368bfc071e97206ea5c6adba68dd0cbf8897)
    FetchContent_MakeAvailable(rcheevos stb)
    set(rc ${rcheevos_SOURCE_DIR}/src)
    add_library(rcheevos STATIC
        ${rc}/rc_client.c ${rc}/rc_compat.c ${rc}/rc_util.c ${rc}/rc_version.c
        ${rc}/rapi/rc_api_common.c ${rc}/rapi/rc_api_info.c ${rc}/rapi/rc_api_runtime.c ${rc}/rapi/rc_api_user.c
        ${rc}/rcheevos/alloc.c ${rc}/rcheevos/condition.c ${rc}/rcheevos/condset.c ${rc}/rcheevos/consoleinfo.c
        ${rc}/rcheevos/format.c ${rc}/rcheevos/lboard.c ${rc}/rcheevos/memref.c ${rc}/rcheevos/operand.c
        ${rc}/rcheevos/rc_validate.c ${rc}/rcheevos/richpresence.c ${rc}/rcheevos/runtime.c
        ${rc}/rcheevos/runtime_progress.c ${rc}/rcheevos/trigger.c ${rc}/rcheevos/value.c
        ${rc}/rhash/md5.c)
    target_include_directories(rcheevos PUBLIC ${rcheevos_SOURCE_DIR}/include ${rc})
    target_include_directories(rcheevos SYSTEM PUBLIC ${stb_SOURCE_DIR})
    set_target_properties(rcheevos PROPERTIES C_STANDARD 99)

    # Options panel (owned by the options work; display.* / input.* settings).
    add_library(knid_options STATIC
        src/options/options_panel.cpp
        src/options/presenter.cpp
        src/options/input.cpp
    )
    target_include_directories(knid_options PUBLIC src)
    target_link_libraries(knid_options PUBLIC imgui knid_display knid_app knid_states SDL3::SDL3)

    # App layer: settings, launcher UI. Uses SDL (pref path, dialogs).
    add_library(knid_app STATIC
        src/app/config.cpp
    )
    target_include_directories(knid_app PUBLIC src)
    target_link_libraries(knid_app PUBLIC SDL3::SDL3)

    # Launcher: first-run ROM setup, themed menu, shortcuts, game session. The DM Sans font
    # (SIL OFL) and a few Lucide icons (ISC), both in assets/fonts, are compiled in.
    set(knid_font_cpp "")
    foreach(pair "dmsans_regular;DMSans-Regular.ttf" "dmsans_semibold;DMSans-SemiBold.ttf"
                 "dmsans_bold;DMSans-Bold.ttf" "lucide_icons;lucide-icons.ttf")
        list(GET pair 0 name)
        list(GET pair 1 file)
        set(out "${CMAKE_CURRENT_BINARY_DIR}/font_${name}.cpp")
        add_custom_command(OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/assets/fonts/${file}
                    -DOUTPUT=${out} -DSYMBOL=knid_font_${name} -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
            DEPENDS assets/fonts/${file} cmake/embed.cmake
            COMMENT "Embedding ${file}")
        list(APPEND knid_font_cpp "${out}")
    endforeach()
    # The KNID logo for the launcher card (a small palette PNG of docs/knid-logo.png).
    set(out "${CMAKE_CURRENT_BINARY_DIR}/image_logo.cpp")
    add_custom_command(OUTPUT "${out}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/assets/logo/knid-logo.png -DOUTPUT=${out}
                -DSYMBOL=knid_image_logo -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS assets/logo/knid-logo.png cmake/embed.cmake
        COMMENT "Embedding the KNID logo")
    list(APPEND knid_font_cpp "${out}")
    # The app icon (512x512, rendered by knid_icon), written into the shortcuts the launcher
    # creates so they carry the KNID icon even from a plain build/knid.
    set(icon_png "${CMAKE_CURRENT_BINARY_DIR}/knid-icon-512.png")
    set(out "${CMAKE_CURRENT_BINARY_DIR}/image_icon.cpp")
    add_custom_command(OUTPUT "${out}"
        COMMAND knid_icon --png 512 "${icon_png}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${icon_png} -DOUTPUT=${out}
                -DSYMBOL=knid_image_icon -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS knid_icon cmake/embed.cmake
        COMMENT "Embedding the KNID app icon")
    list(APPEND knid_font_cpp "${out}")
    # License texts compiled in (shown in F1 > About and by --licenses).
    foreach(pair "license;LICENSE" "notices;THIRD_PARTY_NOTICES.md" "dmsans_ofl;assets/fonts/DMSans-OFL.txt")
        list(GET pair 0 name)
        list(GET pair 1 file)
        set(out "${CMAKE_CURRENT_BINARY_DIR}/text_${name}.cpp")
        add_custom_command(OUTPUT "${out}"
            COMMAND "${CMAKE_COMMAND}" -DINPUT=${PROJECT_SOURCE_DIR}/${file} -DOUTPUT=${out}
                    -DSYMBOL=knid_text_${name} -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
            DEPENDS ${file} cmake/embed.cmake
            COMMENT "Embedding ${file}")
        list(APPEND knid_font_cpp "${out}")
    endforeach()
    add_library(knid_launcher STATIC
        src/app/achievements.cpp
        src/app/credentials.cpp
        src/app/http.cpp
        src/app/app.cpp
        src/app/platform.cpp
        src/app/theme.cpp
        src/app/theme_colors.cpp
        src/app/licenses.cpp
        src/app/toast.cpp
        ${knid_font_cpp}
    )
    target_include_directories(knid_launcher PUBLIC src)
    target_link_libraries(knid_launcher PUBLIC knid_options knid_gba knid_core knid_saves knid_states imgui SDL3::SDL3)
    target_link_libraries(knid_launcher PRIVATE rcheevos)
    target_compile_definitions(knid_launcher PRIVATE KNID_VERSION="${PROJECT_VERSION}")
    if(APPLE)
        target_link_libraries(knid_launcher PRIVATE "-framework Security" "-framework CoreFoundation")
    elseif(WIN32)
        target_link_libraries(knid_launcher PRIVATE winhttp advapi32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(knid_launcher PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    endif()
    if(NOT MSVC)
        target_compile_options(knid_launcher PRIVATE -Wall -Wextra)
    endif()

    # In-app updater: downloads (WinHTTP / curl), install per OS, and its settings tab.
    add_library(knid_updater STATIC src/update/http.cpp src/update/updater.cpp src/update/update_ui.cpp)
    target_include_directories(knid_updater PUBLIC src)
    target_link_libraries(knid_updater PUBLIC knid_update_core knid_app imgui SDL3::SDL3)
    target_compile_definitions(knid_updater PUBLIC KNID_VERSION="${PROJECT_VERSION}")
    if(WIN32)
        target_link_libraries(knid_updater PRIVATE winhttp)
    endif()
    if(NOT MSVC)
        target_compile_options(knid_updater PRIVATE -Wall -Wextra)
    endif()
    target_link_libraries(knid_launcher PUBLIC knid_updater)

    add_executable(knid src/main.cpp)
    if(WIN32)
        # GUI program (no console window), KNID.exe with the star icon and version info.
        set(knid_ico "${CMAKE_CURRENT_BINARY_DIR}/knid.ico")
        add_custom_command(OUTPUT "${knid_ico}" COMMAND knid_icon --ico "${knid_ico}" DEPENDS knid_icon
                           COMMENT "Rendering knid.ico")
        configure_file(packaging/windows/knid.rc.in "${CMAKE_CURRENT_BINARY_DIR}/knid.rc" @ONLY)
        target_sources(knid PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/knid.rc" "${knid_ico}")
        set_target_properties(knid PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME KNID)
        target_link_libraries(knid PRIVATE ole32 shell32 uuid)
    endif()
    target_link_libraries(knid PRIVATE knid_launcher knid_core knid_gba knid_options imgui SDL3::SDL3)
    if(APPLE AND KNID_MACOS_BUNDLE)
        include(cmake/MacBundle.cmake)
        knid_configure_bundle(knid)
    elseif(APPLE)
        # Plain (unbundled) build/knid: embed an Info.plist so the menu bar says KNID, not knid.
        set(knid_plist "${CMAKE_CURRENT_BINARY_DIR}/knid-embedded-Info.plist")
        file(WRITE "${knid_plist}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n"
            "<key>CFBundleName</key><string>KNID</string>\n"
            "<key>CFBundleDisplayName</key><string>KNID</string>\n"
            "<key>CFBundleShortVersionString</key><string>${PROJECT_VERSION}</string>\n"
            "</dict></plist>\n")
        target_link_options(knid PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${knid_plist}")
        set_property(TARGET knid APPEND PROPERTY LINK_DEPENDS "${knid_plist}")
    endif()
    if(KNID_PACKAGE AND WIN32)
        # Release file: the single self-contained KNID.exe (static runtime, SDL, zlib; licenses
        # embedded, see F1 > About and --licenses), copied under its release name.
        set(knid_release_exe "${CMAKE_CURRENT_BINARY_DIR}/KNID-${PROJECT_VERSION}-Windows-x64.exe")
        add_custom_target(package_exe
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:knid>" "${knid_release_exe}"
            DEPENDS knid
            COMMENT "Building ${knid_release_exe}"
            VERBATIM)
    endif()
    if(KNID_PACKAGE AND UNIX AND NOT APPLE)
        # Portable Linux binary: static libstdc++/libgcc; SDL is static and loads X11/Wayland/
        # ALSA/PulseAudio/PipeWire at runtime. Installed into an AppDir by the workflow.
        target_link_options(knid PRIVATE -static-libstdc++ -static-libgcc)
        set(knid_png "${CMAKE_CURRENT_BINARY_DIR}/knid.png")
        add_custom_command(OUTPUT "${knid_png}" COMMAND knid_icon --png 256 "${knid_png}" DEPENDS knid_icon
                           COMMENT "Rendering knid.png")
        add_custom_target(knid_linux_icon ALL DEPENDS "${knid_png}")
        install(TARGETS knid RUNTIME DESTINATION bin)
        install(FILES packaging/linux/knid.desktop DESTINATION share/applications)
        install(FILES "${knid_png}" DESTINATION share/icons/hicolor/256x256/apps)
        install(FILES LICENSE THIRD_PARTY_NOTICES.md packaging/README-Linux.txt DESTINATION share/doc/knid)
        install(FILES assets/fonts/DMSans-OFL.txt assets/fonts/Lucide-LICENSE.txt DESTINATION share/doc/knid)
    endif()
else()
    message(WARNING "SDL3 not found and KNID_FETCH_SDL is OFF: only building knid_core")
endif()

if(KNID_BUILD_TESTS)
    enable_testing()
    add_executable(knid_romcheck tests/romcheck.cpp)
    target_link_libraries(knid_romcheck PRIVATE knid_core)
    add_test(NAME sha1_selftest COMMAND knid_romcheck --selftest)
    add_executable(knid_display_test tests/display_test.cpp)
    target_link_libraries(knid_display_test PRIVATE knid_display)
    add_test(NAME display_options COMMAND knid_display_test)
    add_executable(knid_save_test tests/save_import_test.cpp)
    target_link_libraries(knid_save_test PRIVATE knid_saves)
    add_test(NAME save_import COMMAND knid_save_test)
    add_executable(knid_states_test tests/states_test.cpp)
    target_link_libraries(knid_states_test PRIVATE knid_states)
    add_test(NAME save_state_slots COMMAND knid_states_test)
    add_executable(knid_cheats_test tests/cheats_test.cpp)
    target_include_directories(knid_cheats_test PRIVATE src)
    add_test(NAME cheats COMMAND knid_cheats_test)
    add_executable(knid_update_test tests/update_test.cpp)
    target_link_libraries(knid_update_test PRIVATE knid_update_core)
    target_compile_definitions(knid_update_test PRIVATE KNID_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data")
    add_test(NAME updates COMMAND knid_update_test)
    add_executable(knid_state_test tests/state_test.cpp)
    target_link_libraries(knid_state_test PRIVATE knid_gba knid_core)
    # Uses the user's own ROM when it sits in the source folder (it is never committed).
    target_compile_definitions(knid_state_test PRIVATE
        KNID_TEST_ROM="${PROJECT_SOURCE_DIR}/Kirby - Nightmare in Dream Land.gba"
        KNID_TEST_BIOS="${PROJECT_SOURCE_DIR}/gba_bios.bin")
    add_test(NAME save_state_determinism COMMAND knid_state_test)
    set_tests_properties(save_state_determinism PROPERTIES SKIP_RETURN_CODE 77)
    if(TARGET knid_launcher)
        # RetroAchievements glue against canned server replies (offline).
        add_executable(knid_achievements_test tests/achievements_test.cpp)
        target_link_libraries(knid_achievements_test PRIVATE knid_launcher rcheevos)
        add_test(NAME achievements COMMAND knid_achievements_test)
    endif()
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
