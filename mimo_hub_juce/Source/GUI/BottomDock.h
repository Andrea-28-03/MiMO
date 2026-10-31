#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"

namespace mimo
{

class MainComponent;

class BottomDock
{
public:
    static void paintBottomDockAndSlots(juce::Graphics& g, MainComponent& comp);

    static bool handleDockMouseDown(MainComponent& comp, const juce::Point<float>& pt);

    static juce::Rectangle<float> getDockTabBounds(int tabIdx, int compWidth, int compHeight);
    static juce::Rectangle<float> getSlotBounds(int slotIdx, int compWidth, int compHeight, ActiveTab activeTab);
    static juce::Rectangle<float> getLooperBounds(int compWidth, int compHeight, ActiveTab activeTab);
};

} // namespace mimo
