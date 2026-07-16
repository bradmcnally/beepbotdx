cmake_minimum_required(VERSION 3.16)
project(beepbotdx_desktop)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(
    src
    src/core
    src/views
    ${SDL2_INCLUDE_DIRS}
)

add_definitions(-DDESKTOP_BUILD)

if(SCREEN_W)
    add_definitions(-DSCREEN_WIDTH=${SCREEN_W})
endif()
if(SCREEN_H)
    add_definitions(-DSCREEN_HEIGHT=${SCREEN_H})
endif()

set(CORE_SOURCES
    src/core/app.cpp
    src/core/bloom_field.cpp
    src/core/canvas.cpp
    src/core/character.cpp
    src/core/pattern_gen.cpp
    src/core/project.cpp
    src/core/sequencer.cpp
    src/core/sound_slot.cpp
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
