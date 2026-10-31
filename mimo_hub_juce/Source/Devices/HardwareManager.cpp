#include "HardwareManager.h"
#include "../GUI/MainComponent.h"
#include "../GUI/MiMOEyeDrawer.h"
#include <algorithm>
#include <cmath>

namespace mimo
{

void HardwareManager::processWebcamImage(MainComponent& comp, const juce::Image& image)
{
    if (!image.isValid() || comp.useSmartphoneEye)
        return;

    const int srcW = image.getWidth();
    const int srcH = image.getHeight();
    const int targetW = 320;
    const int targetH = (srcW > 0) ? std::max(120, (320 * srcH) / srcW) : 240;

    juce::Image analysisFrame = image.rescaled(targetW, targetH, juce::Graphics::mediumResamplingQuality);
    if (comp.cameraMirror)
    {
        juce::Image mirrored(juce::Image::RGB, analysisFrame.getWidth(), analysisFrame.getHeight(), false);
        juce::Graphics mg(mirrored);
        mg.drawImageTransformed(analysisFrame, juce::AffineTransform::scale(-1.0f, 1.0f).translated(static_cast<float>(analysisFrame.getWidth()), 0.0f));
        analysisFrame = mirrored;
    }

    juce::Image small = analysisFrame.rescaled(targetW / 2, targetH / 2, juce::Graphics::lowResamplingQuality);

    float totalBrightness = 0.0f;
    float motionDiff = 0.0f;
    float weightedX = 0.0f;
    float weightedY = 0.0f;
    float motionWeightTotal = 0.0f;

    const int w = small.getWidth();
    const int h = small.getHeight();
    const float invW = 1.0f / static_cast<float>(std::max(1, w));
    const float invH = 1.0f / static_cast<float>(std::max(1, h));

    juce::Image::BitmapData currentBmp(small, juce::Image::BitmapData::readOnly);

    {
        juce::ScopedLock sl(comp.cameraLock);
        const bool hasPrev = (comp.prevWebcamImage.isValid() && comp.prevWebcamImage.getWidth() == w && comp.prevWebcamImage.getHeight() == h);
        std::unique_ptr<juce::Image::BitmapData> prevBmpPtr;
        if (hasPrev)
            prevBmpPtr = std::make_unique<juce::Image::BitmapData>(comp.prevWebcamImage, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < h; y += 2)
        {
            for (int x = 0; x < w; x += 2)
            {
                const auto cCurr = currentBmp.getPixelColour(x, y);
                const float bCurr = cCurr.getBrightness();
                totalBrightness += bCurr;

                if (prevBmpPtr != nullptr)
                {
                    const auto cPrev = prevBmpPtr->getPixelColour(x, y);
                    const float d = std::abs(bCurr - cPrev.getBrightness());
                    if (d > 0.045f)
                    {
                        motionDiff += d;
                        weightedX += static_cast<float>(x) * d;
                        weightedY += static_cast<float>(y) * d;
                        motionWeightTotal += d;
                    }
                }
            }
        }

        const float sampleCount = static_cast<float>((w / 2) * (h / 2));
        comp.realCameraBrightness.store(sampleCount > 0.0f ? (totalBrightness / sampleCount) : 0.0f);
        const float normMotion = juce::jlimit(0.0f, 1.0f, (sampleCount > 0.0f ? (motionDiff / sampleCount) : 0.0f) * 12.0f * comp.cameraSensitivity);
        comp.realCameraMotion.store(normMotion);

        if (motionWeightTotal > 0.01f)
        {
            const float cx = (weightedX / motionWeightTotal) * invW;
            const float cy = (weightedY / motionWeightTotal) * invH;
            comp.realCameraCentroidX.store(cx * 2.0f - 1.0f);
            comp.realCameraCentroidY.store(cy * 2.0f - 1.0f);
        }

        comp.prevWebcamImage = small.createCopy();
        comp.latestWebcamImage = analysisFrame;
        comp.cameraFrameCounter.fetch_add(1);
        comp.hasNewWebcamFrame.store(true);
    }

    const auto handSt = comp.analyzeHandFrame(analysisFrame);
    const bool mpActive = (comp.eyeServer != nullptr && comp.eyeServer->isMediaPipeActive());

    if (!mpActive)
    {
        if (handSt.hasHand)
        {
            comp.latestEyeState = handSt;
            if (comp.eyeServer != nullptr)
                comp.eyeServer->updateFromLocalWebcam(handSt);
        }
        else
        {
            comp.latestEyeState.hasHand = false;
        }
    }

    if (comp.eyeServer != nullptr)
        comp.eyeServer->pushVideoFrameForAnalysis(analysisFrame, false);
}

void HardwareManager::updateHardwareDeviceStates(MainComponent& comp)
{
    const bool micInCanvas = comp.isMicInCanvas();
    const bool cameraInCanvas = comp.isCameraInCanvas();

    if (micInCanvas && !comp.isMicRecording)
        startRealMicrophone(comp);
    else if (!micInCanvas && comp.isMicRecording)
        stopRealMicrophone(comp);

    if (cameraInCanvas && !comp.isCameraRecording)
        startRealCamera(comp);
    else if (!cameraInCanvas && comp.isCameraRecording)
        stopRealCamera(comp);
}

void HardwareManager::startRealMicrophone(MainComponent& comp)
{
    comp.isMicRecording = true;
    setMicStatus(comp, HwState::Starting, "Avvio microfono...");
    if (comp.micWorker != nullptr)
        comp.micWorker->setWanted(true);
}

void HardwareManager::stopRealMicrophone(MainComponent& comp)
{
    comp.isMicRecording = false;
    if (comp.micWorker != nullptr)
        comp.micWorker->setWanted(false);
}

void HardwareManager::cycleMicDevice(MainComponent& comp)
{
    juce::StringArray list;
    {
        const juce::ScopedLock sl(comp.micInfoLock);
        list = comp.micDeviceList;
    }
    if (list.isEmpty())
    {
        if (comp.micWorker != nullptr && comp.isMicRecording)
            comp.micWorker->requestReopen();
        return;
    }

    const int nextIdx = (std::max(0, comp.micDeviceIndex.load()) + 1) % list.size();
    comp.micDeviceIndex.store(nextIdx);
    {
        const juce::ScopedLock sl(comp.micInfoLock);
        comp.micDeviceName = list[nextIdx];
    }
    if (comp.micWorker != nullptr && comp.isMicRecording)
        comp.micWorker->requestReopen();
    comp.repaint();
}

void HardwareManager::setMicStatus(MainComponent& comp, HwState st, const juce::String& text)
{
    {
        const juce::ScopedLock sl(comp.micInfoLock);
        comp.micStatusText = text;
    }
    comp.micStateAtomic.store(static_cast<int>(st));
}

juce::String HardwareManager::getMicStatusText(const MainComponent& comp)
{
    const juce::ScopedLock sl(comp.micInfoLock);
    return comp.micStatusText.isNotEmpty() ? comp.micStatusText : juce::String("In ascolto");
}

juce::String HardwareManager::getMicDeviceLabel(const MainComponent& comp)
{
    const juce::ScopedLock sl(comp.micInfoLock);
    if (comp.micDeviceName.isNotEmpty())
        return comp.micDeviceName;
    if (comp.micDeviceList.size() > 0)
        return comp.micDeviceList[juce::jlimit(0, comp.micDeviceList.size() - 1, std::max(0, comp.micDeviceIndex.load()))];
    return "Microfono predefinito";
}

void HardwareManager::startRealCamera(MainComponent& comp)
{
    comp.isCameraRecording = true;
    if (comp.useSmartphoneEye)
    {
        if (comp.eyeServer != nullptr)
            comp.eyeServer->setSmartphoneExclusive(true);
        if (comp.cameraWorker != nullptr)
            comp.cameraWorker->setWanted(false);
        const bool isStreaming = (comp.eyeServer != nullptr && comp.eyeServer->isSmartphoneStreaming());
        setCameraStatus(comp, HwState::Running, isStreaming ? "Smartphone Connesso (Wi-Fi)" : "Smartphone in attesa stream...");
    }
    else
    {
        if (comp.eyeServer != nullptr)
            comp.eyeServer->setSmartphoneExclusive(false);
        const juce::String label = getCameraDeviceLabel(comp);
        setCameraStatus(comp, HwState::Starting, "Avvio webcam: " + label);
        if (comp.cameraWorker != nullptr)
        {
            comp.cameraWorker->setWanted(true);
            comp.cameraWorker->requestReopen();
        }
    }
}

void HardwareManager::stopRealCamera(MainComponent& comp)
{
    comp.isCameraRecording = false;
    if (comp.cameraWorker != nullptr)
        comp.cameraWorker->setWanted(false);
}

void HardwareManager::cycleCameraDevice(MainComponent& comp)
{
    juce::StringArray list;
    {
        const juce::ScopedLock sl(comp.cameraInfoLock);
        list = comp.cameraDeviceList;
    }
    if (list.size() <= 1)
    {
        selectVideoSource(comp, false, 0, list.size() > 0 ? list[0] : juce::String());
        return;
    }

    const int nextIdx = (std::max(0, comp.cameraDeviceIndex.load()) + 1) % list.size();
    selectVideoSource(comp, false, nextIdx, list[nextIdx]);
}

void HardwareManager::setCameraStatus(MainComponent& comp, HwState st, const juce::String& text)
{
    {
        const juce::ScopedLock sl(comp.cameraInfoLock);
        comp.cameraStatusText = text;
    }
    comp.cameraState.store(static_cast<int>(st));
}

juce::String HardwareManager::getCameraStatusText(const MainComponent& comp)
{
    const juce::ScopedLock sl(comp.cameraInfoLock);
    return comp.cameraStatusText.isNotEmpty() ? comp.cameraStatusText : juce::String("Avvio webcam...");
}

juce::String HardwareManager::getCameraDeviceLabel(const MainComponent& comp)
{
    if (comp.useSmartphoneEye)
        return "Smartphone (Fotocamera Wi-Fi)";

    const juce::ScopedLock sl(comp.cameraInfoLock);
    juce::String name = comp.cameraDeviceName;
    if (name.isEmpty() && comp.cameraDeviceList.size() > 0)
        name = comp.cameraDeviceList[juce::jlimit(0, comp.cameraDeviceList.size() - 1, std::max(0, comp.cameraDeviceIndex.load()))];
    if (name.isEmpty())
        return "HP Wide Vision HD (PC)";

    if (name.containsIgnoreCase("camo"))
        return "Camo (Smartphone 60 FPS)";
    if (name.containsIgnoreCase("hp") || name.containsIgnoreCase("wide vision") || name.containsIgnoreCase("nvidia"))
        return "HP Wide Vision HD (PC)";
    return name;
}

std::vector<VideoSourceItem> HardwareManager::getVideoSourceItems(const MainComponent& comp)
{
    std::vector<VideoSourceItem> items;

    juce::StringArray devList = juce::CameraDevice::getAvailableDevices();
    if (devList.isEmpty())
    {
        const juce::ScopedLock sl(comp.cameraInfoLock);
        devList = comp.cameraDeviceList;
    }

    int hpIdx = -1;
    for (int i = 0; i < devList.size(); ++i)
    {
        if (scoreCameraDeviceName(devList[i]) >= 200)
        {
            hpIdx = i;
            break;
        }
    }

    const int pcDefaultIdx = (hpIdx >= 0) ? hpIdx : (devList.size() > 1 ? 1 : 0);
    items.push_back({ "HP Wide Vision HD (Webcam PC Predefinita)", false, pcDefaultIdx });

    const bool phoneStreaming = (comp.eyeServer != nullptr && comp.eyeServer->isSmartphoneStreaming());
    items.push_back({ phoneStreaming ? "Smartphone (Web App Wi-Fi - Connesso)" : "Smartphone (Web App Fotocamera Wi-Fi)", true, -1 });

    int camoIdx = -1;
    for (int i = 0; i < devList.size(); ++i)
    {
        if (devList[i].containsIgnoreCase("camo"))
        {
            camoIdx = i;
            break;
        }
    }
    if (camoIdx >= 0 && camoIdx != pcDefaultIdx)
        items.push_back({ "Camo (Smartphone USB/Wi-Fi 60 FPS)", false, camoIdx });

    for (int i = 0; i < devList.size(); ++i)
    {
        if (i == pcDefaultIdx || i == camoIdx)
            continue;
        items.push_back({ devList[i], false, i });
    }

    if (items.empty())
    {
        items.push_back({ "Webcam PC (Sensore Locale)", false, 0 });
        items.push_back({ "Smartphone (Web App Fotocamera Wi-Fi)", true, -1 });
    }

    return items;
}

void HardwareManager::selectVideoSource(MainComponent& comp, bool useSmartphone, int pcDeviceIdx, const juce::String& pcDeviceName)
{
    comp.useSmartphoneEye = useSmartphone;

    if (comp.eyeServer != nullptr)
        comp.eyeServer->setSmartphoneExclusive(useSmartphone);

    {
        const juce::ScopedLock sl(comp.cameraLock);
        comp.latestWebcamImage = juce::Image();
        comp.prevWebcamImage = juce::Image();
        comp.hasNewWebcamFrame.store(false);
    }

    comp.smoothedHandState = mimo::HandTrackingState{};
    comp.latestEyeState = mimo::HandTrackingState{};
    comp.hasSmoothedHandState = false;

    if (useSmartphone)
    {
        if (comp.cameraWorker != nullptr)
            comp.cameraWorker->setWanted(false);

        const bool isStreaming = (comp.eyeServer != nullptr && comp.eyeServer->isSmartphoneStreaming());
        setCameraStatus(comp, HwState::Running, isStreaming ? "Smartphone Connesso (Wi-Fi)" : "Smartphone in attesa stream...");
    }
    else
    {
        if (pcDeviceIdx >= 0)
            comp.cameraDeviceIndex.store(pcDeviceIdx);

        if (pcDeviceName.isNotEmpty())
        {
            const juce::ScopedLock sl(comp.cameraInfoLock);
            comp.cameraDeviceName = pcDeviceName;
        }

        comp.isCameraRecording = true;
        if (comp.cameraWorker != nullptr)
        {
            comp.cameraWorker->setWanted(true);
            comp.cameraWorker->requestReopen();
        }

        const juce::String label = getCameraDeviceLabel(comp);
        setCameraStatus(comp, HwState::Starting, "Avvio webcam: " + label);
    }

    comp.repaint();
}

juce::StringArray HardwareManager::getCameraDropdownOptions(MainComponent& comp)
{
    const auto items = getVideoSourceItems(comp);
    juce::StringArray opts;
    for (const auto& it : items)
        opts.add(it.label);
    return opts;
}

juce::StringArray HardwareManager::getMicDropdownOptions(const MainComponent& comp)
{
    juce::StringArray opts;
    {
        const juce::ScopedLock sl(comp.micInfoLock);
        opts = comp.micDeviceList;
    }
    if (opts.isEmpty())
        opts.add("Microfono Integrato PC");
    return opts;
}

void HardwareManager::syncEyeNodeOutputsWithDetailSettings(MainComponent& comp)
{
    if (comp.eyeServer != nullptr)
    {
        comp.eyeServer->setHandFilter(0);
        comp.eyeServer->setPinchTriggerEnabled(true);
    }

    for (auto& n : comp.nodes)
    {
        if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
        {
            MiMOEyeDrawer::syncEyeNodeOutputsWithDetailSettings(n, comp.eyePhalanxDetail, comp.eyeHandFilter,
                                                               comp.eyeGesturePos3D, comp.eyeGestureFistTilt,
                                                               comp.eyeGestureFingerFlex, comp.eyeGesturePinchTrig);
        }
    }
}

} // namespace mimo
