cmake_minimum_required(VERSION 3.22)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
project(Tantri VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(TANTRI_COPY_AFTER_BUILD "Install into the user plug-in folders after building" OFF)
option(TANTRI_BUILD_VST2 "Also build the VST2-compatible plug-in (FST headers)" ON)

add_subdirectory(JUCE)

if(APPLE)
    set(TT_FORMATS AU VST3 Standalone)
else()
    set(TT_FORMATS VST3 Standalone)
endif()
if(TANTRI_BUILD_VST2)
    juce_set_vst2_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst2-fst")
    list(APPEND TT_FORMATS VST)
endif()
set(TT_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0 _USE_MATH_DEFINES=1 NOMINMAX=1)

# Three products from one source:
#   Tantri            the stereo instrument
#   Tantri Akasha     the same instrument placed in space: Ambisonics out (openfield's encoder + motions)
#   Tantri Akasha FX  the spatial build as an effect: its input becomes a fourth source in the space
#                     and sounds the sitar's sympathetic strings; still plays MIDI (and the drone)
function(tt_add target code name desc spatial fx icon)
    set(formats ${TT_FORMATS})
    if(fx)
        list(REMOVE_ITEM formats Standalone)
        set(synth FALSE)
        set(au_type kAudioUnitType_MusicEffect)
        set(vst3_cats Fx Spatial)
        set(vst2_cat kPlugCategSpacializer)
    else()
        set(synth TRUE)
        set(au_type kAudioUnitType_MusicDevice)
        set(vst3_cats Instrument Synth)
        set(vst2_cat kPlugCategSynth)
    endif()
    string(TOLOWER "${target}" bid)
    juce_add_plugin(${target}
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Tantri"
        BUNDLE_ID "com.tantri.${bid}"
        PLUGIN_MANUFACTURER_CODE Frkp
        PLUGIN_CODE ${code}
        FORMATS ${formats}
        PRODUCT_NAME "${name}"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/${icon}"
        DESCRIPTION "${desc}"
        IS_SYNTH ${synth}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_MAIN_TYPE ${au_type}
        VST3_CATEGORIES ${vst3_cats}
        VST2_CATEGORY ${vst2_cat}
        COPY_PLUGIN_AFTER_BUILD ${TANTRI_COPY_AFTER_BUILD})
    target_sources(${target} PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(${target} PUBLIC ${TT_DEFS} TANTRI_SPATIAL=${spatial} TANTRI_FX=${fx})
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

    if(TANTRI_BUILD_TESTS)
        juce_add_console_app(${target}Test PRODUCT_NAME "${target}Test")
        target_sources(${target}Test PRIVATE tests/Test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
        if(fx)
            set(t_synth 0)
        else()
            set(t_synth 1)
        endif()
        target_compile_definitions(${target}Test PRIVATE ${TT_DEFS} TANTRI_SPATIAL=${spatial} TANTRI_FX=${fx}
            JucePlugin_Name="${name}" JucePlugin_Build_Standalone=1
            JucePlugin_IsSynth=${t_synth} JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0
            JucePlugin_ManufacturerCode=0x46726b70 JucePlugin_PluginCode=0x546e7472 JucePlugin_VersionString="${PROJECT_VERSION}"
            JucePlugin_Manufacturer="Tantri" JucePlugin_Desc="${name}" JucePlugin_VersionCode=0x10100)
        target_link_libraries(${target}Test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
        if(MSVC)
            target_link_options(${target}Test PRIVATE /STACK:8388608)
        endif()
    endif()
endfunction()

option(TANTRI_BUILD_TESTS "Build the offline test runners" ON)
tt_add(Tantri Tntr "Tantri" "Physically modelled sitar, tabla and tanpura in just shruti tuning" 0 0 icon_1024.png)
tt_add(TantriAkasha TnAk "Tantri Akasha" "Tantri in space: sitar, tabla and tanpura as moving Ambisonic sources" 1 0 akasha_1024.png)
tt_add(TantriAkashaFX TnAx "Tantri Akasha FX" "Tantri Akasha as an effect: the input moves through the space with the instruments" 1 1 akasha_1024.png)
