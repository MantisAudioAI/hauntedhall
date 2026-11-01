# SPDX-License-Identifier: AGPL-3.0-or-later
# Haunted Hall: standalone engine + dev tools (no JUCE, no KICK code).
# Windows: open this folder in Visual Studio 2022 (File > Open > Folder).
cmake_minimum_required(VERSION 3.16)
project(HauntedHall LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(hauntedhall_engine STATIC
  engine/HHCorrosion.cpp
  engine/HHDelay.cpp
  engine/HHDrums.cpp
  engine/HHEngine.cpp
  engine/HHGrid.cpp
  engine/HHPattern.cpp
  engine/HHPresets.cpp
  engine/HHSynth.cpp)
target_include_directories(hauntedhall_engine PUBLIC engine)

if(MSVC)
  target_compile_options(hauntedhall_engine PRIVATE /W4 /fp:fast)
else()
  # KICK builds with -Ofast -ffast-math; the tests pass with these flags.
  target_compile_options(hauntedhall_engine PRIVATE -Wall -Wextra -Wshadow -O3 -ffast-math)
endif()

find_package(Threads REQUIRED)

add_executable(hh_render tools/hh_render.cpp)
target_link_libraries(hh_render PRIVATE hauntedhall_engine)

add_executable(hh_tests tools/hh_tests.cpp)
target_link_libraries(hh_tests PRIVATE hauntedhall_engine Threads::Threads)

# ---------------------------------------------------------------------------
# Standalone test app (the real engine + screens in a desktop window).
# Needs JUCE 7 or newer:  cmake -B build -DHH_JUCE_DIR=C:/path/to/JUCE
set(HH_JUCE_DIR "" CACHE PATH "Path to a JUCE checkout; set it to build the standalone test app")
if(HH_JUCE_DIR)
  add_subdirectory(${HH_JUCE_DIR} juce)

  file(GLOB HH_ASSET_FILES CONFIGURE_DEPENDS
    assets/backgrounds/*.png assets/cartridges/*.png assets/sprites/*.png assets/icons/*.png assets/fonts/*.ttf)
  juce_add_binary_data(hauntedhall_assets SOURCES ${HH_ASSET_FILES})

  juce_add_gui_app(HauntedHallStandalone PRODUCT_NAME "Haunted Hall")
  target_sources(HauntedHallStandalone PRIVATE
    standalone/Main.cpp
    ui/HHUiCommon.cpp ui/HHMasterScreen.cpp ui/HHStepScreen.cpp ui/HHBrowserScreen.cpp)
  target_compile_definitions(HauntedHallStandalone PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(HauntedHallStandalone PRIVATE
    hauntedhall_engine hauntedhall_assets
    juce::juce_gui_basics juce::juce_audio_devices juce::juce_audio_basics
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
