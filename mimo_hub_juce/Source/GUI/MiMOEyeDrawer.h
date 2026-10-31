#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"
#include "../Networking/MiMOEyeServer.h"

namespace mimo
{

class MainComponent;

namespace MiMOEyeDrawer
{
    void drawModernFlatEyeModal(juce::Graphics& g, MainComponent& comp);
    bool handleMouseDown(MainComponent& comp, const juce::Point<float>& pt);
    void drawHandSkeleton(juce::Graphics& g, juce::Rectangle<float> bounds,
                          const mimo::HandTrackingState& state, bool overVideo,
                          bool handEnabledPrim, bool handEnabledSec,
                          bool trigsEnabledPrim, bool trigsEnabledSec);
    void drawQrCode(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds);
    void paintCanvasDropdowns(juce::Graphics& g, MainComponent& comp);

    void syncEyeNodeOutputsWithDetailSettings(CanvasNode& eyeNode,
                                             int detail, int filter,
                                             bool pos3D, bool fistTilt,
                                             bool fingerFlex, bool pinchTrig);

    juce::Rectangle<float> getEyeModalBounds(int compWidth, int compHeight);
    juce::Rectangle<float> getEyeModalCloseBounds(int compWidth, int compHeight);
    juce::Rectangle<float> getEyeModalTabBounds(int compWidth, int compHeight, int tabIdx);
    juce::Rectangle<float> getEyeModalUrlButtonBounds(int compWidth, int compHeight);
    juce::Rectangle<float> getEyeModalActivatePhoneButtonBounds(int compWidth, int compHeight);
    juce::Rectangle<float> getEyeModalHandFilterBounds(int compWidth, int compHeight, int filterIdx);
    juce::Rectangle<float> getEyeModalPhalanxBounds(int compWidth, int compHeight, int detailIdx);
    juce::Rectangle<float> getEyeModalGestureBounds(int compWidth, int compHeight, int gestureIdx);
    juce::Rectangle<float> getEyeModalPinchTestBounds(int compWidth, int compHeight, int pinchIdx);
    juce::Rectangle<float> getEyeModalCameraOptionBounds(int compWidth, int compHeight, int optIdx);
    juce::Rectangle<float> getEyeModalViewToggleBounds(int compWidth, int compHeight);
    juce::Rectangle<float> getEyeCardConfigButtonBounds(const juce::Rectangle<float>& nodeBox);
    juce::Rectangle<float> getEyeCardViewToggleButtonBounds(const juce::Rectangle<float>& nodeBox);
}

} // namespace mimo
