cmake_minimum_required(VERSION 3.22)

project(DiscNativePro VERSION 1.3.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# App version: CI passes -DAPP_VERSION from release tag (v1.2.0 -> 1.2.0).
# Local builds derive it from git describe (v1.0.0-3-gabc123 -> 1.0.0-3-gabc123),
# so the updater compares truthful versions instead of a generic -dev marker.
if(NOT DEFINED APP_VERSION OR "${APP_VERSION}" STREQUAL "")
    execute_process(COMMAND git describe --tags --dirty --always
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE GIT_DESCRIBE
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(GIT_DESCRIBE STREQUAL "")
        set(APP_VERSION "1.0.0-dev")
    else()
        string(REGEX REPLACE "^v" "" APP_VERSION "${GIT_DESCRIBE}")
    endif()
endif()
message(STATUS "DiscNativePro APP_VERSION: ${APP_VERSION}")

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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
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
        src/audio/DynamicBeatgrid.h
        src/audio/KeyDetector.h
        src/audio/KeyDetector.cpp
        src/audio/DeckFxChain.h
        src/audio/DeckFxChain.cpp
        src/audio/StretchAdapter.h
        src/audio/StretchAdapter.cpp
        src/audio/SamplerBank.h
        src/audio/SamplerBank.cpp
        src/audio/AudioEngine.h
        src/audio/AudioEngine.cpp
        src/audio/AudioRecorder.h
        src/audio/AudioRecorder.cpp
        src/audio/SampleTrackGenerator.h
        src/audio/SampleTrackGenerator.cpp
        src/midi/MidiManager.h
        src/midi/MidiManager.cpp
        src/midi/MidiMapping.h
        src/midi/MidiMapping.cpp
        src/services/YouTubeService.h
        src/services/YouTubeService.cpp
        src/services/YouTubeAudioDownloader.h
        src/services/YouTubeAudioDownloader.cpp
        src/services/ThumbnailCache.h
        src/services/ThumbnailCache.cpp
        src/services/AIService.h
        src/services/AIService.cpp
        src/services/SecretStore.h
        src/services/SecretStore.cpp
        src/services/LanguageManager.h
        src/services/LanguageManager.cpp
        src/services/WaveformTheme.h
        src/services/WaveformTheme.cpp
        src/services/UpdateService.h
        src/services/UpdateService.cpp
        src/services/UsbDriveScanner.h
        src/services/UsbDriveScanner.cpp
        src/services/LibraryCacheManager.h
        src/services/LibraryCacheManager.cpp
        src/services/AsyncFolderScanner.h
        src/services/AsyncFolderScanner.cpp
        src/services/SoundCloudService.h
        src/services/SoundCloudService.cpp
        src/ui/TopBarComponent.h
        src/ui/TopBarComponent.cpp
        src/ui/DjKnob.h
        src/ui/VuMeter.h
        src/ui/JogWheel.h
        src/ui/JogWheel.cpp
        src/ui/OverviewWaveform.h
        src/ui/OverviewWaveform.cpp
        src/ui/WavesViewComponent.h
        src/ui/WavesViewComponent.cpp
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
        src/ui/YouTubeEmbedHtml.h
        src/ui/YouTubeEmbedHtml.cpp
        src/ui/MixerComponent.h
        src/ui/MixerComponent.cpp
        src/ui/LibraryComponent.h
        src/ui/LibraryComponent.cpp
        src/ui/YouTubeLibraryView.h
        src/ui/YouTubeLibraryView.cpp
        src/ui/UsbLibraryView.h
        src/ui/UsbLibraryView.cpp
        src/ui/AIMatchView.h
        src/ui/AIMatchView.cpp
        src/ui/SoundCloudLibraryView.h
        src/ui/SoundCloudLibraryView.cpp
        src/ui/FolderTreeComponent.h
        src/ui/FolderTreeComponent.cpp
        src/ui/MidiModalComponent.h
        src/ui/MidiModalComponent.cpp
        src/ui/MidiMappingTableComponent.h
        src/ui/MidiMappingTableComponent.cpp
        src/ui/SettingsModalComponent.h
        src/ui/SettingsModalComponent.cpp
        src/ui/FxSlotComponent.h
        src/ui/FxSlotComponent.cpp
        src/ui/FxPanelComponent.h
        src/ui/FxPanelComponent.cpp
        src/ui/SamplerPanelComponent.h
        src/ui/SamplerPanelComponent.cpp
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

target_include_directories(DiscNativePro
    PRIVATE
        src
)

target_compile_definitions(DiscNativePro
    PRIVATE
        DISCNATIVEPRO_VERSION="${APP_VERSION}"
        JUCE_USE_MP3AUDIOFORMAT=1
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
)

if(APPLE)
    target_sources(DiscNativePro
        PRIVATE
            src/ui/YouTubeVideoComponent.mm
    )
    target_compile_definitions(DiscNativePro
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )
    target_link_libraries(DiscNativePro
        PRIVATE
            "-framework WebKit"
            "-framework Cocoa"
            "-framework Security"
    )

    set(YT_DLP_CANDIDATE "$ENV{HOME}/Library/Application Support/DiscNativePro/bin/yt-dlp")
    if (EXISTS "${YT_DLP_CANDIDATE}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${YT_DLP_CANDIDATE}"
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/yt-dlp"
            COMMENT "Bundling yt-dlp into app resources"
        )
    endif()
elseif(WIN32)
    # Descargar automáticamente el SDK oficial de Microsoft.Web.WebView2
    include(FetchContent)
    FetchContent_Declare(
        microsoft_webview2
        URL "https://globalcdn.nuget.org/packages/microsoft.web.webview2.1.0.1901.177.nupkg"
        DOWNLOAD_NAME "microsoft.web.webview2.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(microsoft_webview2)

    target_sources(DiscNativePro
        PRIVATE
            src/ui/YouTubeVideoComponent_win.cpp
    )
    target_compile_definitions(DiscNativePro
        PRIVATE
            JUCE_WEB_BROWSER=1
            JUCE_USE_WIN_WEBVIEW2=1
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
            JUCE_USE_CURL=0
    )

    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
        set(WV2_ARCH "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(WV2_ARCH "x86")
    else()
        set(WV2_ARCH "x64")
    endif()

    target_include_directories(DiscNativePro
        PRIVATE
            "${microsoft_webview2_SOURCE_DIR}/build/native/include"
    )

    target_link_libraries(DiscNativePro
        PRIVATE
            psapi
            mfplat
            mfreadwrite
            mfuuid
            advapi32
            "${microsoft_webview2_SOURCE_DIR}/build/native/${WV2_ARCH}/WebView2LoaderStatic.lib"
    )

    add_custom_command(TARGET DiscNativePro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${microsoft_webview2_SOURCE_DIR}/runtimes/win-${WV2_ARCH}/native/WebView2Loader.dll"
            "$<TARGET_FILE_DIR:DiscNativePro>/WebView2Loader.dll"
    )
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    pkg_search_module(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.1 webkit2gtk-4.0)

    target_sources(DiscNativePro
        PRIVATE
            src/ui/YouTubeVideoComponent_linux.cpp
    )
    target_compile_definitions(DiscNativePro
        PRIVATE
            JUCE_WEB_BROWSER=1
            JUCE_USE_CURL=0
    )
    target_link_libraries(DiscNativePro
        PRIVATE
            PkgConfig::GTK3
            PkgConfig::WEBKIT
    )
endif()
