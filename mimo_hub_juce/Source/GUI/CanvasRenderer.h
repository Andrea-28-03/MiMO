#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"

namespace mimo
{

class MainComponent;

class CanvasRenderer
{
public:
    static void paintStudioCanvas(juce::Graphics& g, MainComponent& comp);
    static void paintCables(juce::Graphics& g, MainComponent& comp, const std::vector<PinHit>& allPins);
    static void paintLiveCable(juce::Graphics& g, MainComponent& comp);
    static void paintPins(juce::Graphics& g, MainComponent& comp, const std::vector<PinHit>& allPins);
    static void paintCanvasDropdowns(juce::Graphics& g, MainComponent& comp);
    static void paintNestedModal(juce::Graphics& g, MainComponent& comp, const CanvasNode& node);
    static void paintMarqueeSelection(juce::Graphics& g, MainComponent& comp);

    static bool handleNestedModalMouseDown(MainComponent& comp, const juce::Point<float>& pt);

    // Rendering specializzato dei nodi (CanvasNodeRenderer.cpp)
    static void paintCanvasNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node);
    static void paintRockPiNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node, const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow);
    static void paintHardwareNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node, const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow);
    static void paintNestedEffectNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node, const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow);
    static void paintInstrumentNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node, const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow);
    static void paintMasterOutNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node, const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow);
};

} // namespace mimo
