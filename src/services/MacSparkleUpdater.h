#pragma once

#include <juce_core/juce_core.h>

// Puente C++ -> Sparkle 2 (macOS). El controller estándar de Sparkle maneja
// la UI completa: detección, alerta con "Install on Quit" e instalación al
// reiniciar. La app solo necesita inicializar y pedir chequeos.
#if JUCE_MAC
class SparkleBridge
{
public:
    // Idempotente: inicializa el updater con feed delegado y arranca el
    // chequeo automático programado de Sparkle.
    static void startup();

    // Chequeo pedido por el usuario (abre la UI estándar de Sparkle).
    static void checkForUpdates();

    static void shutdown();
};
#endif
