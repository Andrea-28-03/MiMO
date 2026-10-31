#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mimo
{

class MainComponent;

class CanvasInteraction
{
public:
    static void handleMouseMove(MainComponent& comp, const juce::MouseEvent& e);
    static void handleMouseDown(MainComponent& comp, const juce::MouseEvent& e);
    static void handleMouseDrag(MainComponent& comp, const juce::MouseEvent& e);
    static void handleMouseUp(MainComponent& comp, const juce::MouseEvent& e);
    static void handleMouseDoubleClick(MainComponent& comp, const juce::MouseEvent& e);
    static void handleMouseWheelMove(MainComponent& comp, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel);
    static void handleMouseMagnify(MainComponent& comp, const juce::MouseEvent& e, float scaleFactor);
    static bool handleKeyPressed(MainComponent& comp, const juce::KeyPress& key);

    // Gestione Drag & Drop Nodi, Cavi e Manopole
    static void handleDragStart(MainComponent& comp, const juce::MouseEvent& e);
    static void handleDragging(MainComponent& comp, const juce::MouseEvent& e);
    static void handleDragEnd(MainComponent& comp, const juce::MouseEvent& e);
};

} // namespace mimo
