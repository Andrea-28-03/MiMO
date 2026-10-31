#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#ifndef JUCE_USE_CAMERA
 #define JUCE_USE_CAMERA 1
#endif
#include <juce_video/juce_video.h>
#include "MiMOLookAndFeel.h"
#include "CanvasGraph.h"
#include "CanvasGraphEditor.h"
#include "CanvasAnimation.h"
#include "MiMOEyeDrawer.h"
#include "CanvasRenderer.h"
#include "SidebarComponent.h"
#include "BottomDock.h"
#include "LiveAlbumView.h"
#include "CanvasInteraction.h"
#include "CanvasUndoManager.h"
#include "../Devices/HardwareWorkers.h"
#include "../Devices/HardwareManager.h"
#include "../Networking/MiMOEyeServer.h"
#include "../Audio/AudioEngine.h"
#include <vector>
#include <atomic>
#include <memory>

namespace mimo
{

class MainComponent : public juce::Component,
                      private juce::Timer,
                      public juce::AudioIODeviceCallback,
                      public juce::CameraDevice::Listener,
                      public juce::DarkModeSettingListener
{
public:
    friend class MicWorker;
    friend class CameraWorker;
    friend class HardwareManager;
    friend class HandTracker;
    friend class PresetCatalog;
    friend class CanvasGraphEditor;
    friend class CanvasAnimation;
    friend class CanvasRenderer;
    friend class SidebarComponent;
    friend class BottomDock;
    friend class LiveAlbumView;
    friend class CanvasInteraction;
    friend class CanvasContextMenu;
    friend class ShortcutsModal;
    friend void MiMOEyeDrawer::drawModernFlatEyeModal(juce::Graphics& g, MainComponent& comp);
    friend bool MiMOEyeDrawer::handleMouseDown(MainComponent& comp, const juce::Point<float>& pt);

    // Type aliases per compatibilità retroattiva
    using VideoSourceItem = mimo::VideoSourceItem;
    using KnobParam = mimo::KnobParam;
    using InnerFaustBlock = mimo::InnerFaustBlock;
    using ModOutputDef = mimo::ModOutputDef;
    using NodeOutputDef = mimo::NodeOutputDef;
    using NodeOutputGroup = mimo::NodeOutputGroup;
    using NodeType = mimo::NodeType;
    using CanvasNode = mimo::CanvasNode;
    using CanvasCable = mimo::CanvasCable;
    using GraphSnapshot = mimo::GraphSnapshot;
    using CatalogItem = mimo::CatalogItem;
    using PolaroidItem = mimo::PolaroidItem;
    using PinHit = mimo::PinHit;
    using KnobHit = mimo::KnobHit;
    using ActiveTab = mimo::ActiveTab;
    using LooperState = mimo::LooperState;
    using DragMode = mimo::DragMode;
    using HwState = mimo::HwState;
    using CameraTrackingMode = mimo::CameraTrackingMode;

    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // DarkModeSettingListener
    void darkModeSettingChanged() override;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // CameraDevice::Listener
    void imageReceived (const juce::Image& image) override { HardwareManager::processWebcamImage(*this, image); }

    // Interazioni Mouse & Tastiera Nativa JUCE
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    MiMOLookAndFeel lookAndFeel;
    juce::Image paperTextureCache;

    // Stato Attivo dell'Applicazione
    ActiveTab activeTab = ActiveTab::Studio;

    // Pan & Zoom liberi sul Canvas Infinito
    juce::Point<float> canvasOffset { 540.0f, 360.0f };
    juce::Point<float> panVelocity { 0.0f, 0.0f };
    float timeSinceLastWheel = 1.0f;
    float canvasZoom = 1.0f;
    int boilTick = 0;
    int frameCounter = 0;

    // Grafo corrente nel Workspace STUDIO
    std::vector<CanvasNode> nodes;
    std::vector<CanvasCable> cables;

    // Sidebar destra a scomparsa
    bool sidebarOpen = true;
    bool integratedSectionOpen = true;
    bool instrumentsSectionOpen = true;
    bool effectsSectionOpen = false;
    bool utilitiesSectionOpen = false;
    bool settingsOpen = false;
    bool shortcutsModalOpen = false;
    std::vector<CatalogItem> instrumentCatalog;
    std::vector<CatalogItem> effectCatalog;
    std::vector<CatalogItem> utilityCatalog;
    float sidebarScrollY = 0.0f;

    // Moduli Integrati
    float micSensitivity = 0.65f;
    bool micTriggerFired = false;
    int micTriggerTicks = 0;

    bool cameraSettingsOpen = false;
    float cameraSensitivity = 0.70f;
    bool cameraMirror = true;
    CameraTrackingMode cameraTrackingMode = CameraTrackingMode::Motion;

    // Microfono + Sintesi Audio
    std::unique_ptr<MicWorker> micWorker;
    std::atomic<int> micStateAtomic { 0 };
    juce::CriticalSection micInfoLock;
    juce::String micStatusText;
    juce::String micDeviceName;
    juce::StringArray micDeviceList;
    std::atomic<int> micDeviceIndex { -1 };
    bool isMicRecording = false;
    float lastDrawnMicLevel = -1.0f;
    std::atomic<float> realMicLevel { 0.0f };
    std::atomic<float> realMicPeakRaw { 0.0f };
    std::atomic<float> realMicVelocity { 0.0f };
    std::atomic<bool> realMicTriggered { false };
    std::atomic<float> micThresholdAtomic { 0.45f };
    double micSampleRate = 48000.0;
    int micHoldSamples = 0;
    bool micArmed = true;
    float getMicThreshold() const noexcept { return juce::jlimit(0.05f, 0.95f, 0.96f - micSensitivity * 0.86f); }

    // Motore Audio Modulare DSP
    mimo::audio::AudioEngine audioEngine;
    std::atomic<float> triggerPulseAlpha { 0.0f };
    std::atomic<int> lastTriggerSourceCode { 0 };
    std::atomic<int> lastTriggerFingerIdx { -1 };
    std::atomic<bool> lastTriggerIsSecondary { false };
    static juce::String getSoundNameForId(int soundId);

    // Sintetizzatore percussivo / sonoro interno
    std::atomic<bool> synthTriggerPending { false };
    std::atomic<float> synthHitVelocity { 0.8f };
    std::atomic<int> synthInstrumentType { 0 };
    std::atomic<float> synthFilterCutoffHz { 2400.0f };
    std::atomic<float> synthFilterReso { 0.3f };
    std::atomic<float> synthMasterGain { 1.20f };
    std::atomic<float> appMasterVolume { 1.20f };
    void setAppMasterVolume(float newVolume);
    float getAppMasterVolume() const noexcept { return appMasterVolume.load(); }
    void saveSettings();
    void loadSettings();
    double synthSampleRate = 48000.0;
    float synthEnv = 0.0f;
    float synthPitchEnv = 0.0f;
    float synthPhase = 0.0f;
    float synthFilterLp = 0.0f;
    float synthFilterBp = 0.0f;
    uint32_t synthRng = 22222u;

    void setMicStatus(HwState st, const juce::String& text) { HardwareManager::setMicStatus(*this, st, text); }
    juce::String getMicStatusText() const { return HardwareManager::getMicStatusText(*this); }
    juce::String getMicDeviceLabel() const { return HardwareManager::getMicDeviceLabel(*this); }

    // Webcam
    std::unique_ptr<CameraWorker> cameraWorker;
    std::atomic<int> cameraState { 0 };
    juce::CriticalSection cameraInfoLock;
    juce::String cameraStatusText;
    juce::String cameraDeviceName;
    juce::StringArray cameraDeviceList;
    std::atomic<int> cameraDeviceIndex { -1 };
    std::atomic<int> cameraFrameCounter { 0 };
    bool isCameraRecording = false;
    juce::CriticalSection cameraLock;
    juce::Image latestWebcamImage;
    juce::Image prevWebcamImage;
    std::atomic<float> realCameraMotion { 0.0f };
    std::atomic<float> realCameraBrightness { 0.0f };
    std::atomic<float> realCameraCentroidX { 0.0f };
    std::atomic<float> realCameraCentroidY { 0.0f };
    std::atomic<bool> hasNewWebcamFrame { false };

    void setCameraStatus(HwState st, const juce::String& text) { HardwareManager::setCameraStatus(*this, st, text); }
    juce::String getCameraStatusText() const { return HardwareManager::getCameraStatusText(*this); }
    juce::String getCameraDeviceLabel() const { return HardwareManager::getCameraDeviceLabel(*this); }

    bool isDrumInCanvas() const;
    bool isMicInCanvas() const;
    bool isCameraInCanvas() const;
    bool isHandEnabled(bool isSecondary) const;
    bool isHandTriggersEnabled(bool isSecondary) const;
    bool isGroupEnabled(const juce::String& groupId) const;
    void updateHardwareDeviceStates() { HardwareManager::updateHardwareDeviceStates(*this); }
    void startRealMicrophone() { HardwareManager::startRealMicrophone(*this); }
    void stopRealMicrophone() { HardwareManager::stopRealMicrophone(*this); }
    void startRealCamera() { HardwareManager::startRealCamera(*this); }
    void stopRealCamera() { HardwareManager::stopRealCamera(*this); }
    void cycleMicDevice() { HardwareManager::cycleMicDevice(*this); }
    void cycleCameraDevice() { HardwareManager::cycleCameraDevice(*this); }
    void toggleDrumInCanvas() { CanvasGraphEditor::toggleDrumInCanvas(*this); }
    void toggleMicInCanvas() { CanvasGraphEditor::toggleMicInCanvas(*this); }
    void toggleCameraInCanvas() { CanvasGraphEditor::toggleCameraInCanvas(*this); }
    static juce::Rectangle<float> getIntegratedDeviceRowBounds(const juce::Rectangle<float>& nodeBox)
    {
        return { nodeBox.getX() + 12.0f, nodeBox.getBottom() - 26.0f, nodeBox.getWidth() - 24.0f, 19.0f };
    }

    // MiMO Eye AI Server & Drawer
    std::unique_ptr<mimo::MiMOEyeServer> eyeServer;
    bool eyeDrawerOpen = false;
    int eyeDrawerTab = 0;
    bool camDropdownOpen = false;
    bool micDropdownOpen = false;
    bool useSmartphoneEye = false;
    bool eyeShowVideo = true;
    mimo::HandTrackingState latestEyeState;
    mimo::HandTrackingState smoothedHandState;
    bool hasSmoothedHandState = false;
    mimo::HandTrackingState analyzeHandFrame(const juce::Image& frame);

    int eyeHandFilter = 0;
    int eyePhalanxDetail = 2;
    bool eyeGesturePos3D = true;
    bool eyeGestureFistTilt = true;
    bool eyeGestureFingerFlex = false;
    bool eyeGesturePinchTrig = true;

    void syncEyeNodeOutputsWithDetailSettings() { HardwareManager::syncEyeNodeOutputsWithDetailSettings(*this); }
    std::vector<VideoSourceItem> getVideoSourceItems() const { return HardwareManager::getVideoSourceItems(*this); }
    void selectVideoSource(bool useSmartphone, int pcDeviceIdx = -1, const juce::String& pcDeviceName = {})
    {
        HardwareManager::selectVideoSource(*this, useSmartphone, pcDeviceIdx, pcDeviceName);
    }
    juce::StringArray getCameraDropdownOptions() { return HardwareManager::getCameraDropdownOptions(*this); }
    juce::StringArray getMicDropdownOptions() { return HardwareManager::getMicDropdownOptions(*this); }

    // Editor Nodi Nested a tutto schermo e Selezione Nodi (Multipla)
    juce::String expandedNodeId;
    juce::String selectedNodeId;
    std::vector<juce::String> selectedNodeIds;
    std::map<juce::String, juce::Point<float>> dragStartNodeOffsets;
    juce::Point<float> lassoStartWorld;
    juce::Point<float> lassoCurrentWorld;
    juce::Point<float> rightMouseDownPos;
    bool isRightClickPan = false;

    bool isNodeSelected(const juce::String& id) const
    {
        return std::find(selectedNodeIds.begin(), selectedNodeIds.end(), id) != selectedNodeIds.end() || (selectedNodeId == id);
    }
    void selectSingleNode(const juce::String& id)
    {
        selectedNodeIds.clear();
        if (id.isNotEmpty())
            selectedNodeIds.push_back(id);
        selectedNodeId = id;
    }
    void toggleNodeSelection(const juce::String& id)
    {
        auto it = std::find(selectedNodeIds.begin(), selectedNodeIds.end(), id);
        if (it != selectedNodeIds.end())
        {
            selectedNodeIds.erase(it);
            selectedNodeId = selectedNodeIds.empty() ? juce::String() : selectedNodeIds.back();
        }
        else
        {
            selectedNodeIds.push_back(id);
            selectedNodeId = id;
        }
    }
    void clearNodeSelection()
    {
        selectedNodeIds.clear();
        selectedNodeId.clear();
    }
    void bringNodeToFront(const juce::String& id)
    {
        auto it = std::find_if(nodes.begin(), nodes.end(), [&](const CanvasNode& n) { return n.id == id; });
        if (it != nodes.end() && it != nodes.end() - 1)
        {
            CanvasNode nodeCopy = *it;
            nodes.erase(it);
            nodes.push_back(nodeCopy);
        }
    }
    juce::Rectangle<float> getLassoWorldRect() const
    {
        const float x1 = std::min(lassoStartWorld.x, lassoCurrentWorld.x);
        const float y1 = std::min(lassoStartWorld.y, lassoCurrentWorld.y);
        const float x2 = std::max(lassoStartWorld.x, lassoCurrentWorld.x);
        const float y2 = std::max(lassoStartWorld.y, lassoCurrentWorld.y);
        return { x1, y1, std::max(1.0f, x2 - x1), std::max(1.0f, y2 - y1) };
    }

    // Gestione Cronologia Undo/Redo e Clipboard Copia/Incolla
    CanvasUndoManager undoManager;
    void pushUndoSnapshot()
    {
        undoManager.pushSnapshot(nodes, cables);
    }
    void undo();
    void redo();
    void copySelectedNodes();
    void pasteNodes();

    // Sistema Polaroid, Album e 5 Slot Scena + 1-Button Looper
    bool hasPendingPolaroid = false;
    PolaroidItem pendingPolaroid;
    bool polaroidHasTyped = false;
    int polaroidCountdownTicks = 18;
    std::vector<PolaroidItem> photoAlbum;
    PolaroidItem sceneSlots[5];
    int activeSceneSlot = 0;
    int editingSlotIndex = -1;

    // Stato Workspace LIVE
    int selectedAlbumIndex = -1;
    juce::String filterEngine = "ALL";
    int hoveredSlotIndex = -1;
    int holdSlotIndex = -1;
    float holdProgress = 0.0f;
    int snappedSlotIndex = -1;
    float snapAnimScale = 1.0f;
    float looperSweepProgress = 0.0f;
    double looperStartTime = 0.0;
    double looperDurationSec = 4.0;
    LooperState looperState = LooperState::Idle;

    void initDefaultLivePresets();
    void toggleLooper();
    void clearLooper();
    void triggerSceneSlot(int slotIdx);

    // Drag & Drop
    DragMode dragMode = DragMode::None;
    juce::Point<float> dragStartMouse;
    juce::Point<float> dragStartOffset;
    juce::String draggedNodeId;
    juce::String draggedBlockId;
    juce::String draggedParamKey;
    float dragStartKnobNorm = 0.0f;

    // Disegno Cavo a Mano Libera
    juce::String cableSourceNodeId;
    juce::String cableSourcePinId;
    bool cableSourceIsOutput = true;
    bool cableSourceIsMod = false;
    bool cableSourceIsTrigger = false;
    MiMOLookAndFeel::PinShapeType cableSourceShape = MiMOLookAndFeel::PinShapeType::AudioCircle;
    juce::Colour cableSourceColour;
    juce::Point<float> cableStartWorld;
    juce::Point<float> cableCurrentWorld;
    std::vector<juce::Point<float>> cableFreehandWorld;

    // Selezione Cavi e Pin
    juce::String selectedCableId;
    juce::String selectedPinNodeId;
    juce::String selectedPinId;

    bool isUnhookingExistingCable = false;
    CanvasCable unhookedOriginalCable;
    juce::Point<float> unhookDragStartWorld;

    juce::String hoveredPinNodeId;
    juce::String hoveredPinId;
    juce::Point<float> hoveredPinPos;
    bool hasHoveredPin = false;

    CatalogItem draggedCatalogItem;
    PolaroidItem draggedPolaroid;
    juce::Point<float> currentMousePos;
    juce::String hoveredHwNodeId;
    juce::String hoveredAudioCableId;

    // Inizializzazione, Layout, Hit-Testing e Rendering
    void initCatalogs();
    void initDefaultStudioGraph();
    CanvasNode createFxNodeFromCatalog(const CatalogItem& fx, juce::Point<float> worldPos, int uniqueSeed, bool isVertical = false)
    {
        return CanvasGraphHelper::createFxNodeFromCatalog(fx, worldPos, uniqueSeed, isVertical);
    }
    CanvasNode createInstrumentNodeFromCatalog(const CatalogItem& item, juce::Point<float> worldPos, int uniqueSeed = 1)
    {
        return CanvasGraphHelper::createInstrumentNodeFromCatalog(item, worldPos, uniqueSeed);
    }
    void syncAudioRoutes();

    juce::Point<float> screenToWorld(juce::Point<float> screenPt) const noexcept
    {
        return CanvasGeometry::screenToWorld(screenPt, canvasOffset, canvasZoom);
    }
    juce::Point<float> worldToScreen(juce::Point<float> worldPt) const noexcept
    {
        return CanvasGeometry::worldToScreen(worldPt, canvasOffset, canvasZoom);
    }
    juce::Rectangle<float> worldToScreenRect(juce::Rectangle<float> worldRect) const noexcept
    {
        return CanvasGeometry::worldToScreenRect(worldRect, canvasOffset, canvasZoom);
    }

    float getNodeCalculatedHeight(const CanvasNode& node) const
    {
        return CanvasGeometry::getNodeCalculatedHeight(node);
    }
    std::vector<PinHit> collectAllPins() const
    {
        return CanvasGeometry::collectAllPins(nodes);
    }
    juce::Point<float> getPinWorldPos(const juce::String& nodeId, const juce::String& pinId) const
    {
        return CanvasGeometry::getPinWorldPos(nodes, nodeId, pinId);
    }
    juce::Point<float> getPinNormal(const juce::String& nodeId, const juce::String& pinId) const
    {
        return CanvasGeometry::getPinNormal(nodes, nodeId, pinId);
    }
    std::vector<KnobHit> collectCanvasKnobs() const
    {
        return CanvasGeometry::collectCanvasKnobs(nodes);
    }

    juce::String findAudioCableNear(juce::Point<float> worldPt, float maxDist = 20.0f) const
    {
        return CanvasGeometry::findAudioCableNear(cables, nodes, worldPt, maxDist);
    }
    juce::String findHwModuleAt(juce::Point<float> worldPt) const
    {
        return CanvasGeometry::findHwModuleAt(nodes, worldPt);
    }
    void insertEffectOnCable(const CatalogItem& fx, const juce::String& cableId, juce::Point<float> worldPos)
    {
        CanvasGraphEditor::insertEffectOnCable(*this, fx, cableId, worldPos);
    }
    void deleteSelectedEffect()
    {
        CanvasGraphEditor::deleteSelectedEffect(*this);
    }
    void deleteNode(const juce::String& nodeId)
    {
        CanvasGraphEditor::deleteNode(*this, nodeId);
    }

    // Azioni Polaroid
    void triggerSnapPolaroid() { LiveAlbumView::triggerSnapPolaroid(*this); }
    void commitPolaroidToAlbum(const PolaroidItem& item) { LiveAlbumView::commitPolaroidToAlbum(*this, item); }
    void assignPolaroidToSlot(int slotIdx, const PolaroidItem& item) { LiveAlbumView::assignPolaroidToSlot(*this, slotIdx, item); }

    // Inoltro Layout e Bounds
    juce::Rectangle<float> getLooperBounds() const { return BottomDock::getLooperBounds(getWidth(), getHeight(), activeTab); }
    juce::Rectangle<float> getDockTabBounds(int tabIdx) const { return BottomDock::getDockTabBounds(tabIdx, getWidth(), getHeight()); }
    juce::Rectangle<float> getSlotBounds(int slotIdx) const { return BottomDock::getSlotBounds(slotIdx, getWidth(), getHeight(), activeTab); }
    juce::Rectangle<float> getSidebarToggleBounds() const { return SidebarComponent::getSidebarToggleBounds(getWidth(), getHeight(), sidebarOpen); }
    juce::Rectangle<float> getCameraButtonBounds() const { return SidebarComponent::getCameraButtonBounds(getWidth(), getHeight(), sidebarOpen); }
    juce::Rectangle<float> getSettingsButtonBounds() const { return SidebarComponent::getSettingsButtonBounds(getWidth(), getHeight(), sidebarOpen); }
    juce::Rectangle<float> getSettingsPanelBounds() const { return SidebarComponent::getSettingsPanelBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getSettingsVolumeTrackBounds() const { return SidebarComponent::getSettingsVolumeTrackBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getSettingsVolDownBtnBounds() const { return SidebarComponent::getSettingsVolDownBtnBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getSettingsVol100BtnBounds() const { return SidebarComponent::getSettingsVol100BtnBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getSettingsVol150BtnBounds() const { return SidebarComponent::getSettingsVol150BtnBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getSettingsVolUpBtnBounds() const { return SidebarComponent::getSettingsVolUpBtnBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getIntegratedHeaderBounds() const { return SidebarComponent::getIntegratedHeaderBounds(getWidth(), getHeight(), sidebarScrollY); }
    juce::Rectangle<float> getIntegratedBodyBounds() const { return SidebarComponent::getIntegratedBodyBounds(getWidth(), getHeight(), sidebarScrollY, cameraSettingsOpen); }
    juce::Rectangle<float> getDrumSwitchBounds() const { return SidebarComponent::getDrumSwitchBounds(getWidth(), getHeight(), sidebarScrollY, cameraSettingsOpen); }
    juce::Rectangle<float> getMicSwitchBounds() const { return SidebarComponent::getMicSwitchBounds(getWidth(), getHeight(), sidebarScrollY, cameraSettingsOpen); }
    juce::Rectangle<float> getMicSliderBounds() const { return {}; }
    juce::Rectangle<float> getCameraSwitchBounds() const { return SidebarComponent::getCameraSwitchBounds(getWidth(), getHeight(), sidebarScrollY, cameraSettingsOpen); }
    juce::Rectangle<float> getCameraSettingsButtonBounds() const { return SidebarComponent::getCameraSettingsButtonBounds(getWidth(), getHeight(), sidebarScrollY, cameraSettingsOpen); }
    juce::Rectangle<float> getCameraSliderBounds() const { return {}; }
    juce::Rectangle<float> getCameraModePillBounds(int modeIdx) const { return SidebarComponent::getCameraModePillBounds(getWidth(), getHeight(), sidebarScrollY, modeIdx); }
    juce::Rectangle<float> getCameraMirrorCheckboxBounds() const { return SidebarComponent::getCameraMirrorCheckboxBounds(getWidth(), getHeight(), sidebarScrollY); }
    juce::Rectangle<float> getInstrumentsHeaderBounds() const { return SidebarComponent::getInstrumentsHeaderBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen); }
    juce::Rectangle<float> getInstrumentsBodyBounds() const { return SidebarComponent::getInstrumentsBodyBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen, instrumentCatalog.size()); }
    juce::Rectangle<float> getEffectsHeaderBounds() const { return SidebarComponent::getEffectsHeaderBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen, instrumentsSectionOpen, instrumentCatalog.size()); }
    juce::Rectangle<float> getEffectsBodyBounds() const { return SidebarComponent::getEffectsBodyBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen, instrumentsSectionOpen, instrumentCatalog.size(), effectCatalog.size()); }
    juce::Rectangle<float> getUtilitiesHeaderBounds() const { return SidebarComponent::getUtilitiesHeaderBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen, instrumentsSectionOpen, instrumentCatalog.size(), effectsSectionOpen, effectCatalog.size()); }
    juce::Rectangle<float> getUtilitiesBodyBounds() const { return SidebarComponent::getUtilitiesBodyBounds(getWidth(), getHeight(), sidebarScrollY, integratedSectionOpen, cameraSettingsOpen, instrumentsSectionOpen, instrumentCatalog.size(), effectsSectionOpen, effectCatalog.size(), utilityCatalog.size()); }
    float getSidebarContentHeight() const { return SidebarComponent::getSidebarContentHeight(integratedSectionOpen, cameraSettingsOpen, instrumentsSectionOpen, instrumentCatalog.size(), effectsSectionOpen, effectCatalog.size(), utilitiesSectionOpen, utilityCatalog.size()); }
    float getSidebarMaxScroll() const { return SidebarComponent::getSidebarMaxScroll(getHeight(), getSidebarContentHeight()); }
    juce::Rectangle<float> getSidebarViewportBounds() const { return SidebarComponent::getSidebarViewportBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getPolaroidCardBounds() const { return { static_cast<float>(getWidth()) * 0.5f - 175.0f, static_cast<float>(getHeight()) * 0.5f - 210.0f, 350.0f, 390.0f }; }

    // Gestione Modalità Notte
    bool isDarkMode = true;
    static bool detectSystemDarkMode();
    void setDarkMode(bool dark);
    void toggleDarkMode();

    int fxCounter = 2;
    juce::Image mimoImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace mimo
