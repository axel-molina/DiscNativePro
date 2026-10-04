cmake_minimum_required(VERSION 3.22)

project(DiscNativePro VERSION 1.0.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch JUCE 8
message(STATUS "Fetching JUCE 8 framework...")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(DiscNativePro
    PRODUCT_NAME "DiscNativePro"
    COMPANY_NAME "DiscPro"
    BUNDLE_ID "com.discpro.discnativepro"
)

target_sources(DiscNativePro
    PRIVATE
        src/Main.cpp
        src/audio/DeckPlayer.h
        src/audio/DeckPlayer.cpp
        src/audio/DjMixer.h
        src/audio/DjMixer.cpp
        src/audio/BeatDetector.h
        src/audio/BeatDetector.cpp
        src/audio/AudioEngine.h
        src/audio/AudioEngine.cpp
        src/audio/AudioRecorder.h
        src/audio/AudioRecorder.cpp
        src/audio/SampleTrackGenerator.h
        src/audio/SampleTrackGenerator.cpp
        src/midi/MidiManager.h
        src/midi/MidiManager.cpp
        src/services/YouTubeService.h
        src/services/YouTubeService.cpp
        src/services/ThumbnailCache.h
        src/services/ThumbnailCache.cpp
        src/ui/TopBarComponent.h
        src/ui/TopBarComponent.cpp
        src/ui/DjKnob.h
        src/ui/VuMeter.h
        src/ui/JogWheel.h
        src/ui/JogWheel.cpp
        src/ui/OverviewWaveform.h
        src/ui/OverviewWaveform.cpp
        src/ui/VerticalWaveform.h
        src/ui/VerticalWaveform.cpp
        src/ui/ChannelStripComponent.h
        src/ui/ChannelStripComponent.cpp
        src/ui/CenterColumnComponent.h
        src/ui/CenterColumnComponent.cpp
        src/ui/ScrollingWaveform.h
        src/ui/ScrollingWaveform.cpp
        src/ui/DeckComponent.h
        src/ui/DeckComponent.cpp
        src/ui/YouTubeVideoComponent.h
        src/ui/YouTubeVideoComponent.mm
        src/ui/MixerComponent.h
        src/ui/MixerComponent.cpp
        src/ui/LibraryComponent.h
        src/ui/LibraryComponent.cpp
        src/ui/YouTubeLibraryView.h
        src/ui/YouTubeLibraryView.cpp
        src/ui/FolderTreeComponent.h
        src/ui/FolderTreeComponent.cpp
        src/ui/MidiModalComponent.h
        src/ui/MidiModalComponent.cpp
        src/ui/SettingsModalComponent.h
        src/ui/SettingsModalComponent.cpp
        src/ui/FxPanelComponent.h
        src/ui/FxPanelComponent.cpp
        src/ui/MainComponent.h
        src/ui/MainComponent.cpp
        src/ui/CoverArtGenerator.h
        src/ui/CoverArtGenerator.cpp
        src/ui/DjLookAndFeel.h
        src/ui/DjLookAndFeel.cpp
        src/ui/LucideIcons.h
        src/ui/LucideIcons.cpp
        src/ui/DjButton.h
)

target_compile_definitions(DiscNativePro
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_include_directories(DiscNativePro
    PRIVATE
        src
)

target_link_libraries(DiscNativePro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        "-framework WebKit"
        "-framework Cocoa"
)
