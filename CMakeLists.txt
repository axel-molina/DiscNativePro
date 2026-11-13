cmake_minimum_required(VERSION 3.22)

# Piso de macOS: el dylib vendorizado de ONNX Runtime en uso (libonnxruntime.1.dylib
# → wheel 1.27.0, minos 14.0) marca el mínimo; si se vuelve a 1.22.0 (minos 13.3),
# este piso puede bajar. Sin esto el target hereda el macOS de la máquina que
# compila (CI macos-latest) y el binario queda "requires macOS 26.0".
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
endif()

project(DiscNativePro VERSION 1.4.0 LANGUAGES C CXX)

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
        set(APP_VERSION "${PROJECT_VERSION}")
    else()
        string(REGEX REPLACE "^v" "" APP_VERSION "${GIT_DESCRIBE}")
    endif()
endif()

string(REGEX MATCH "^[0-9]+(\\.[0-9]+){0,2}" APP_VERSION_NORMALIZED "${APP_VERSION}")
if(APP_VERSION_NORMALIZED STREQUAL "")
    set(APP_VERSION_NORMALIZED "${PROJECT_VERSION}")
endif()
set(APP_VERSION "${APP_VERSION_NORMALIZED}")
message(STATUS "DiscNativePro APP_VERSION: ${APP_VERSION}")

# Versión numérica para metadatos del bundle (CFBundleShortVersionString /
# VS_VERSION_INFO). git describe puede producir "1.3.0-1-g4dda9cb", que no es
# válido para el bundle, así que se recorta al prefijo X.Y.Z.
set(APP_BUNDLE_VERSION "${APP_VERSION}")
message(STATUS "DiscNativePro bundle version: ${APP_BUNDLE_VERSION}")

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

# LAME MP3 encoder (vendorizado en third_party/lame) para el broadcast Icecast
add_subdirectory(third_party/lame)

juce_add_gui_app(DiscNativePro
    PRODUCT_NAME "DiscNativePro"
    COMPANY_NAME "DiscPro"
    BUNDLE_ID "com.discpro.discnativepro"
    VERSION "${APP_BUNDLE_VERSION}"
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
        src/audio/BroadcastService.h
        src/audio/BroadcastService.cpp
        src/audio/MicChannel.h
        src/audio/MicChannel.cpp
        src/audio/MasterLimiter.h
        src/audio/MasterLimiter.cpp
        src/audio/Mp3Exporter.h
        src/audio/Mp3Exporter.cpp
        src/services/FfmpegLocator.h
        src/services/FfmpegLocator.cpp
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
        src/services/MixxxPresetCatalog.h
        src/services/MixxxPresetCatalog.cpp
        src/services/MixxxMappingImporter.h
        src/services/MixxxMappingImporter.cpp
        src/services/VdjMappingImporter.h
        src/services/VdjMappingImporter.cpp
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
        src/services/PlaylistStore.h
        src/services/PlaylistStore.cpp
        src/services/StemSeparationService.h
        src/services/StemSeparationService.cpp
        src/services/OnnxStemEngine.cpp
        src/services/TwoStemEngine.h
        src/services/TwoStemEngine.cpp
        third_party/kissfft/kiss_fft.c
        third_party/kissfft/kiss_fftr.c
        src/services/AsyncFolderScanner.h
        src/services/AsyncFolderScanner.cpp
        src/services/SoundCloudService.h
        src/services/SoundCloudService.cpp
        src/ui/TopBarComponent.h
        src/ui/TopBarComponent.cpp
        src/ui/BpmFormat.h
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
        src/ui/GridEditorComponent.h
        src/ui/GridEditorComponent.cpp
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
        third_party/kissfft
)

# P26: kissfft es vendorizado (BSD) y compila dentro del target principal;
# sus warnings de conversión propios se silencian sin relajar el nivel
# estricto del resto del proyecto (mismo criterio que StretchAdapter).
set_source_files_properties(
    third_party/kissfft/kiss_fft.c
    third_party/kissfft/kiss_fftr.c
    PROPERTIES COMPILE_OPTIONS "-w"
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
        mp3lame
)

# -----------------------------------------------------------------------------
# P23: ONNX Runtime (motor ML de stems, vendorizado en third_party/onnxruntime).
# La runtime se enlaza y el dylib/dll/so se copia al bundle/junto al exe en el
# post-build. El modelo (assets/models/htdemucs_fp32.onnx, gitignored) se copia
# a Resources / junto al exe SOLO si existe; sin modelo la app degrada con
# gracia (sin separación de stems).
# -----------------------------------------------------------------------------
set(ORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
set(STEMS_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/htdemucs_fp32.onnx")

if(APPLE)
    target_include_directories(DiscNativePro PRIVATE "${ORT_ROOT}/macos/include")
    target_link_libraries(DiscNativePro PRIVATE "${ORT_ROOT}/macos/lib/libonnxruntime.dylib")
    set_target_properties(DiscNativePro PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    add_custom_command(TARGET DiscNativePro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_ROOT}/macos/lib/libonnxruntime-wheel-1.27.0.dylib"
            "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Frameworks/"
        COMMENT "Bundling ONNX Runtime into app Frameworks"
    )
    if (EXISTS "${STEMS_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS_MODEL}"
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/models/"
            COMMENT "Bundling htdemucs model into app resources"
        )
    endif()
elseif(WIN32)
    target_include_directories(DiscNativePro PRIVATE "${ORT_ROOT}/windows/include")
    target_link_libraries(DiscNativePro PRIVATE "${ORT_ROOT}/windows/lib/onnxruntime.lib")
    add_custom_command(TARGET DiscNativePro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_ROOT}/windows/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:DiscNativePro>/onnxruntime.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_ROOT}/windows/lib/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:DiscNativePro>/onnxruntime_providers_shared.dll"
        COMMENT "Bundling ONNX Runtime next to executable"
    )
    if (EXISTS "${STEMS_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS_MODEL}"
                "$<TARGET_FILE_DIR:DiscNativePro>/models/htdemucs_fp32.onnx"
            COMMENT "Bundling htdemucs model next to executable"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    target_include_directories(DiscNativePro PRIVATE "${ORT_ROOT}/linux/include")
    target_link_libraries(DiscNativePro PRIVATE "${ORT_ROOT}/linux/lib/libonnxruntime.so")
    set_target_properties(DiscNativePro PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
    add_custom_command(TARGET DiscNativePro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_ROOT}/linux/lib/libonnxruntime.so.1.31.1"
            "$<TARGET_FILE_DIR:DiscNativePro>/"
        COMMENT "Bundling ONNX Runtime next to executable"
    )
    if (EXISTS "${STEMS_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:DiscNativePro>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS_MODEL}"
                "$<TARGET_FILE_DIR:DiscNativePro>/models/"
            COMMENT "Bundling htdemucs model next to executable"
        )
    endif()
endif()

# P25: modelo 2-stem (uvr_mdxnet_voc_ft_fp16.onnx, gitignored) junto al de
# 4 stems; sin él la app degrada a la separación completa.
set(STEMS2_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/uvr_mdxnet_voc_ft_fp16.onnx")
if(APPLE)
    if (EXISTS "${STEMS2_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS2_MODEL}"
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/models/"
            COMMENT "Bundling 2-stem model into app resources"
        )
    endif()
elseif(WIN32)
    if (EXISTS "${STEMS2_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:DiscNativePro>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS2_MODEL}"
                "$<TARGET_FILE_DIR:DiscNativePro>/models/"
            COMMENT "Bundling 2-stem model next to executable"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    if (EXISTS "${STEMS2_MODEL}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:DiscNativePro>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STEMS2_MODEL}"
                "$<TARGET_FILE_DIR:DiscNativePro>/models/"
            COMMENT "Bundling 2-stem model next to executable"
        )
    endif()
endif()

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

    # ffmpeg (transporte RTMP del broadcast). Igual que yt-dlp: se copia al bundle
    # si existe localmente; si no, la app lo descarga en runtime (FfmpegLocator).
    set(FFMPEG_CANDIDATE "$ENV{HOME}/Library/Application Support/DiscNativePro/bin/ffmpeg")
    if (EXISTS "${FFMPEG_CANDIDATE}")
        add_custom_command(TARGET DiscNativePro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_CANDIDATE}"
                "$<TARGET_BUNDLE_DIR:DiscNativePro>/Contents/Resources/ffmpeg"
            COMMENT "Bundling ffmpeg into app resources"
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
            # Fuerza el rasterizador GDI software (comportamiento JUCE 7) en vez
            # de Direct2D, que deja la ventana sin pintar en equipos con GPU
            # híbrida Intel+AMD. Reversible cuando JUCE estabilice D2D.
            JUCE_DIRECT2D=0
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

# -----------------------------------------------------------------------------
# P27: empaquetado Linux (.deb) — binario, runtime ONNX, .desktop e icono.
# Los modelos ONNX siguen gitignored y NO van al paquete (stems degradan con
# aviso, igual que el tar.gz). El .so de ONNX va a /usr/lib/DiscNativePro y el
# INSTALL_RPATH lo incluye.
# -----------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    set_target_properties(DiscNativePro PROPERTIES
        INSTALL_RPATH "$ORIGIN:/usr/lib/DiscNativePro"
    )
    install(TARGETS DiscNativePro RUNTIME DESTINATION bin)
    if(EXISTS "${ORT_ROOT}/linux/lib/libonnxruntime.so.1.31.1")
        install(FILES "${ORT_ROOT}/linux/lib/libonnxruntime.so.1.31.1"
                RENAME "libonnxruntime.so.1"
                DESTINATION lib/DiscNativePro)
    endif()
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
            RENAME "discnativepro.png"
            DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/install/linux/discnativepro.desktop"
            DESTINATION share/applications)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "discnativepro")
    set(CPACK_PACKAGE_VENDOR "DiscNativePro")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Consola DJ nativa: doble deck, stems, FX sincronizados y MIDI")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Axel Molina <ivanmolina052@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtk-3-0, libwebkit2gtk-4.1-0 | libwebkit2gtk-4.0-37, libasound2, libjack-jackd2-0 | libjack0, libfreetype6, libcurl4, libx11-6, libxcomposite1, libxcursor1, libxext6, libxinerama1, libxrandr2, libxrender1, libglu1-mesa, libgl1")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    include(CPack)
endif()
