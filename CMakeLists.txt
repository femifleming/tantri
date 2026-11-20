cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
project(Tantri VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(TANTRI_COPY_AFTER_BUILD "Install into the user plug-in folders after building" OFF)
option(TANTRI_BUILD_VST2 "Also build the VST2-compatible plug-in (FST headers)" ON)

add_subdirectory(JUCE)

set(TT_FORMATS AU VST3 Standalone)
if(TANTRI_BUILD_VST2)
    juce_set_vst2_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst2-fst")
    list(APPEND TT_FORMATS VST)
endif()

juce_add_plugin(Tantri
    VERSION 1.0.0
    COMPANY_NAME "FreakiePie"
    BUNDLE_ID "com.freakiepie.tantri"
    PLUGIN_MANUFACTURER_CODE Frkp
    PLUGIN_CODE Tntr
    FORMATS ${TT_FORMATS}
    PRODUCT_NAME "Tantri"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon_1024.png"
    DESCRIPTION "Physically modelled sitar, tabla and tanpura in just shruti tuning"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    VST2_CATEGORY kPlugCategSynth
    COPY_PLUGIN_AFTER_BUILD ${TANTRI_COPY_AFTER_BUILD})

target_sources(Tantri PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
set(TT_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_MODAL_LOOPS_PERMITTED=0 _USE_MATH_DEFINES=1)
target_compile_definitions(Tantri PUBLIC ${TT_DEFS})
target_link_libraries(Tantri
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

juce_add_console_app(TantriTest PRODUCT_NAME "TantriTest")
target_sources(TantriTest PRIVATE tests/Test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(TantriTest PRIVATE ${TT_DEFS} JucePlugin_Name="Tantri" JucePlugin_Build_Standalone=1
    JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0
    JucePlugin_ManufacturerCode=0x46726b70 JucePlugin_PluginCode=0x546e7472 JucePlugin_VersionString="1.0.0"
    JucePlugin_Manufacturer="FreakiePie" JucePlugin_Desc="Tantri" JucePlugin_VersionCode=0x10000)
target_link_libraries(TantriTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
