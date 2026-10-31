#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#ifndef JUCE_USE_CAMERA
 #define JUCE_USE_CAMERA 1
#endif
#include <juce_video/juce_video.h>
#include "MiMOLookAndFeel.h"
#include "../Networking/MiMOEyeServer.h"
#include "../Utils/qrcodegen.hpp"
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
    // Strutture Dati del Grafo a Nodi (STUDIO & Nodi Nested)
    // =========================================================================
    struct KnobParam
    {
        juce::String key;
        juce::String label;
        float minVal = 0.0f;
        float maxVal = 100.0f;
        float value = 50.0f;
        float defaultVal = 50.0f;
        juce::String unit;
        bool isExposed = true;

        float getNormalized() const;
        void setFromNormalized(float norm);
        juce::String getFormattedValue() const;
    };

    struct InnerFaustBlock
    {
        juce::String id;
        juce::String label;
        juce::String faustCode;
        juce::Colour colour;
        bool isLfo = false;
        juce::Point<float> pos;
        std::vector<KnobParam> params;
    };

    struct ModOutputDef
    {
        juce::String key;
        juce::String label;
        juce::Colour colour;
    };

    struct NodeOutputDef
    {
        juce::String pinId;       // es. "mod:x", "trigger:out", "trigger:pinch_idx"
        juce::String label;       // es. "Asse X (Orizz.)", "Pinch Indice"
        juce::Colour colour;      // es. crayonCyan, crayonCherry, 0xFFFF7A18
        bool isTrigger = false;   // true -> Diamond pin, false -> Tri pin
        juce::String hint;        // es. "(tira verso manopola)", "(verso TRIG strumento)"
    };

    struct NodeOutputGroup
    {
        juce::String id;          // es. "eye_palm", "eye_fingers", "eye_pinches", "eye_sec"
        juce::String name;        // es. "Palmo 3D (DX)", "Dita & Falangi (DX)"
        juce::String parentGroup; // es. "eye_dx" o "eye_sx" (vuoto se gruppo radice)
        bool isExpanded = false;  // Aperto o Chiuso a tendina
        bool isEnabled = true;    // Abilitato o Disabilitato (Interruttore ON/OFF)
        std::vector<NodeOutputDef> items;
    };

    enum class NodeType
    {
        RockPiCentral,
        HardwareModule,
        NestedEffect,
        InstrumentNode,
        MasterOut
    };

    struct CanvasNode
    {
        juce::String id;
        NodeType type = NodeType::HardwareModule;
        juce::String label;
        juce::String subtitle;
        juce::String portBadge;
        juce::Point<float> pos; // Coordinate correnti nel Canvas Infinito
        juce::Point<float> targetPos; // Coordinate target per l'allargamento fluido
        bool isAnimatingPos = false;
        juce::Point<float> size;
        juce::Colour crayonColour = juce::Colours::transparentBlack;
        bool hasInstrument = false;
        juce::String instrumentName;
        juce::String instrumentEngine;
        std::vector<ModOutputDef> modOutputs;
        std::vector<NodeOutputGroup> outputGroups; // Gruppi a tendina espandibili / collassabili
        std::vector<InnerFaustBlock> innerBlocks;
        std::vector<KnobParam> directParams; // Usato da MasterOut
        bool isVerticalFlow = false; // Se true: audio:in dal basso, audio:out dall'alto (flusso verticale modulo -> MIMO)
        bool isIntegratedMic = false;
        bool isIntegratedCamera = false;
    };

    struct CanvasCable
    {
        juce::String id;
        juce::String sourceNodeId;
        juce::String sourcePinId;
        juce::String targetNodeId;
        juce::String targetPinId;
        bool isModulation = false;
        juce::Colour colour = MiMOLookAndFeel::fountainInk;
        std::vector<juce::Point<float>> freehandNorm;
        bool isTrigger = false;
    };

    struct GraphSnapshot
    {
        std::vector<CanvasNode> nodes;
        std::vector<CanvasCable> cables;
    };

    // =========================================================================
    // Catalogo Sidebar Destra (Strumenti ed Effetti)
    // =========================================================================
    struct CatalogItem
    {
        juce::String id;
        juce::String category;
        juce::String name;
        juce::String engine;
        juce::String hint;
        juce::Colour colour;
        bool isEffect = false;
        bool isIntegrated = false;
        std::vector<InnerFaustBlock> templateBlocks;
    };

    // =========================================================================
    // Sistema "Polaroid" & Workspace LIVE
    // =========================================================================
    struct PolaroidItem
    {
        juce::String id;
        juce::String autoName;
        juce::String title;
        juce::String timestamp;
        juce::String engineType = "hybrid"; // "sfizz", "faust", "hybrid"
        int bpm = 112;
        float rotationDeg = 0.0f;
        std::vector<std::pair<juce::String, juce::Colour>> badges;
        int nodeCount = 0;
        int cableCount = 0;
        int modCount = 0;
        juce::Image previewImage;
        GraphSnapshot snapshot;
        bool isValid = false;
    };

    // =========================================================================
    // Stato Attivo dell'Applicazione
    // =========================================================================
    enum class ActiveTab { Studio, Live, Community };
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
    enum class CameraTrackingMode { Motion, Light, Presence };
    CameraTrackingMode cameraTrackingMode = CameraTrackingMode::Motion;

    // Dispositivi Hardware Reali (Microfono laptop & Webcam laptop)
    enum class HwState { Off = 0, Starting = 1, Running = 2, Error = 3 };

    // --- Microfono + Sintesi Audio (gestiti su thread dedicato MicWorker: 0 ms blocco UI) ---
    class MicWorker;
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
    class CameraWorker;
    std::unique_ptr<CameraWorker> cameraWorker;
    std::atomic<int> cameraState { 0 };            // HwState (scritto dal worker)
    juce::CriticalSection cameraInfoLock;
    juce::String cameraStatusText;
    juce::String cameraDeviceName;
    juce::StringArray cameraDeviceList;
    std::atomic<int> cameraDeviceIndex { -1 };     // -1 = selezione automatica webcam fisica (es. HP Wide Vision)
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
    int eyeDrawerTab = 0; // 0 = Smartphone MediaPipe AI (60 FPS), 1 = Webcam PC Fallback
    bool camDropdownOpen = false;
    bool micDropdownOpen = false;
    bool useSmartphoneEye = false;
    bool eyeShowVideo = true; // true = Mostra Video + Scheletro, false = Solo Scheletro Vettoriale (Ultra-Leggero)
    mimo::HandTrackingState latestEyeState;
    mimo::HandTrackingState smoothedHandState;
    bool hasSmoothedHandState = false;
    mimo::HandTrackingState analyzeHandFrame(const juce::Image& frame);

    // Impostazioni Dettaglio Analisi Mano su PC (Mano, Numero Falangi, Gesture)
    int eyeHandFilter = 0;           // 0 = Entrambe, 1 = Mano Destra, 2 = Mano Sinistra
    int eyePhalanxDetail = 2;        // 0 = Palmo (1 pt), 1 = 5 Punte Dita (6 pt), 2 = 21 Falangi Complete
    bool eyeGesturePos3D = true;     // Assi X / Y / Profondita Z
    bool eyeGestureFistTilt = true;  // Chiusura Pugno + Rotazione Mano
    bool eyeGestureFingerFlex = false; // Flessione 4 Falangi Singole (Indice, Medio, Anulare, Mignolo)
    bool eyeGesturePinchTrig = true; // Trigger Percussivi Pinch (Pollice + 4 Dita)

    void syncEyeNodeOutputsWithDetailSettings();

    struct VideoSourceItem
    {
        juce::String label;
        bool isSmartphone = false;
        int pcDeviceIdx = -1;
    };
    std::vector<VideoSourceItem> getVideoSourceItems() const;
    void selectVideoSource(bool useSmartphone, int pcDeviceIdx = -1, const juce::String& pcDeviceName = {});

    juce::StringArray getCameraDropdownOptions();
    juce::StringArray getMicDropdownOptions();
    void paintCanvasDropdowns(juce::Graphics& g);
    void drawModernFlatEyeCard(juce::Graphics& g, const CanvasNode& node);
    void drawModernFlatEyeModal(juce::Graphics& g);
    void drawHandSkeleton(juce::Graphics& g, juce::Rectangle<float> bounds, const mimo::HandTrackingState& state, bool overVideo = false);
    static void drawQrCode(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds);

    juce::Rectangle<float> getEyeModalBounds() const;
    juce::Rectangle<float> getEyeModalCloseBounds() const;
    juce::Rectangle<float> getEyeModalTabBounds(int tabIdx) const;
    juce::Rectangle<float> getEyeModalUrlButtonBounds() const;
    juce::Rectangle<float> getEyeModalActivatePhoneButtonBounds() const;
    juce::Rectangle<float> getEyeModalPinchTestBounds(int pinchIdx) const;
    juce::Rectangle<float> getEyeModalHandFilterBounds(int filterIdx) const;
    juce::Rectangle<float> getEyeModalPhalanxBounds(int detailIdx) const;
    juce::Rectangle<float> getEyeModalGestureBounds(int gestureIdx) const;
    juce::Rectangle<float> getEyeModalCameraOptionBounds(int optIdx) const;
    juce::Rectangle<float> getEyeModalViewToggleBounds() const;
    static juce::Rectangle<float> getEyeCardConfigButtonBounds(const juce::Rectangle<float>& nodeBox);
    static juce::Rectangle<float> getEyeCardViewToggleButtonBounds(const juce::Rectangle<float>& nodeBox);

    // Editor Nodi Nested a tutto schermo (Doppio Clic) e Selezione Nodi
    juce::String expandedNodeId;
    juce::String selectedNodeId;

    // Sistema Polaroid, Album e 5 Slot Scena + 1-Button Looper
    bool hasPendingPolaroid = false;
    PolaroidItem pendingPolaroid;
    bool polaroidHasTyped = false;
    int polaroidCountdownTicks = 18; // ~3.0 secondi a 6 Hz
    std::vector<PolaroidItem> photoAlbum;
    PolaroidItem sceneSlots[5];
    int activeSceneSlot = 0;
    int editingSlotIndex = -1;

    // Stato Workspace LIVE (Performance & Feedback I2C)
    int selectedAlbumIndex = -1;
    juce::String filterEngine = "ALL"; // "ALL", "sfizz", "faust"
    int hoveredSlotIndex = -1;
    int holdSlotIndex = -1;
    float holdProgress = 0.0f; // 0.0f .. 1.0f (assegnazione rapida 1s)
    int snappedSlotIndex = -1;
    float snapAnimScale = 1.0f; // Animazione elastica spring al drop
    float looperSweepProgress = 0.0f; // Sweep rotativo 0..1 a 60 FPS
    double looperStartTime = 0.0;
    double looperDurationSec = 4.0;

    enum class LooperState { Idle, Recording, Playing, Overdubbing };
    LooperState looperState = LooperState::Idle;

    void initDefaultLivePresets();
    void toggleLooper();
    void clearLooper();
    void triggerSceneSlot(int slotIdx);
    juce::Rectangle<float> getLooperBounds() const;

    // =========================================================================
    // Stato di Interazione Mouse / Drag & Drop
    // =========================================================================
    enum class DragMode
    {
        None,
        PanCanvas,
        MoveNode,
        MoveInnerBlock,
        TurnKnob,
        TurnInnerKnob,
        DrawCable,
        DragSidebarItem,
        DragPolaroid,
        DragMicSensitivity,
        DragCanvasMicSensitivity,
        DragCameraSensitivity,
        DragSettingsVolume
    };
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

    // Selezione Cavi e Pin per cancellazione rapida ([CANC] / [Backspace])
    juce::String selectedCableId;
    juce::String selectedPinNodeId;
    juce::String selectedPinId;

    // Distacco e re-routing cavi ("prendendo quel nodo e lasciandolo nel nulla si stacchi")
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
    CanvasNode createFxNodeFromCatalog(const CatalogItem& fx, juce::Point<float> worldPos, int uniqueSeed, bool isVertical = false);
    CanvasNode createInstrumentNodeFromCatalog(const CatalogItem& item, juce::Point<float> worldPos, int uniqueSeed = 1);
    void syncAudioRoutes();

    juce::Point<float> screenToWorld(juce::Point<float> screenPt) const;
    juce::Point<float> worldToScreen(juce::Point<float> worldPt) const;
    juce::Rectangle<float> worldToScreenRect(juce::Rectangle<float> worldRect) const;

    // Calcolo posizioni Pin ed Elementi nel Canvas
    struct PinHit
    {
        juce::String nodeId;
        juce::String pinId;
        bool isOutput = false;
        bool isMod = false;
        juce::Colour colour = MiMOLookAndFeel::fountainInk;
        juce::Point<float> worldPos;
        juce::Point<float> normal { 0.0f, 0.0f }; // Direzione di uscita o ingresso presa
        MiMOLookAndFeel::PinShapeType shapeType = MiMOLookAndFeel::PinShapeType::AudioCircle;
    };
    float getNodeCalculatedHeight(const CanvasNode& node) const;
    std::vector<PinHit> collectAllPins() const;
    juce::Point<float> getPinWorldPos(const juce::String& nodeId, const juce::String& pinId) const;
    juce::Point<float> getPinNormal(const juce::String& nodeId, const juce::String& pinId) const;

    struct KnobHit
    {
        juce::String nodeId;
        juce::String blockId;
        juce::String paramKey;
        juce::Rectangle<float> worldBounds;
    };
    std::vector<KnobHit> collectCanvasKnobs() const;

    // Hit-test per Drop Effetti ("Snap-on-Wire") e Moduli
    juce::String findAudioCableNear(juce::Point<float> worldPt, float maxDist = 20.0f) const;
    juce::String findHwModuleAt(juce::Point<float> worldPt) const;
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

    // Gestione Modalità Notte (Dark Mode di default & sincronizzata con il sistema)
    bool isDarkMode = true;
    static bool detectSystemDarkMode();
    void setDarkMode(bool dark);
    void toggleDarkMode();

    // Contatore progressivo per istanze di nodi ed effetti
    int fxCounter = 2;

    // Immagine Hardware MiMO Centrale
    juce::Image mimoImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace mimo
