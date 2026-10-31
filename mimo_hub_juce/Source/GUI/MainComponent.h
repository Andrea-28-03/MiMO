#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#ifndef JUCE_USE_CAMERA
 #define JUCE_USE_CAMERA 1
#endif
#include <juce_video/juce_video.h>
#include "MiMOLookAndFeel.h"
#include "CanvasGraph.h"
#include "MiMOEyeDrawer.h"
#include "../Devices/HardwareWorkers.h"
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

    // Type aliases per compatibilità retroattiva con i tipi estratti
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

    // DarkModeSettingListener (Sincronizzazione automatica con il tema di sistema)
    void darkModeSettingChanged() override;

    // AudioIODeviceCallback (Microfono reale del laptop)
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // CameraDevice::Listener (Videocamera/Webcam reale del laptop)
    void imageReceived (const juce::Image& image) override;

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

    // =========================================================================
    // Stato Attivo dell'Applicazione
    // =========================================================================
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

    // Sidebar destra a scomparsa con zone indipendenti (Moduli Integrati, Strumenti, Effetti)
    bool sidebarOpen = true;
    bool integratedSectionOpen = true;
    bool instrumentsSectionOpen = true;
    bool effectsSectionOpen = false;
    bool settingsOpen = false;
    std::vector<CatalogItem> instrumentCatalog;
    std::vector<CatalogItem> effectCatalog;

    // Moduli Integrati (Hardware Reale del Laptop: Microfono & Webcam)
    float micSensitivity = 0.65f;
    bool micTriggerFired = false;
    int micTriggerTicks = 0;

    bool cameraSettingsOpen = false;
    float cameraSensitivity = 0.70f;
    bool cameraMirror = true;
    CameraTrackingMode cameraTrackingMode = CameraTrackingMode::Motion;

    // --- Microfono + Sintesi Audio (gestiti su thread dedicato MicWorker: 0 ms blocco UI) ---
    std::unique_ptr<MicWorker> micWorker;
    std::atomic<int> micStateAtomic { 0 };         // HwState (scritto da MicWorker)
    juce::CriticalSection micInfoLock;
    juce::String micStatusText;
    juce::String micDeviceName;
    juce::StringArray micDeviceList;
    std::atomic<int> micDeviceIndex { -1 };        // -1 = selezione automatica microfono fisico
    bool isMicRecording = false;                   // desiderato (modulo nel canvas)
    float lastDrawnMicLevel = -1.0f;
    std::atomic<float> realMicLevel { 0.0f };      // 0..1 su scala dB (-58..0 dB)
    std::atomic<float> realMicPeakRaw { 0.0f };
    std::atomic<float> realMicVelocity { 0.0f };
    std::atomic<bool> realMicTriggered { false };
    std::atomic<float> micThresholdAtomic { 0.45f };
    double micSampleRate = 48000.0;                // audio thread
    int micHoldSamples = 0;                        // audio thread
    bool micArmed = true;                          // audio thread
    float getMicThreshold() const noexcept { return juce::jlimit(0.05f, 0.95f, 0.96f - micSensitivity * 0.86f); }

    // Motore Audio Modulare DSP (Thread Audio Master)
    mimo::audio::AudioEngine audioEngine;
    std::atomic<float> triggerPulseAlpha { 0.0f };
    std::atomic<int> lastTriggerSourceCode { 0 }; // 0=none, 1=hw-drum, 2=hw-mic, 3=hw-eye, 4=other

    // Sintetizzatore percussivo / sonoro interno (suona sui colpi del microfono o test)
    std::atomic<bool> synthTriggerPending { false };
    std::atomic<float> synthHitVelocity { 0.8f };
    std::atomic<int> synthInstrumentType { 0 };    // 0=Drum Kit, 1=808 Sub, 2=Kalimba/Marimba, 3=Synth
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

    void setMicStatus(HwState st, const juce::String& text);
    juce::String getMicStatusText() const;
    juce::String getMicDeviceLabel() const;

    // --- Webcam (aperta/chiusa su un thread dedicato: l'interfaccia non si blocca mai) ---
    std::unique_ptr<CameraWorker> cameraWorker;
    std::atomic<int> cameraState { 0 };            // HwState (scritto dal worker)
    juce::CriticalSection cameraInfoLock;
    juce::String cameraStatusText;
    juce::String cameraDeviceName;
    juce::StringArray cameraDeviceList;
    std::atomic<int> cameraDeviceIndex { -1 };     // -1 = selezione automatica webcam fisica
    std::atomic<int> cameraFrameCounter { 0 };
    bool isCameraRecording = false;                // desiderato (modulo nel canvas)
    juce::CriticalSection cameraLock;
    juce::Image latestWebcamImage;
    juce::Image prevWebcamImage;
    std::atomic<float> realCameraMotion { 0.0f };
    std::atomic<float> realCameraBrightness { 0.0f };
    std::atomic<float> realCameraCentroidX { 0.0f };
    std::atomic<float> realCameraCentroidY { 0.0f };
    std::atomic<bool> hasNewWebcamFrame { false };

    void setCameraStatus(HwState st, const juce::String& text);
    juce::String getCameraStatusText() const;
    juce::String getCameraDeviceLabel() const;

    bool isDrumInCanvas() const;
    bool isMicInCanvas() const;
    bool isCameraInCanvas() const;
    bool isHandEnabled(bool isSecondary) const;
    bool isHandTriggersEnabled(bool isSecondary) const;
    bool isGroupEnabled(const juce::String& groupId) const;
    void updateHardwareDeviceStates();
    void startRealMicrophone();
    void stopRealMicrophone();
    void startRealCamera();
    void stopRealCamera();
    void cycleMicDevice();
    void cycleCameraDevice();
    void toggleDrumInCanvas();
    void toggleMicInCanvas();
    void toggleCameraInCanvas();
    static juce::Rectangle<float> getIntegratedDeviceRowBounds(const juce::Rectangle<float>& nodeBox);

    // --- MiMO Eye AI Server & Drawer / Popup Modal + Tendine Canvas ---
    std::unique_ptr<mimo::MiMOEyeServer> eyeServer;
    bool eyeDrawerOpen = false;
    int eyeDrawerTab = 0; // 0 = Webcam PC & Sorgenti, 1 = Connetti Smartphone (QR)
    bool camDropdownOpen = false;
    bool micDropdownOpen = false;
    bool useSmartphoneEye = false;
    bool eyeShowVideo = true;
    mimo::HandTrackingState latestEyeState;
    mimo::HandTrackingState smoothedHandState;
    bool hasSmoothedHandState = false;
    mimo::HandTrackingState analyzeHandFrame(const juce::Image& frame);

    // Impostazioni Dettaglio Analisi Mano su PC (Mano, Numero Falangi, Gesture)
    int eyeHandFilter = 0;             // 0 = Entrambe, 1 = Mano Destra, 2 = Mano Sinistra
    int eyePhalanxDetail = 2;          // 0 = Palmo (1 pt), 1 = 5 Punte Dita (6 pt), 2 = 21 Falangi Complete
    bool eyeGesturePos3D = true;       // Assi X / Y / Profondita Z
    bool eyeGestureFistTilt = true;    // Chiusura Pugno + Rotazione Mano
    bool eyeGestureFingerFlex = false; // Flessione 4 Falangi Singole
    bool eyeGesturePinchTrig = true;   // Trigger Percussivi Pinch

    void syncEyeNodeOutputsWithDetailSettings();

    std::vector<VideoSourceItem> getVideoSourceItems() const;
    void selectVideoSource(bool useSmartphone, int pcDeviceIdx = -1, const juce::String& pcDeviceName = {});

    juce::StringArray getCameraDropdownOptions();
    juce::StringArray getMicDropdownOptions();
    void paintCanvasDropdowns(juce::Graphics& g);
    void drawModernFlatEyeCard(juce::Graphics& g, const CanvasNode& node);
    void drawModernFlatEyeModal(juce::Graphics& g);
    void drawHandSkeleton(juce::Graphics& g, juce::Rectangle<float> bounds, const mimo::HandTrackingState& state, bool overVideo = false);
    static void drawQrCode(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds);

    juce::Rectangle<float> getEyeModalBounds() const { return MiMOEyeDrawer::getEyeModalBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getEyeModalCloseBounds() const { return MiMOEyeDrawer::getEyeModalCloseBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getEyeModalTabBounds(int tabIdx) const { return MiMOEyeDrawer::getEyeModalTabBounds(getWidth(), getHeight(), tabIdx); }
    juce::Rectangle<float> getEyeModalUrlButtonBounds() const { return MiMOEyeDrawer::getEyeModalUrlButtonBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getEyeModalActivatePhoneButtonBounds() const { return MiMOEyeDrawer::getEyeModalActivatePhoneButtonBounds(getWidth(), getHeight()); }
    juce::Rectangle<float> getEyeModalHandFilterBounds(int filterIdx) const { return MiMOEyeDrawer::getEyeModalHandFilterBounds(getWidth(), getHeight(), filterIdx); }
    juce::Rectangle<float> getEyeModalPhalanxBounds(int detailIdx) const { return MiMOEyeDrawer::getEyeModalPhalanxBounds(getWidth(), getHeight(), detailIdx); }
    juce::Rectangle<float> getEyeModalGestureBounds(int gestureIdx) const { return MiMOEyeDrawer::getEyeModalGestureBounds(getWidth(), getHeight(), gestureIdx); }
    juce::Rectangle<float> getEyeModalPinchTestBounds(int pinchIdx) const { return MiMOEyeDrawer::getEyeModalPinchTestBounds(getWidth(), getHeight(), pinchIdx); }
    juce::Rectangle<float> getEyeModalCameraOptionBounds(int optIdx) const { return MiMOEyeDrawer::getEyeModalCameraOptionBounds(getWidth(), getHeight(), optIdx); }
    juce::Rectangle<float> getEyeModalViewToggleBounds() const { return MiMOEyeDrawer::getEyeModalViewToggleBounds(getWidth(), getHeight()); }
    static juce::Rectangle<float> getEyeCardConfigButtonBounds(const juce::Rectangle<float>& nodeBox) { return MiMOEyeDrawer::getEyeCardConfigButtonBounds(nodeBox); }
    static juce::Rectangle<float> getEyeCardViewToggleButtonBounds(const juce::Rectangle<float>& nodeBox) { return MiMOEyeDrawer::getEyeCardViewToggleButtonBounds(nodeBox); }

    // Editor Nodi Nested a tutto schermo (Doppio Clic) e Selezione Nodi
    juce::String expandedNodeId;
    juce::String selectedNodeId;

    // Sistema Polaroid, Album e 5 Slot Scena + 1-Button Looper
    bool hasPendingPolaroid = false;
    PolaroidItem pendingPolaroid;
    bool polaroidHasTyped = false;
    int polaroidCountdownTicks = 18;
    std::vector<PolaroidItem> photoAlbum;
    PolaroidItem sceneSlots[5];
    int activeSceneSlot = 0;
    int editingSlotIndex = -1;

    // Stato Workspace LIVE (Performance & Feedback I2C)
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
    juce::Rectangle<float> getLooperBounds() const;

    // =========================================================================
    // Stato di Interazione Mouse / Drag & Drop
    // =========================================================================
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

    // Distacco e re-routing cavi
    bool isUnhookingExistingCable = false;
    CanvasCable unhookedOriginalCable;
    juce::Point<float> unhookDragStartWorld;

    // Hover sui Pin e sui Cavi
    juce::String hoveredPinNodeId;
    juce::String hoveredPinId;
    juce::Point<float> hoveredPinPos;
    bool hasHoveredPin = false;

    // Drag & Drop dalla Sidebar Destra o Polaroid
    CatalogItem draggedCatalogItem;
    PolaroidItem draggedPolaroid;
    juce::Point<float> currentMousePos;
    juce::String hoveredHwNodeId;
    juce::String hoveredAudioCableId;

    // =========================================================================
    // Metodi Interni di Inizializzazione, Layout, Hit-Testing e Rendering
    // =========================================================================
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
    void insertEffectOnCable(const CatalogItem& fx, const juce::String& cableId, juce::Point<float> worldPos);
    void deleteSelectedEffect();
    void deleteNode(const juce::String& nodeId);

    // Azioni Polaroid
    void triggerSnapPolaroid();
    void commitPolaroidToAlbum(const PolaroidItem& item);
    void assignPolaroidToSlot(int slotIdx, const PolaroidItem& item);

    // Metodi di Disegno Sezioni
    void paintStudioCanvas(juce::Graphics& g);
    void paintCanvasNode(juce::Graphics& g, const CanvasNode& node);
    void paintRightSidebar(juce::Graphics& g);
    void paintNestedModal(juce::Graphics& g, const CanvasNode& node);
    void paintLiveAlbumView(juce::Graphics& g);
    void paintCommunityView(juce::Graphics& g);
    void paintPolaroidOverlay(juce::Graphics& g);
    void paintBottomDockAndSlots(juce::Graphics& g);
    void paintSettingsPanel(juce::Graphics& g);

    // Rettangoli UI fissi (Screen Space)
    juce::Rectangle<float> getSidebarToggleBounds() const;
    juce::Rectangle<float> getCameraButtonBounds() const;
    juce::Rectangle<float> getSettingsButtonBounds() const;
    juce::Rectangle<float> getSettingsPanelBounds() const;
    juce::Rectangle<float> getSettingsVolumeTrackBounds() const;
    juce::Rectangle<float> getSettingsVolDownBtnBounds() const;
    juce::Rectangle<float> getSettingsVol100BtnBounds() const;
    juce::Rectangle<float> getSettingsVol150BtnBounds() const;
    juce::Rectangle<float> getSettingsVolUpBtnBounds() const;

    juce::Rectangle<float> getIntegratedHeaderBounds() const;
    juce::Rectangle<float> getIntegratedBodyBounds() const;
    juce::Rectangle<float> getDrumSwitchBounds() const;
    juce::Rectangle<float> getMicSwitchBounds() const;
    juce::Rectangle<float> getMicSliderBounds() const;
    juce::Rectangle<float> getCameraSwitchBounds() const;
    juce::Rectangle<float> getCameraSettingsButtonBounds() const;
    juce::Rectangle<float> getCameraSliderBounds() const;
    juce::Rectangle<float> getCameraModePillBounds(int modeIdx) const;
    juce::Rectangle<float> getCameraMirrorCheckboxBounds() const;

    juce::Rectangle<float> getInstrumentsHeaderBounds() const;
    juce::Rectangle<float> getInstrumentsBodyBounds() const;
    juce::Rectangle<float> getEffectsHeaderBounds() const;
    juce::Rectangle<float> getEffectsBodyBounds() const;
    juce::Rectangle<float> getSlotBounds(int slotIdx) const;
    juce::Rectangle<float> getDockTabBounds(int tabIdx) const;
    juce::Rectangle<float> getPolaroidCardBounds() const;

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
