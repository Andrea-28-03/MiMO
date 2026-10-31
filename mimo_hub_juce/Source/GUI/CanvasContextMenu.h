#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mimo
{

class MainComponent;

class CanvasContextMenu
{
public:
    static void showQuickAddMenu(MainComponent& comp, juce::Point<float> screenPos);
    static void showContextMenu(MainComponent& comp, juce::Point<float> screenPos);

private:
    static juce::PopupMenu buildInstrumentsSubMenu(MainComponent& comp, juce::Point<float> worldPos);
    static juce::PopupMenu buildEffectsSubMenu(MainComponent& comp, juce::Point<float> worldPos);
    static juce::PopupMenu buildHardwareSubMenu(MainComponent& comp, juce::Point<float> worldPos);
    static juce::PopupMenu buildUtilitySubMenu(MainComponent& comp, juce::Point<float> worldPos);
};

} // namespace mimo
