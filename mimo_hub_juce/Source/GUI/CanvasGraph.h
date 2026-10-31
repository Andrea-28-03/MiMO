#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MiMOLookAndFeel.h"
#include <vector>
#include <cmath>

namespace mimo
{

// =============================================================================
// Enums del Canvas e Workspace
// =============================================================================
enum class NodeType
{
    RockPiCentral,
    HardwareModule,
    NestedEffect,
    InstrumentNode,
    MasterOut
};

enum class ActiveTab
{
    Studio,
    Live,
    Community
};

enum class LooperState
{
    Idle,
    Recording,
    Playing,
    Overdubbing
};

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

// =============================================================================
// Strutture Dati del Grafo a Nodi (STUDIO & Nodi Nested)
// =============================================================================
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

struct CanvasNode
{
    juce::String id;
    NodeType type = NodeType::HardwareModule;
    juce::String label;
    juce::String subtitle;
    juce::String portBadge;
    juce::Point<float> pos;         // Coordinate correnti nel Canvas Infinito
    juce::Point<float> targetPos;   // Coordinate target per l'allargamento fluido
    bool isAnimatingPos = false;
    juce::Point<float> size;
    juce::Colour crayonColour = juce::Colours::transparentBlack;
    bool hasInstrument = false;
    juce::String instrumentName;
    juce::String instrumentEngine;
    std::vector<ModOutputDef> modOutputs;
    std::vector<NodeOutputGroup> outputGroups; // Gruppi a tendina espandibili / collassabili
    std::vector<InnerFaustBlock> innerBlocks;
    std::vector<KnobParam> directParams;       // Usato da MasterOut e nodi strumento
    bool isVerticalFlow = false; // Se true: audio:in dal basso, audio:out dall'alto
    bool isIntegratedMic = false;
    bool isIntegratedCamera = false;
    bool isBypassed = false;
    float peakLevel = 0.0f;
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

// =============================================================================
// Catalogo Sidebar Destra (Strumenti ed Effetti)
// =============================================================================
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

// =============================================================================
// Sistema Polaroid & Workspace LIVE
// =============================================================================
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

// =============================================================================
// Hit-Test per Pin e Manopole
// =============================================================================
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

struct KnobHit
{
    juce::String nodeId;
    juce::String blockId;
    juce::String paramKey;
    juce::Rectangle<float> worldBounds;
};

// =============================================================================
// Metodi di Calcolo Geometrico e Topologico del Canvas
// =============================================================================
struct CanvasGeometry
{
    static juce::Point<float> screenToWorld(juce::Point<float> screenPt, juce::Point<float> offset, float zoom) noexcept;
    static juce::Point<float> worldToScreen(juce::Point<float> worldPt, juce::Point<float> offset, float zoom) noexcept;
    static juce::Rectangle<float> worldToScreenRect(juce::Rectangle<float> worldRect, juce::Point<float> offset, float zoom) noexcept;

    static float getNodeCalculatedHeight(const CanvasNode& node);
    static std::vector<PinHit> collectAllPins(const std::vector<CanvasNode>& nodes);
    static juce::Point<float> getPinWorldPos(const std::vector<CanvasNode>& nodes, const juce::String& nodeId, const juce::String& pinId);
    static juce::Point<float> getPinNormal(const std::vector<CanvasNode>& nodes, const juce::String& nodeId, const juce::String& pinId);
    static std::vector<KnobHit> collectCanvasKnobs(const std::vector<CanvasNode>& nodes);

    static juce::String findAudioCableNear(const std::vector<CanvasCable>& cables, const std::vector<CanvasNode>& nodes,
                                           juce::Point<float> worldPt, float maxDist = 20.0f);
    static juce::String findHwModuleAt(const std::vector<CanvasNode>& nodes, juce::Point<float> worldPt);
};

// =============================================================================
// Helper di Creazione Nodi dal Catalogo
// =============================================================================
struct CanvasGraphHelper
{
    static CanvasNode createFxNodeFromCatalog(const CatalogItem& fx, juce::Point<float> worldPos, int uniqueSeed, bool isVertical = false);
    static CanvasNode createInstrumentNodeFromCatalog(const CatalogItem& item, juce::Point<float> worldPos, int uniqueSeed = 1);

    static inline juce::Rectangle<float> getBypassButtonBounds(const juce::Rectangle<float>& box) noexcept
    {
        return { box.getRight() - 50.0f, box.getY() + 8.0f, 20.0f, 18.0f };
    }

    static inline juce::Rectangle<float> getVuMeterBounds(const juce::Rectangle<float>& box) noexcept
    {
        return { box.getX() + 14.0f, box.getBottom() - 10.0f, box.getWidth() - 28.0f, 4.0f };
    }
};

} // namespace mimo
