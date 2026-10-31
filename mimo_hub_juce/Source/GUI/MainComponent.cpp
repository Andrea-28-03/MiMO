#include "MainComponent.h"
#include "PresetCatalog.h"
#include "../Devices/HandTracker.h"
#include <BinaryData.h>
#include <cmath>
#include <algorithm>

namespace mimo
{

MainComponent::MainComponent()
{
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    const bool sysDark = detectSystemDarkMode();
    setDarkMode(sysDark);
    juce::Desktop::getInstance().addDarkModeSettingListener(this);

    mimoImage = juce::ImageFileFormat::loadFrom(BinaryData::mimo_device_png, BinaryData::mimo_device_pngSize);
    if (mimoImage.isNull())
    {
        juce::Array<juce::File> searchList = {
            juce::File::getCurrentWorkingDirectory().getChildFile("Resources/mimo_device.png"),
            juce::File::getCurrentWorkingDirectory().getChildFile("mimo_hub_juce/Resources/mimo_device.png"),
            juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("Resources/mimo_device.png"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/Resources/mimo_device.png"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/Immagini/MIMO.png")
        };
        for (const auto& f : searchList)
        {
            if (f.existsAsFile())
            {
                mimoImage = juce::ImageFileFormat::loadFrom(f);
                if (mimoImage.isValid())
                    break;
            }
        }
    }

    if (mimoImage.isValid() && mimoImage.getWidth() > 680)
        mimoImage = mimoImage.rescaled(640, 400, juce::Graphics::highResamplingQuality);

    initCatalogs();
    initDefaultStudioGraph();
    initDefaultLivePresets();

    setSize(1280, 800);
    canvasOffset = { 500.0f, 320.0f };
    panVelocity = { 0.0f, 0.0f };
    timeSinceLastWheel = 1.0f;

    audioEngine.startAudio();
    loadSettings();
    setAppMasterVolume(appMasterVolume.load());

    micWorker = std::make_unique<MicWorker>(*this);
    micWorker->startThread();

    cameraWorker = std::make_unique<CameraWorker>(*this);
    cameraWorker->startThread();

    {
        const auto initialDevs = juce::CameraDevice::getAvailableDevices();
        const juce::ScopedLock sl(cameraInfoLock);
        cameraDeviceList = initialDevs;
        int bestScore = -9999;
        int bestIdx = 0;
        for (int i = 0; i < initialDevs.size(); ++i)
        {
            const int sc = scoreCameraDeviceName(initialDevs[i]);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestIdx = i;
            }
        }
        if (!initialDevs.isEmpty())
        {
            cameraDeviceIndex.store(bestIdx);
            cameraDeviceName = initialDevs[bestIdx];
        }
    }

    eyeServer = std::make_unique<mimo::MiMOEyeServer>(8088, 8089);
    eyeServer->setSmartphoneExclusive(useSmartphoneEye);
    eyeServer->onPinchTrigger = [this](int fingerIdx, float vel, bool isSecondary)
    {
        if (!eyeGesturePinchTrig || eyePhalanxDetail < 1)
            return;
        if (!isHandTriggersEnabled(isSecondary))
            return;
        audioEngine.triggerFromSource("hw-eye", fingerIdx, vel, isSecondary);
        lastTriggerSourceCode.store(3);
        lastTriggerFingerIdx.store(fingerIdx);
        lastTriggerIsSecondary.store(isSecondary);
        triggerPulseAlpha.store(1.0f);
    };
    eyeServer->onSmartphoneFrame = [this](const juce::Image& phoneImg)
    {
        if (!useSmartphoneEye)
            return;

        if (phoneImg.isValid())
        {
            {
                const juce::ScopedLock sl(cameraLock);
                latestWebcamImage = phoneImg;
                hasNewWebcamFrame.store(true);
            }

            const auto handSt = analyzeHandFrame(phoneImg);
            const bool mpActive = (eyeServer != nullptr && eyeServer->isMediaPipeActive());
            if (!mpActive)
            {
                latestEyeState = handSt;
                if (eyeServer != nullptr)
                    eyeServer->updateFromLocalWebcam(handSt);
            }
        }
    };
    eyeServer->startServer();

    syncEyeNodeOutputsWithDetailSettings();
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    saveSettings();
    juce::Desktop::getInstance().removeDarkModeSettingListener(this);
    stopTimer();
    audioEngine.stopAudio();
    if (eyeServer != nullptr)
        eyeServer->stopServer();
    eyeServer.reset();
    cameraWorker.reset();
    micWorker.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (outputChannelData != nullptr)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }

    if (inputChannelData == nullptr || numInputChannels < 1 || numSamples <= 0)
        return;

    float peak = 0.0f;
    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        const float* in = inputChannelData[ch];
        if (in == nullptr)
            continue;
        const auto range = juce::FloatVectorOperations::findMinAndMax(in, numSamples);
        peak = std::max(peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
    }
    realMicPeakRaw.store(peak);

    const float db = juce::Decibels::gainToDecibels(peak, -80.0f);
    const float level = juce::jlimit(0.0f, 1.0f, (db + 58.0f) / 56.0f);

    const float current = realMicLevel.load();
    const float smoothed = (level > current) ? (current * 0.25f + level * 0.75f) : (current * 0.90f + level * 0.10f);
    realMicLevel.store(juce::jlimit(0.0f, 1.0f, smoothed));

    const float threshold = micThresholdAtomic.load();
    if (micHoldSamples > 0)
        micHoldSamples -= numSamples;

    if (level >= threshold && micArmed && micHoldSamples <= 0)
    {
        const float vel = juce::jlimit(0.25f, 1.0f, (level - threshold) / std::max(0.05f, 1.0f - threshold) * 0.75f + 0.25f);
        if (isMicInCanvas())
        {
            realMicVelocity.store(vel);
            realMicTriggered.store(true);
            audioEngine.triggerFromSource("hw-mic", 0, vel);
            lastTriggerSourceCode.store(2);
            triggerPulseAlpha.store(1.0f);
            synthHitVelocity.store(vel);
            synthTriggerPending.store(true);
        }
        micArmed = false;
        micHoldSamples = static_cast<int>(micSampleRate * 0.075);
    }
    else if (level < threshold - 0.06f)
    {
        micArmed = true;
    }
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr && device->getCurrentSampleRate() > 0.0)
        micSampleRate = device->getCurrentSampleRate();
    micHoldSamples = 0;
    micArmed = true;
}

void MainComponent::audioDeviceStopped() {}

mimo::HandTrackingState MainComponent::analyzeHandFrame(const juce::Image& frame)
{
    return HandTracker::analyzeHandFrame(*this, frame);
}

bool MainComponent::isDrumInCanvas() const
{
    for (const auto& n : nodes)
        if (n.id == "hw-drum")
            return true;
    return false;
}

bool MainComponent::isMicInCanvas() const
{
    for (const auto& n : nodes)
        if (n.isIntegratedMic || n.id == "hw-mic")
            return true;
    return false;
}

bool MainComponent::isCameraInCanvas() const
{
    for (const auto& n : nodes)
        if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
            return true;
    return false;
}

bool MainComponent::isHandEnabled(bool isSecondary) const
{
    const juce::String rootId = isSecondary ? "eye_sx" : "eye_dx";
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera)
        {
            for (const auto& grp : n.outputGroups)
                if (grp.id == rootId)
                    return grp.isEnabled;
        }
    }
    return true;
}

bool MainComponent::isHandTriggersEnabled(bool isSecondary) const
{
    if (!isHandEnabled(isSecondary))
        return false;

    const juce::String trigId = isSecondary ? "eye_sx_pinches" : "eye_dx_pinches";
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera)
        {
            for (const auto& grp : n.outputGroups)
                if (grp.id == trigId)
                    return grp.isEnabled;
        }
    }
    return true;
}

bool MainComponent::isGroupEnabled(const juce::String& groupId) const
{
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera || n.id == "hw-drum")
        {
            for (const auto& grp : n.outputGroups)
            {
                if (grp.id == groupId)
                {
                    if (!grp.parentGroup.isEmpty())
                    {
                        for (const auto& pGrp : n.outputGroups)
                            if (pGrp.id == grp.parentGroup && !pGrp.isEnabled)
                                return false;
                    }
                    return grp.isEnabled;
                }
            }
        }
    }
    return true;
}

void MainComponent::timerCallback()
{
    bool needsRepaint = false;
    CanvasAnimation::updateTimerTick(*this, needsRepaint);
    if (needsRepaint)
        repaint();
}

void MainComponent::initCatalogs()
{
    PresetCatalog::initCatalogs(instrumentCatalog, effectCatalog, utilityCatalog);
}

void MainComponent::syncAudioRoutes()
{
    std::vector<mimo::audio::NodeAudioRoute> routes;
    for (const auto& c : cables)
    {
        mimo::audio::NodeAudioRoute r;
        r.sourceNodeId = c.sourceNodeId;
        r.sourcePinId = c.sourcePinId;
        r.targetNodeId = c.targetNodeId;
        r.targetPinId = c.targetPinId;
        r.isModulation = c.isModulation;
        routes.push_back(r);
    }

    std::vector<mimo::audio::NodeGraphDescriptor> descriptors;
    for (const auto& n : nodes)
    {
        mimo::audio::NodeGraphDescriptor d;
        d.id = n.id;
        d.label = n.label;
        d.category = n.subtitle;
        d.engine = n.instrumentEngine;
        d.isUtility = n.id.startsWithIgnoreCase("ut-") || n.id.startsWithIgnoreCase("mod-") ||
                      n.id.containsIgnoreCase("lfo") || n.id.containsIgnoreCase("follower") ||
                      n.id.containsIgnoreCase("scale") || n.id.containsIgnoreCase("looper");
        d.isEffect = (n.type == NodeType::NestedEffect);
        d.isInstrument = !d.isUtility && !d.isEffect && (n.type == NodeType::InstrumentNode || n.hasInstrument);
        d.isBypassed = n.isBypassed;
        descriptors.push_back(d);
    }

    audioEngine.syncGraph(descriptors, routes);
}

void MainComponent::initDefaultStudioGraph()
{
    PresetCatalog::initDefaultStudioGraph(nodes, cables, effectCatalog);
    syncAudioRoutes();
}

void MainComponent::initDefaultLivePresets()
{
    PresetCatalog::initDefaultLivePresets(photoAlbum, sceneSlots, nodes, cables);
    activeSceneSlot = 0;
}

void MainComponent::toggleLooper()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (looperState == LooperState::Idle)
    {
        looperStartTime = now;
        looperState = LooperState::Recording;
    }
    else if (looperState == LooperState::Recording)
    {
        looperDurationSec = std::max(0.5, now - looperStartTime);
        looperStartTime = now;
        looperState = LooperState::Playing;
    }
    else if (looperState == LooperState::Playing)
    {
        looperState = LooperState::Overdubbing;
    }
    else if (looperState == LooperState::Overdubbing)
    {
        looperState = LooperState::Playing;
    }
    repaint();
}

void MainComponent::clearLooper()
{
    looperState = LooperState::Idle;
    looperSweepProgress = 0.0f;
    repaint();
}

void MainComponent::triggerSceneSlot(int slotIdx)
{
    if (slotIdx >= 0 && slotIdx < 5)
    {
        activeSceneSlot = slotIdx;
        repaint();
    }
}

juce::String MainComponent::getSoundNameForId(int soundId)
{
    switch (soundId)
    {
        case 0: return "Cassa";
        case 1: return "Rullante";
        case 2: return "Charleston";
        case 3: return "Percussione";
        case 4: return "Piano Do3";
        case 5: return "Piano Re3";
        case 6: return "Piano Mi3";
        case 7: return "Piano Sol3";
        case 8: return "Piano La3";
        case 9: return "Piano Do4";
        case 10: return "Synth Lead";
        case 11: return "Pad Gessetto";
        case 12: return "808 Sub";
        case 13: return "Latta Giocattolo";
        case 14: return "Carillon";
        default: return "Voce " + juce::String(soundId);
    }
}

void MainComponent::setAppMasterVolume(float newVolume)
{
    const float clamped = juce::jlimit(0.0f, 2.0f, newVolume);
    appMasterVolume.store(clamped);
    audioEngine.setMasterVolume(clamped);
    synthMasterGain.store(clamped);

    for (auto& n : nodes)
    {
        if (n.type == NodeType::MasterOut && !n.directParams.empty())
        {
            n.directParams[0].value = juce::jlimit(-48.0f, 6.0f, juce::Decibels::gainToDecibels(clamped, -48.0f));
            break;
        }
    }
    repaint();
}

void MainComponent::saveSettings()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MiMOHub");
    dir.createDirectory();
    auto f = dir.getChildFile("settings.txt");
    f.replaceWithText("volume=" + juce::String(appMasterVolume.load(), 3) + "\ndarkMode=" + juce::String(isDarkMode ? 1 : 0));
}

void MainComponent::loadSettings()
{
    const auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MiMOHub").getChildFile("settings.txt");
    if (f.existsAsFile())
    {
        const auto lines = juce::StringArray::fromLines(f.loadFileAsString());
        for (const auto& l : lines)
        {
            if (l.startsWithIgnoreCase("volume="))
            {
                const float v = l.fromFirstOccurrenceOf("volume=", false, false).getFloatValue();
                if (v >= 0.05f && v <= 2.0f)
                    appMasterVolume.store(v);
            }
            else if (l.startsWithIgnoreCase("darkMode="))
            {
                isDarkMode = (l.fromFirstOccurrenceOf("darkMode=", false, false).getIntValue() != 0);
                MiMOLookAndFeel::setDarkMode(isDarkMode);
            }
        }
    }
}

bool MainComponent::detectSystemDarkMode()
{
   #if JUCE_WINDOWS
    const juce::String regKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\AppsUseLightTheme";
    if (juce::WindowsRegistry::valueExists(regKey))
    {
        const juce::String val = juce::WindowsRegistry::getValue(regKey, "0");
        return (val.getIntValue() == 0);
    }
   #endif

    if (juce::Desktop::getInstance().isDarkModeActive())
        return true;

    return true;
}

void MainComponent::darkModeSettingChanged()
{
    setDarkMode(detectSystemDarkMode());
}

void MainComponent::setDarkMode(bool dark)
{
    isDarkMode = dark;
    MiMOLookAndFeel::setDarkMode(isDarkMode);
    paperTextureCache = juce::Image();
    repaint();
}

void MainComponent::toggleDarkMode()
{
    setDarkMode(!isDarkMode);
}

void MainComponent::paint(juce::Graphics& g)
{
    MiMOLookAndFeel::drawPaperTexture(g, getLocalBounds(), paperTextureCache);

    if (activeTab == ActiveTab::Studio)
    {
        CanvasRenderer::paintStudioCanvas(g, *this);
        SidebarComponent::paintRightSidebar(g, *this);
    }
    else if (activeTab == ActiveTab::Live)
    {
        LiveAlbumView::paintLiveAlbumView(g, *this);
    }
    else
    {
        LiveAlbumView::paintCommunityView(g, *this);
    }

    if (editingSlotIndex >= 0)
    {
        juce::Rectangle<float> saveBtn(static_cast<float>(getWidth()) * 0.5f - 195.0f, 16.0f, 390.0f, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, saveBtn, 99, MiMOLookAndFeel::crayonSage, 0.90f,
                                       MiMOLookAndFeel::fountainInk, 2.4f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("[OK] TORNA AL LIVE  //  Scena S" + juce::String(editingSlotIndex + 1) + " Salvata",
                   saveBtn.toNearestInt(), juce::Justification::centred);
    }

    if (expandedNodeId.isNotEmpty())
    {
        for (const auto& n : nodes)
        {
            if (n.id == expandedNodeId)
            {
                CanvasRenderer::paintNestedModal(g, *this, n);
                break;
            }
        }
    }

    if (hasPendingPolaroid)
        LiveAlbumView::paintPolaroidOverlay(g, *this);

    if (dragMode == DragMode::DragSidebarItem)
    {
        juce::Rectangle<float> ghost(currentMousePos.x - 85.0f, currentMousePos.y - 22.0f, 170.0f, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, ghost, 777, draggedCatalogItem.colour, 0.85f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(draggedCatalogItem.name, ghost.toNearestInt(), juce::Justification::centred);
    }
    else if (dragMode == DragMode::DragPolaroid)
    {
        juce::Rectangle<float> ghost(currentMousePos.x - 70.0f, currentMousePos.y - 45.0f, 140.0f, 90.0f);
        MiMOLookAndFeel::drawSketchBox(g, ghost, 778, MiMOLookAndFeel::crayonOchre, 0.65f,
                                       MiMOLookAndFeel::crayonOchre, 2.0f, false, MiMOLookAndFeel::polaroidWhite);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(draggedPolaroid.title, ghost.toNearestInt(), juce::Justification::centred);
    }

    BottomDock::paintBottomDockAndSlots(g, *this);

    if (eyeDrawerOpen)
        MiMOEyeDrawer::drawModernFlatEyeModal(g, *this);
}

void MainComponent::resized() {}

void MainComponent::mouseMove(const juce::MouseEvent& e)
{
    currentMousePos = e.position;
    if (activeTab == ActiveTab::Studio)
    {
        if (sidebarOpen && e.position.x >= getWidth() - 300)
            SidebarComponent::handleMouseDrag(*this, e.position);
        else
            CanvasInteraction::handleMouseMove(*this, e);
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    currentMousePos = e.position;

    if (eyeDrawerOpen)
    {
        if (MiMOEyeDrawer::handleMouseDown(*this, e.position))
            return;
    }

    if (editingSlotIndex >= 0)
    {
        juce::Rectangle<float> saveBtn(static_cast<float>(getWidth()) * 0.5f - 195.0f, 16.0f, 390.0f, 44.0f);
        if (saveBtn.contains(e.position))
        {
            activeTab = ActiveTab::Live;
            editingSlotIndex = -1;
            repaint();
            return;
        }
    }

    if (e.position.y >= getHeight() - 110)
    {
        if (BottomDock::handleDockMouseDown(*this, e.position))
            return;
    }

    if (activeTab == ActiveTab::Live)
    {
        LiveAlbumView::handleLiveMouseDown(*this, e.position, e);
        return;
    }

    if (activeTab == ActiveTab::Studio)
    {
        if (hasPendingPolaroid)
        {
            if (LiveAlbumView::handlePolaroidMouseDown(*this, e.position))
                return;
        }

        if (sidebarOpen && e.position.x >= getWidth() - 300)
        {
            if (SidebarComponent::handleMouseDown(*this, e.position, e))
                return;
        }
        else if (SidebarComponent::getSidebarToggleBounds(getWidth(), getHeight(), sidebarOpen).contains(e.position)
              || SidebarComponent::getCameraButtonBounds(getWidth(), getHeight(), sidebarOpen).contains(e.position)
              || SidebarComponent::getSettingsButtonBounds(getWidth(), getHeight(), sidebarOpen).contains(e.position)
              || (settingsOpen && SidebarComponent::getSettingsPanelBounds(getWidth(), getHeight()).contains(e.position)))
        {
            if (SidebarComponent::handleMouseDown(*this, e.position, e))
                return;
        }

        CanvasInteraction::handleMouseDown(*this, e);
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    currentMousePos = e.position;
    if (sidebarOpen && dragMode == DragMode::None && e.position.x >= getWidth() - 300)
        SidebarComponent::handleMouseDrag(*this, e.position);
    else
        CanvasInteraction::handleMouseDrag(*this, e);
}

void MainComponent::mouseUp(const juce::MouseEvent& e)
{
    currentMousePos = e.position;
    CanvasInteraction::handleMouseUp(*this, e);
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    CanvasInteraction::handleMouseDoubleClick(*this, e);
}

void MainComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (sidebarOpen && e.position.x >= getWidth() - 300)
        SidebarComponent::handleMouseWheel(*this, e, wheel);
    else
        CanvasInteraction::handleMouseWheelMove(*this, e, wheel);
}

void MainComponent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    CanvasInteraction::handleMouseMagnify(*this, e, scaleFactor);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    return CanvasInteraction::handleKeyPressed(*this, key);
}

} // namespace mimo
