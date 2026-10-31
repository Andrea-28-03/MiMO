#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#ifndef JUCE_USE_CAMERA
 #define JUCE_USE_CAMERA 1
#endif
#include <juce_video/juce_video.h>
#include <atomic>
#include <memory>

namespace mimo
{

class MainComponent;

enum class HwState
{
    Off = 0,
    Starting = 1,
    Running = 2,
    Error = 3
};

enum class CameraTrackingMode
{
    Motion,
    Light,
    Presence
};

struct VideoSourceItem
{
    juce::String label;
    bool isSmartphone = false;
    int pcDeviceIdx = -1;
};

int scoreCameraDeviceName(const juce::String& name);
int scoreMicDeviceName(const juce::String& name);
bool isSkinPixel(int r, int g, int b);

// =============================================================================
// MicWorker: gestisce microfono WASAPI e sintesi su thread dedicato (0 ms blocco UI)
// =============================================================================
class MicWorker : public juce::Thread,
                  private juce::AudioIODeviceCallback
{
public:
    explicit MicWorker(MainComponent& o);
    ~MicWorker() override;

    void setWanted(bool shouldRun);
    void requestReopen();
    void run() override;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* dev) override;
    void audioDeviceStopped() override;

    void openDevices();
    void closeDevices();

    MainComponent& owner;
    std::unique_ptr<juce::AudioIODeviceType> wasapiType;
    std::unique_ptr<juce::AudioIODevice> inDevice;
    std::unique_ptr<juce::AudioIODevice> outDevice;
    std::atomic<bool> wanted { false };
    std::atomic<int> reopenGen { 0 };
};

// =============================================================================
// CameraWorker: gestisce webcam laptop (DirectShow/MediaFoundation) su thread dedicato
// =============================================================================
class CameraWorker : public juce::Thread
{
public:
    explicit CameraWorker(MainComponent& o);
    ~CameraWorker() override;

    void setWanted(bool shouldRun);
    void requestReopen();
    void run() override;

private:
    bool openDevice();
    void generateSimulatedOpticalFrame(float phase);
    void closeDevice();

    MainComponent& owner;
    std::unique_ptr<juce::CameraDevice> device;
    std::atomic<bool> wanted { false };
    std::atomic<int> reopenGen { 0 };
};

} // namespace mimo
