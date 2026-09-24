#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"

class DiscNativeProApplication : public juce::JUCEApplication
{
public:
    DiscNativeProApplication() {}

    const juce::String getApplicationName() override       { return "DiscNativePro"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLineParameters) override
    {
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
