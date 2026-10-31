#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_video/juce_video.h>
#include "HardwareWorkers.h"

namespace mimo
{

class MainComponent;

class HardwareManager
{
public:
    static void processWebcamImage(MainComponent& comp, const juce::Image& image);
    static void updateHardwareDeviceStates(MainComponent& comp);
    static void startRealMicrophone(MainComponent& comp);
    static void stopRealMicrophone(MainComponent& comp);
    static void cycleMicDevice(MainComponent& comp);
    static void setMicStatus(MainComponent& comp, HwState st, const juce::String& text);
    static juce::String getMicStatusText(const MainComponent& comp);
    static juce::String getMicDeviceLabel(const MainComponent& comp);

    static void startRealCamera(MainComponent& comp);
    static void stopRealCamera(MainComponent& comp);
    static void cycleCameraDevice(MainComponent& comp);
    static void setCameraStatus(MainComponent& comp, HwState st, const juce::String& text);
    static juce::String getCameraStatusText(const MainComponent& comp);
    static juce::String getCameraDeviceLabel(const MainComponent& comp);

    static std::vector<VideoSourceItem> getVideoSourceItems(const MainComponent& comp);
    static void selectVideoSource(MainComponent& comp, bool useSmartphone, int pcDeviceIdx = -1, const juce::String& pcDeviceName = {});
    static juce::StringArray getCameraDropdownOptions(MainComponent& comp);
    static juce::StringArray getMicDropdownOptions(const MainComponent& comp);
    static void syncEyeNodeOutputsWithDetailSettings(MainComponent& comp);
};

} // namespace mimo
