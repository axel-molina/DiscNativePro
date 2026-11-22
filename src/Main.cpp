#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"
#include "ui/AppFonts.h"
#include "ui/AppLookAndFeel.h"
#include "services/StemSeparationService.h"
#include "services/UpdateService.h"

#if JUCE_MAC
    #include "services/MacSparkleUpdater.h"
#endif

#if JUCE_LINUX
    #include <cstdlib>
#endif

// Fuente única de versión: CMake inyecta DISCNATIVEPRO_VERSION desde APP_VERSION
// en target_compile_definitions. Si falta, falla explícitamente en compilación en
// vez de mostrar una versión hardcodeada falsa.
#ifndef DISCNATIVEPRO_VERSION
    #error "DISCNATIVEPRO_VERSION no definido: CMake debe inyectarlo desde APP_VERSION (target_compile_definitions)."
#endif

#if JUCE_LINUX
namespace
{
    // WebKitGTK 2.42+ usa el renderer DMA-BUF y el compositing acelerado por
    // defecto. En NVIDIA propietario, XWayland y máquinas virtuales eso produce
    // la firma clásica "el audio suena pero el vídeo es un rectángulo blanco":
    // el pipeline de media va por su propio sink y sigue sonando mientras la
    // superficie web no llega a pintarse. Estos dos switches son el workaround
    // documentado por WebKitGTK y lo dejan en render por software.
    //
    // overwrite=0: si el usuario ya los definió en su entorno (p. ej.
    // WEBKIT_DISABLE_COMPOSITING_MODE=0 en una GPU que sí funciona) se respeta.
    //
    // ponytail: coste = decodificar vídeo en CPU en vez de GPU. Para un iframe
    // de YouTube es despreciable frente a una ventana en blanco. Si algún día se
    // quiere GPU, la ruta es exponerlo como preferencia y no tocarlo aquí.
    void configureWebKitForLinux()
    {
        ::setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 0);
        ::setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 0);
    }
}
#endif

namespace
{
// P27-bis: el layout pide ~1800 px logicos (2 decks + mixer de 508 + margenes).
// Si el area util es menor (pantallas pequenas o escalado de Windows alto), se
// reduce TODA la UI de forma uniforme con la escala global de JUCE: asi no se
// recorta la bandeja 2 ni los hot cues. En pantallas grandes no hace nada.
void applyCompactUiScaleIfNeeded()
{
    constexpr float designWidth  = 1800.0f;
    constexpr float designHeight = 860.0f;

    if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = disp->userArea.toFloat();
        const float scale = juce::jmin (1.0f,
                                        area.getWidth()  / designWidth,
                                        area.getHeight() / designHeight);

        if (scale < 0.99f)
            juce::Desktop::getInstance().setGlobalScaleFactor (juce::jmax (0.6f, scale));
    }
}
} // namespace

class DiscNativeProApplication : public juce::JUCEApplication
{
public:
    DiscNativeProApplication() {}

    const juce::String getApplicationName() override       { return "DiscNativePro"; }
    const juce::String getApplicationVersion() override    { return DISCNATIVEPRO_VERSION; }
    // Instancia única: un segundo lanzamiento reactiva la ventana existente
    // (ver anotherInstanceStarted) en vez de dejar procesos huérfanos.
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise(const juce::String& commandLineParameters) override
    {
       #if JUCE_LINUX
        // Antes de crear ninguna ventana ni webview: WebKit lee su entorno al
        // arrancar el proceso web (fork del nuestro), así que tiene que estar
        // puesto aquí y no dentro del componente.
        configureWebKitForLinux();
       #endif

        // P37: las caras embebidas deben existir antes del primer Font, y el
        // LookAndFeel global antes de crear cualquier componente.
        AppFonts::initialise();
        juce::LookAndFeel::setDefaultLookAndFeel (&appLookAndFeel);

        // Antes de crear la ventana: si el layout no cabe, escalar la UI.
       #if ! (JUCE_IOS || JUCE_ANDROID)
        applyCompactUiScaleIfNeeded();
       #endif

        mainWindow.reset(new MainWindow(getApplicationName()));

       #if JUCE_MAC
        // P30: Sparkle maneja detección programada y la alerta estándar con
        // "Install on Quit". Debe vivir después de crear la ventana.
        SparkleBridge::startup();
       #endif

        // Arrancar en pantalla completa (modo kiosco): tapa la barra de tareas.
        if (commandLineParameters.contains("--fullscreen"))
            juce::Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);

        if (commandLineParameters.contains("--snapshot"))
        {
            auto path = commandLineParameters.fromFirstOccurrenceOf("--snapshot", false, false).trim();
            if (path.startsWithChar('='))
                path = path.substring(1).trim();
            if (path.isEmpty())
                path = "/tmp/discnativepro_snap.png";

            juce::Timer::callAfterDelay(400, [this, path]() {
                if (mainWindow != nullptr && mainWindow->getContentComponent() != nullptr)
                {
                    auto* comp = mainWindow->getContentComponent();
                    auto img = comp->createComponentSnapshot(comp->getLocalBounds());
                    juce::File outFile(path);
                    outFile.deleteFile();
                    juce::FileOutputStream stream(outFile);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                    }
                }
                quit();
            });
        }
    }

    void shutdown() override
    {
        // P30 (Windows Setup): si hay una actualización descargada y confirmada,
        // el instalador Inno se lanza /VERYSILENT justo antes de morir el
        // proceso — /CloseApplications=force termina esta instancia.
        UpdateService::runPendingInstaller();

       #if JUCE_MAC
        SparkleBridge::shutdown();
       #endif
        // ORT antes que nada: libera la sesión en contexto vivo (el teardown
        // estático de exit() la mata con abort()).
        StemSeparationService::getInstance().shutdown();
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        if (mainWindow == nullptr)
            return;

        if (mainWindow->isMinimised())
            mainWindow->setMinimised(false);

        mainWindow->setVisible(true);
        mainWindow->toFront(true);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Colour::fromRGB(12, 13, 16),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
           #else
            setResizable(true, true);
            setResizeLimits(900, 600, 3840, 2160);

            // P27: el tamaño natural del layout (1360x860) puede exceder el
            // área de trabajo lógica en pantallas chicas o con scaling de
            // Windows al 125/150 %; se acota al display antes de centrar.
            juce::Rectangle<int> workArea(0, 0, 1360, 860);
            if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                workArea = disp->userArea;
            // P27-bis: arrancar ocupando toda el area util (antes se limitaba a
            // 1360x860 y en pantallas grandes quedaba una ventana pequena con
            // franjas negras alrededor). F11 sigue dando pantalla completa real.
            const int w = juce::jlimit(900, 3840, workArea.getWidth());
            const int h = juce::jlimit(600, 2160, workArea.getHeight());
            centreWithSize(w, h);
           #endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    // Declarado antes que mainWindow: los componentes se destruyen primero y
    // el LookAndFeel global sigue vivo mientras alguien lo referencia.
    AppLookAndFeel appLookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(DiscNativeProApplication)
