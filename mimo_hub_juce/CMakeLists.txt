cmake_minimum_required(VERSION 3.22)

project(MiMOHub VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Scaricamento automatico JUCE 7.0.12 (Tag stabile e leggero)
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# Definizione Applicazione Grafica Standalone
juce_add_gui_app(MiMOHub
    PRODUCT_NAME "MiMO Hub"
    VERSION 1.0.0
    BUNDLE_ID org.mimo.hub
)

juce_add_binary_data(MiMOBinaryData
    SOURCES
        Resources/mimo_device.png
)

target_sources(MiMOHub PRIVATE
    Source/Main.cpp
    Source/Audio/AudioDSP.h
    Source/Audio/AudioEngine.h
    Source/Audio/DSPUtils.h
    Source/Audio/NodeAudioProcessor.h
    Source/Audio/Modules/Instruments/AnalogDrumVoice.h
    Source/Audio/Modules/Instruments/MelodicSynthVoice.h
    Source/Audio/Modules/Effects/SVFFilter.h
    Source/Audio/Modules/Effects/StereoTapeDelay.h
    Source/Audio/Modules/Effects/LushReverb.h
    Source/GUI/MiMOLookAndFeel.h
    Source/GUI/MiMOLookAndFeel.cpp
    Source/GUI/MainComponent.h
    Source/GUI/MainComponent.cpp
    Source/Utils/qrcodegen.hpp
    Source/Utils/qrcodegen.cpp
    Source/Networking/MiMOEyeServer.h
    Source/Networking/MiMOEyeServer.cpp
)

target_include_directories(MiMOHub PRIVATE
    Source
)

# I sorgenti contengono simboli Unicode (✓, ✕, ●, ...): compila come UTF-8 su MSVC
target_compile_options(MiMOHub PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(MiMOHub PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=1
    JUCE_SILENCE_GLOBAL_VARS_WARNING=1
)

target_link_libraries(MiMOHub PRIVATE
    MiMOBinaryData
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_osc
    juce::juce_video
)



