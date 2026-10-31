#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"

namespace mimo
{

class MainComponent;

class SidebarComponent
{
public:
    static void paintRightSidebar(juce::Graphics& g, MainComponent& comp);
    static void paintSettingsPanel(juce::Graphics& g, MainComponent& comp);

    static bool handleMouseDown(MainComponent& comp, const juce::Point<float>& pt, const juce::MouseEvent& e);
    static bool handleMouseWheel(MainComponent& comp, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel);
    static bool handleMouseDrag(MainComponent& comp, const juce::Point<float>& pt);

    // Bounds statici/dinamici
    static juce::Rectangle<float> getSidebarToggleBounds(int compWidth, int compHeight, bool sidebarOpen);
    static juce::Rectangle<float> getCameraButtonBounds(int compWidth, int compHeight, bool sidebarOpen);
    static juce::Rectangle<float> getSettingsButtonBounds(int compWidth, int compHeight, bool sidebarOpen);
    static juce::Rectangle<float> getSettingsPanelBounds(int compWidth, int compHeight);
    static juce::Rectangle<float> getSettingsVolumeTrackBounds(int compWidth, int compHeight);
    static juce::Rectangle<float> getSettingsVolDownBtnBounds(int compWidth, int compHeight);
    static juce::Rectangle<float> getSettingsVol100BtnBounds(int compWidth, int compHeight);
    static juce::Rectangle<float> getSettingsVol150BtnBounds(int compWidth, int compHeight);
    static juce::Rectangle<float> getSettingsVolUpBtnBounds(int compWidth, int compHeight);

    static juce::Rectangle<float> getIntegratedHeaderBounds(int compWidth, int compHeight, float sidebarScrollY);
    static juce::Rectangle<float> getIntegratedBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen);
    static juce::Rectangle<float> getDrumSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen);
    static juce::Rectangle<float> getMicSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen);
    static juce::Rectangle<float> getCameraSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen);
    static juce::Rectangle<float> getCameraSettingsButtonBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen);
    static juce::Rectangle<float> getCameraModePillBounds(int compWidth, int compHeight, float sidebarScrollY, int modeIdx);
    static juce::Rectangle<float> getCameraMirrorCheckboxBounds(int compWidth, int compHeight, float sidebarScrollY);

    static juce::Rectangle<float> getInstrumentsHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen);
    static juce::Rectangle<float> getInstrumentsBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, size_t instrumentCount);
    static juce::Rectangle<float> getEffectsHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount);
    static juce::Rectangle<float> getEffectsBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, size_t effectCount);
    static juce::Rectangle<float> getUtilitiesHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, bool fxOpen, size_t effectCount);
    static juce::Rectangle<float> getUtilitiesBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, bool fxOpen, size_t effectCount, size_t utilCount);

    static float getSidebarContentHeight(bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instCount, bool fxOpen, size_t fxCount, bool utilOpen, size_t utilCount);
    static float getSidebarMaxScroll(int compHeight, float contentHeight);
    static juce::Rectangle<float> getSidebarViewportBounds(int compWidth, int compHeight);
};

} // namespace mimo
