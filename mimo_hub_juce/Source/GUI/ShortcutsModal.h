#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mimo
{

class MainComponent;

class ShortcutsModal
{
public:
    static void paint(juce::Graphics& g, MainComponent& comp);
    static bool handleMouseDown(MainComponent& comp, const juce::Point<float>& pt);
    static juce::Rectangle<float> getModalBounds(int compWidth, int compHeight);
};

} // namespace mimo
