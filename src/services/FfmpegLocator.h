#pragma once

#include <juce_core/juce_core.h>

// Localización e instalación del binario ffmpeg (para el transporte RTMP).
// Mismo patrón que el localizador de yt-dlp en YouTubeAudioDownloader.
namespace FfmpegLocator
{
// App Support/bin -> Bundle Resources -> Homebrew (arm/intel) -> PATH
juce::File locateFfmpeg();

// Descarga el binario estatico oficial a App Support/bin. Bloqueante
// (llamar desde un hilo de trabajo). Devuelve true si quedo instalado y usable.
bool ensureInstalled();

// Ruta destino de la instalacion (para mensajes de la UI)
juce::File getInstallTargetFile();
}
