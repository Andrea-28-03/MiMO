#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mimo
{

class MainComponent;

class CanvasAnimation
{
public:
    static void updateTimerTick(MainComponent& comp, bool& needsRepaint);
    static void updateModulations(MainComponent& comp, bool& needsRepaint);
};

} // namespace mimo
