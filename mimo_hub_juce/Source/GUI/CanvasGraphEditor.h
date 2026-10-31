#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"

namespace mimo
{

class MainComponent;

class CanvasGraphEditor
{
public:
    static void insertEffectOnCable(MainComponent& comp, const CatalogItem& fx, const juce::String& cableId, juce::Point<float> worldPos);
    static void deleteNode(MainComponent& comp, const juce::String& nodeId);
    static void deleteSelectedEffect(MainComponent& comp);

    static void toggleDrumInCanvas(MainComponent& comp);
    static void toggleMicInCanvas(MainComponent& comp);
    static void toggleCameraInCanvas(MainComponent& comp);
};

} // namespace mimo
