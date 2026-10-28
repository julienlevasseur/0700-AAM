# kick-plugin-host: runs a VST3 or LV2 instrument next to the KICK application,
# played from the KICK's MIDI ports.
cmake_minimum_required(VERSION 3.16)
project(kick_plugin_host VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE is shared with Kick (same version and source cache), without Kick's patch.
if(NOT CPM_SOURCE_CACHE)
    set(CPM_SOURCE_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/../.3rdPartySourceCache)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/../Kick/get_cpm.cmake)
CPMAddPackage(
    NAME JUCE_HOST
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 7.0.12
    DOWNLOAD_ONLY True
)
add_subdirectory(${JUCE_HOST_SOURCE_DIR} ./juce)

juce_add_console_app(kick-plugin-host
    PRODUCT_NAME "kick-plugin-host"
    VERSION ${PROJECT_VERSION})

target_sources(kick-plugin-host PRIVATE
    Source/Main.cpp
    Source/HostConfig.cpp
    Source/HostProcessor.cpp
    Source/LiveHost.cpp
    Source/OfflineRun.cpp)

set(HOST_DEFINITIONS
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_AU=0
    JUCE_ALSA=1
    JUCE_JACK=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0)
target_compile_definitions(kick-plugin-host PRIVATE ${HOST_DEFINITIONS})

target_link_libraries(kick-plugin-host PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

install(TARGETS kick-plugin-host DESTINATION bin)

# Unit tests, with a fake plugin: ctest --test-dir build
option(KICK_PLUGIN_HOST_TESTS "Build the unit tests" ON)
if(KICK_PLUGIN_HOST_TESTS)
    enable_testing()
    juce_add_console_app(kick-plugin-host-tests PRODUCT_NAME "kick-plugin-host-tests")
    target_sources(kick-plugin-host-tests PRIVATE
        tests/TestMain.cpp
        tests/HostConfigTests.cpp
        tests/HostProcessorTests.cpp
        Source/HostConfig.cpp
        Source/HostProcessor.cpp)
    target_compile_definitions(kick-plugin-host-tests PRIVATE ${HOST_DEFINITIONS}
        KICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(kick-plugin-host-tests PRIVATE
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME kick-plugin-host-tests COMMAND kick-plugin-host-tests)
endif()
