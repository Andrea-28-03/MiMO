#include <juce_gui_basics/juce_gui_basics.h>
#include "GUI/MainComponent.h"
#include <cstdlib>

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
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
       #if JUCE_WINDOWS
        // Risoluzione autorizzazioni webcam Windows 10/11 per volumi virtuali (Google Drive H:\):
        // Quando un processo viene avviato da Google Drive for Desktop (H:\),
        // il Windows Camera Frame Server blocca la webcam fisica nativa (HP Wide Vision HD)
        // restituendo l'errore 0x80070490 (ERROR_NOT_FOUND).
        // Se rileviamo l'avvio da H:\ o da un'unità virtuale/cloud, copiamo l'eseguibile nella cartella locale
        // %LOCALAPPDATA%\MiMOHub e lo rilanciamo da disco fisso locale con pieni privilegi.
        const auto currentExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const juce::String localApp = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", "C:\\Users\\Utente\\AppData\\Local");
        const juce::File localDir = juce::File(localApp).getChildFile("MiMOHub");
        const juce::File targetExe = localDir.getChildFile("MiMO Hub.exe");

        // Se avviato dall'unità cloud virtuale H:\:
        // copia l'eseguibile su disco fisso locale (%LOCALAPPDATA%\MiMOHub) e lancia la copia locale
        // per garantire l'accesso hardware alla fotocamera da parte di Windows Camera Frame Server.
        if (currentExe.existsAsFile() && currentExe.getFullPathName().startsWithIgnoreCase("H:\\"))
        {
            localDir.createDirectory();
            if (!targetExe.existsAsFile()
                || currentExe.getLastModificationTime() > targetExe.getLastModificationTime()
                || currentExe.getSize() != targetExe.getSize())
            {
                targetExe.deleteFile();
                currentExe.copyFileTo(targetExe);
            }

            if (targetExe.existsAsFile() && targetExe.startAsProcess(commandLine))
            {
                std::exit(0);
            }
        }
       #endif

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
        if (mainWindow != nullptr)
        {
            mainWindow->toFront(true);
        }
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace mimo

// Registrazione standard del punto d'ingresso JUCE
START_JUCE_APPLICATION(mimo::MiMOHubApplication)
