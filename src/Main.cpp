#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"

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

        mainWindow.reset(new MainWindow(getApplicationName()));

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
            centreWithSize(getWidth(), getHeight());
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
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(DiscNativeProApplication)
