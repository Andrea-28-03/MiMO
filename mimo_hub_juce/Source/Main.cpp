#include <juce_gui_basics/juce_gui_basics.h>
#include "GUI/MainComponent.h"

namespace mimo
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name)
        : DocumentWindow(name,
                         MiMOLookAndFeel::paperBg,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);

        #if JUCE_IOS || JUCE_ANDROID
        setFullScreen(true);
        #else
        setResizable(true, true);
        setResizeLimits(1024, 640, 2560, 1600);
        centreWithSize(getWidth(), getHeight());
        #endif

        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class MiMOHubApplication : public juce::JUCEApplication
{
public:
    MiMOHubApplication() = default;

    const juce::String getApplicationName() override       { return "MiMO Hub"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace mimo

// Registrazione standard del punto d'ingresso JUCE
START_JUCE_APPLICATION(mimo::MiMOHubApplication)
