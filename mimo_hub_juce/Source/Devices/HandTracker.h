#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../Networking/MiMOEyeServer.h"

namespace mimo
{

class MainComponent;

class HandTracker
{
public:
    static HandTrackingState analyzeHandFrame(MainComponent& comp, const juce::Image& frame);
};

} // namespace mimo
