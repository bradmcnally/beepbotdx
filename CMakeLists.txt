cmake_minimum_required(VERSION 3.16)
project(beepbotdx_desktop)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CARDPUTER_ZERO "Use the Cardputer Zero layout and keymap" OFF)
option(CARDPUTER_ZERO_DEVICE "Use the Cardputer Zero Linux LCD and keypad" OFF)
if(CARDPUTER_ZERO_DEVICE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The zero-device preset requires Linux. Use zero for simulation.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(
    src
    src/core
    src/views
    ${SDL2_INCLUDE_DIRS}
)

add_definitions(-DDESKTOP_BUILD)

if(CARDPUTER_ZERO OR CARDPUTER_ZERO_DEVICE)
    add_definitions(-DCARDPUTER_ZERO -DSCREEN_WIDTH=320 -DSCREEN_HEIGHT=170)
elseif(SCREEN_W)
    add_definitions(-DSCREEN_WIDTH=${SCREEN_W})
endif()
if(SCREEN_H AND NOT CARDPUTER_ZERO AND NOT CARDPUTER_ZERO_DEVICE)
    add_definitions(-DSCREEN_HEIGHT=${SCREEN_H})
endif()

set(CORE_SOURCES
    src/core/app.cpp
    src/core/bloom_field.cpp
    src/core/canvas.cpp
    src/core/character.cpp
    src/core/pattern_gen.cpp
    src/core/project.cpp
    src/core/project_format.cpp
    src/core/sequencer.cpp
    src/core/sound_slot.cpp
    src/core/wav.cpp
    src/core/theme.cpp
)

set(VIEW_SOURCES
    src/views/sound_view.cpp
    src/views/pattern_select_view.cpp
    src/views/pattern_edit_view.cpp
    src/views/song_view.cpp
    src/views/play_view.cpp
    src/views/project_view.cpp
    src/views/settings_view.cpp
    src/views/project_info_view.cpp
)

set(PLATFORM_SOURCES
    src/platform_desktop/main_desktop.cpp
    src/platform_desktop/display.cpp
    src/platform_desktop/audio.cpp
    src/platform_desktop/input.cpp
    src/platform_desktop/storage.cpp
    src/platform_desktop/power.cpp
    src/platform_desktop/memory.cpp
    src/platform_desktop/led.cpp
)

add_executable(beepbotdx
    ${CORE_SOURCES}
    ${VIEW_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_directories(beepbotdx PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(beepbotdx ${SDL2_LIBRARIES})

if(CARDPUTER_ZERO_DEVICE)
    target_sources(beepbotdx PRIVATE platform_zero/native_io.cpp)
    target_include_directories(beepbotdx PRIVATE platform_zero)
    target_compile_definitions(beepbotdx PRIVATE CARDPUTER_ZERO_DEVICE)

    install(TARGETS beepbotdx RUNTIME DESTINATION share/APPLaunch/apps/beepbotdx)
    install(PROGRAMS platform_zero/run_beepbotdx.sh
        DESTINATION share/APPLaunch/bin RENAME beepbotdx)
    install(FILES platform_zero/beepbotdx.desktop DESTINATION share/APPLaunch/applications)
    install(FILES platform_zero/beepbotdx.png DESTINATION share/APPLaunch/share/images)
    install(FILES platform_zero/99-beepbotdx-act-led.rules DESTINATION lib/udev/rules.d)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(CPACK_GENERATOR DEB)
        set(CPACK_PACKAGE_NAME beepbotdx)
        set(CPACK_PACKAGE_VERSION 0.6.0)
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Portable sampler and sequencer for Cardputer Zero")
        set(CPACK_PACKAGE_CONTACT "beepBot DX contributors")
        set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
        set(CPACK_DEBIAN_PACKAGE_SECTION APPLaunch)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/platform_zero/postinst")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        include(CPack)
    endif()
endif()
