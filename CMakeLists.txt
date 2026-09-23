cmake_minimum_required(VERSION 3.22)

# Must be set before project(): afterwards CMake ignores it and builds for the SDK's own macOS version.
# 10.13 High Sierra covers older Intel Macs; the arm64 slice is always 11.0+.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Oldest macOS supported")

project(Hypernova VERSION 0.12.0)

set(CMAKE_CXX_STANDARD 17)
option(HYPERNOVA_COPY_PLUGINS "Install plug-ins into ~/Library/Audio/Plug-Ins after building" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Hypernova
    COMPANY_NAME "Arrow"
    PLUGIN_MANUFACTURER_CODE Arrw
    PLUGIN_CODE ArBs
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Hypernova"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/art/AppIcon.png"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${HYPERNOVA_COPY_PLUGINS})

set(HYPERNOVA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EditorLayout.cpp)

target_sources(Hypernova PRIVATE ${HYPERNOVA_SOURCES})

# Bundled display fonts (all SIL Open Font License).
file(GLOB HYPERNOVA_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.ttf")
juce_add_binary_data(HypernovaFonts SOURCES ${HYPERNOVA_FONTS})
set_target_properties(HypernovaFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(Hypernova PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    HYPERNOVA_VERSION="${PROJECT_VERSION}")

target_link_libraries(Hypernova
    PRIVATE
        HypernovaFonts
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test: renders every factory preset, checks for NaNs/levels/CPU and writes demo WAVs.
# cmake --build build --target SmokeTest && ./build/SmokeTest_artefacts/Release/SmokeTest dist/demos
juce_add_console_app(SmokeTest PRODUCT_NAME "SmokeTest")
target_sources(SmokeTest PRIVATE Tests/SmokeTest.cpp ${HYPERNOVA_SOURCES})
target_compile_definitions(SmokeTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Hypernova" JUCE_MODAL_LOOPS_PERMITTED=1 HYPERNOVA_VERSION="${PROJECT_VERSION}")
target_link_libraries(SmokeTest PRIVATE HypernovaFonts juce::juce_audio_utils juce::juce_dsp juce::juce_opengl PUBLIC juce::juce_recommended_config_flags)

# Renders the editor to PNG for visual checks: UISnapshot <outDir>
juce_add_console_app(UISnapshot PRODUCT_NAME "UISnapshot")
target_sources(UISnapshot PRIVATE Tests/UISnapshot.cpp ${HYPERNOVA_SOURCES})
target_compile_definitions(UISnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Hypernova" HYPERNOVA_VERSION="${PROJECT_VERSION}")
target_link_libraries(UISnapshot PRIVATE HypernovaFonts juce::juce_audio_utils juce::juce_dsp juce::juce_opengl PUBLIC juce::juce_recommended_config_flags)

# Writes the FOUNDERS PACK (.hnpreset files) that ships in the DMG and installer: MakeFoundersPack <outDir>
juce_add_console_app(MakeFoundersPack PRODUCT_NAME "MakeFoundersPack")
target_sources(MakeFoundersPack PRIVATE Tests/MakeFoundersPack.cpp ${HYPERNOVA_SOURCES})
target_compile_definitions(MakeFoundersPack PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Hypernova" HYPERNOVA_VERSION="${PROJECT_VERSION}")
target_link_libraries(MakeFoundersPack PRIVATE HypernovaFonts juce::juce_audio_utils juce::juce_dsp juce::juce_opengl PUBLIC juce::juce_recommended_config_flags)

# Renders the installer/DMG artwork in the plug-in's own style: MakeInstallerArt <repoRoot>
juce_add_console_app(MakeInstallerArt PRODUCT_NAME "MakeInstallerArt")
target_sources(MakeInstallerArt PRIVATE Tests/MakeInstallerArt.cpp)
target_compile_definitions(MakeInstallerArt PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MakeInstallerArt PRIVATE HypernovaFonts juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags)
