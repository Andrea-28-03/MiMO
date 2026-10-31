#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"

namespace mimo
{

class MainComponent;

class LiveAlbumView
{
public:
    static void paintLiveAlbumView(juce::Graphics& g, MainComponent& comp);
    static void paintCommunityView(juce::Graphics& g, MainComponent& comp);
    static void paintPolaroidOverlay(juce::Graphics& g, MainComponent& comp);

    static bool handlePolaroidMouseDown(MainComponent& comp, const juce::Point<float>& pt);
    static bool handleLiveMouseDown(MainComponent& comp, const juce::Point<float>& pt, const juce::MouseEvent& e);

    static void triggerSnapPolaroid(MainComponent& comp);
    static void commitPolaroidToAlbum(MainComponent& comp, const PolaroidItem& item);
    static void assignPolaroidToSlot(MainComponent& comp, int slotIdx, const PolaroidItem& item);
};

} // namespace mimo
