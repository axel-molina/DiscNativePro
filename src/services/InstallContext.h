#pragma once

#include <juce_core/juce_core.h>

// Detecta el contexto de instalación actual para decidir cómo se actualiza
// la app (el updater interno debe desactivarse en Store y .deb).
class InstallContext
{
public:
    enum class Kind
    {
        MacApp,          // macOS: actualiza vía Sparkle
        WinStore,        // MSIX de Microsoft Store: la Store actualiza sola
        WinSetup,        // Setup.exe (Inno Setup): instalación silenciosa propia
        LinuxDeb,        // paquete .deb: apt gestiona las actualizaciones
        LinuxPortable    // tar.gz portable: auto-reemplazo propio
    };

    static Kind detect();

    // Etiqueta estable para settings.json / logs.
    static juce::String key(Kind kind);
};
