#include "HardwareWorkers.h"
#include "../GUI/MainComponent.h"
#include <cmath>

#if JUCE_WINDOWS
extern "C" __declspec(dllimport) long __stdcall CoInitializeEx(void* pvReserved, unsigned long dwCoInit);
extern "C" __declspec(dllimport) void __stdcall CoUninitialize();
#endif

namespace mimo
{

int scoreCameraDeviceName(const juce::String& name)
{
    const auto s = name.toLowerCase();
    // Webcam nativa del PC (HP, Wide Vision) = PRIORITÀ ASSOLUTA (500) come predefinita!
    if (s.contains("hp") || s.contains("wide vision"))
        return 500;
    if (s.contains("integrated") || s.contains("webcam") || s.contains("hd camera") || s.contains("uvc")
        || s.contains("logitech") || s.contains("front") || s.contains("internal"))
        return 350;

    // Su laptop HP con NVIDIA Broadcast attivo, NVIDIA fa da bridge per la webcam HP fisica (250)
    if (s.contains("nvidia"))
        return 250;

    // Camo: smartphone camera USB/Wi-Fi (150)
    if (s.contains("camo"))
        return 150;

    if (s.contains("droidcam") || s.contains("iriun") || s.contains("epoccam"))
        return 80;

    // Camere virtuali (OBS, ManyCam, ecc.) = priorita negativa per non selezionarle mai all'avvio!
    if (s.contains("obs") || s.contains("virtual")
        || s.contains("manycam") || s.contains("snap")
        || s.contains("vcam") || s.contains("splitcam") || s.contains("xsplit"))
        return -200;

    return 20;
}

bool isSkinPixel(int r, int g, int b)
{
    // Spazio colore YCbCr: segmentazione cromatica robusta per tutte le tonalita di pelle e condizioni di luce
    const float cb = 128.0f - 0.168736f * static_cast<float>(r) - 0.331264f * static_cast<float>(g) + 0.500000f * static_cast<float>(b);
    const float cr = 128.0f + 0.500000f * static_cast<float>(r) - 0.418688f * static_cast<float>(g) - 0.081312f * static_cast<float>(b);

    if (cb >= 75.0f && cb <= 135.0f && cr >= 128.0f && cr <= 182.0f)
        return true;

    // Fallback RGB (copre illuminazione calda, tungsteno o forte contrasto)
    if (r > 60 && g > 35 && b > 20 && r >= g && (r - b) >= 8)
        return true;

    return false;
}

int scoreMicDeviceName(const juce::String& name)
{
    const auto s = name.toLowerCase();
    if (s.contains("camo") || s.contains("nvidia") || s.contains("virtual")
        || s.contains("cable") || s.contains("stereo mix") || s.contains("missaggio")
        || s.contains("loopback") || s.contains("what u hear"))
        return -100;

    // Evita di forzare le cuffie Bluetooth in modalità telefonica Hands-Free se c'è il mic integrato
    if (s.contains("hands-free") || s.contains("ag audio"))
        return -30;

    if (s.contains("gruppo microfoni") || s.contains("microphone array")
        || s.contains("smart sound") || s.contains("intel")
        || s.contains("realtek") || s.contains("interno") || s.contains("internal")
        || s.contains("array"))
        return 100;

    return 25;
}

// =============================================================================
// MicWorker
// =============================================================================
MicWorker::MicWorker(MainComponent& o)
    : juce::Thread("MiMO Mic Worker"), owner(o)
{
}

MicWorker::~MicWorker()
{
    wanted.store(false);
    signalThreadShouldExit();
    notify();
    stopThread(4000);
}

void MicWorker::setWanted(bool shouldRun)
{
    if (shouldRun && !wanted.load())
        reopenGen.fetch_add(1);
    wanted.store(shouldRun);
    notify();
}

void MicWorker::requestReopen()
{
    reopenGen.fetch_add(1);
    notify();
}

void MicWorker::run()
{
   #if JUCE_WINDOWS
    const long hr = CoInitializeEx(nullptr, 0x2 /* COINIT_APARTMENTTHREADED */);
   #endif

    int handledGen = -1;
    int silentCheckCounter = 0;
    bool autoFallbackTried = false;

    while (!threadShouldExit())
    {
        const bool want = wanted.load();
        const int gen = reopenGen.load();

        if (!want)
        {
            if (inDevice != nullptr || outDevice != nullptr || owner.micStateAtomic.load() != static_cast<int>(HwState::Off))
            {
                closeDevices();
                owner.setMicStatus(HwState::Off, "Microfono spento");
            }
            handledGen = -1;
            silentCheckCounter = 0;
            autoFallbackTried = false;
        }
        else if (gen != handledGen)
        {
            handledGen = gen;
            silentCheckCounter = 0;
            closeDevices();
            openDevices();
        }
        else if (inDevice != nullptr && owner.micStateAtomic.load() == static_cast<int>(HwState::Running))
        {
            // Se il dispositivo scelto automaticamente produce 0.0 assoluto per ~2.5s, prova il prossimo microfono fisico
            if (owner.realMicPeakRaw.load() <= 0.000001f)
            {
                if (++silentCheckCounter == 16 && !autoFallbackTried)
                {
                    autoFallbackTried = true;
                    juce::StringArray list;
                    {
                        const juce::ScopedLock sl(owner.micInfoLock);
                        list = owner.micDeviceList;
                    }
                    if (list.size() > 1)
                    {
                        const int cur = owner.micDeviceIndex.load();
                        for (int step = 1; step < list.size(); ++step)
                        {
                            const int cand = (cur + step) % list.size();
                            if (scoreMicDeviceName(list[cand]) > -50)
                            {
                                owner.micDeviceIndex.store(cand);
                                reopenGen.fetch_add(1);
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                silentCheckCounter = 0;
            }
        }

        wait(150);
    }

    closeDevices();

   #if JUCE_WINDOWS
    if (hr >= 0)
        CoUninitialize();
   #endif
}

void MicWorker::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                 int /*numInputChannels*/,
                                                 float* const* outputChannelData,
                                                 int numOutputChannels,
                                                 int numSamples,
                                                 const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
        return;

    if (owner.synthTriggerPending.exchange(false))
    {
        owner.synthEnv = juce::jlimit(0.25f, 1.0f, owner.synthHitVelocity.load());
        owner.synthPitchEnv = 1.0f;
    }

    if (owner.synthEnv <= 0.0005f)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    const float sr = static_cast<float>(std::max(8000.0, owner.synthSampleRate));
    const int instType = owner.synthInstrumentType.load();
    const float cutoffHz = juce::jlimit(80.0f, 12000.0f, owner.synthFilterCutoffHz.load());
    const float reso = juce::jlimit(0.05f, 0.92f, owner.synthFilterReso.load());
    const float masterGain = juce::jlimit(0.0f, 2.0f, owner.synthMasterGain.load());

    const float f = juce::jlimit(0.01f, 0.85f, 2.0f * std::sin(juce::MathConstants<float>::pi * std::min(cutoffHz, sr * 0.42f) / sr));
    const float q = 1.0f - reso * 0.85f;

    float* outL = outputChannelData[0];
    float* outR = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        owner.synthRng = owner.synthRng * 1664525u + 1013904223u;
        const float noise = (static_cast<float>(owner.synthRng & 0xFFFFu) / 32768.0f) - 1.0f;

        float baseFreq = 58.0f;
        float pitchDrop = 160.0f;
        float envDecay = 0.99935f;
        float noiseMix = 0.28f;

        if (instType == 1) // Tamburi Giocattolo / Snare
        {
            baseFreq = 145.0f;
            pitchDrop = 220.0f;
            envDecay = 0.9991f;
            noiseMix = 0.55f;
        }
        else if (instType == 2) // Piano Feltro / Carillon / Melodico
        {
            baseFreq = 220.0f;
            pitchDrop = 12.0f;
            envDecay = 0.99965f;
            noiseMix = 0.06f;
        }
        else if (instType == 3) // Synth / Theremin / Pad
        {
            baseFreq = 110.0f + (cutoffHz / 12000.0f) * 330.0f;
            pitchDrop = 40.0f;
            envDecay = 0.9997f;
            noiseMix = 0.10f;
        }

        const float freq = baseFreq + owner.synthPitchEnv * pitchDrop;
        owner.synthPhase += freq / sr;
        if (owner.synthPhase >= 1.0f)
            owner.synthPhase -= 1.0f;

        const float tone = std::sin(owner.synthPhase * juce::MathConstants<float>::twoPi)
                         + 0.35f * std::sin(owner.synthPhase * juce::MathConstants<float>::twoPi * 2.0f);
        const float raw = (tone * (1.0f - noiseMix) + noise * noiseMix * owner.synthPitchEnv) * owner.synthEnv;

        // Filtro SVF 12dB/oct controllato dalle manopole Cutoff/Risonanza nel Canvas!
        owner.synthFilterLp += f * owner.synthFilterBp;
        const float hp = raw - owner.synthFilterLp - q * owner.synthFilterBp;
        owner.synthFilterBp += f * hp;

        const float sample = juce::jlimit(-0.95f, 0.95f, owner.synthFilterLp * 0.55f * masterGain);

        if (outL != nullptr) outL[i] = sample;
        if (outR != nullptr) outR[i] = sample;

        owner.synthEnv *= envDecay;
        owner.synthPitchEnv *= 0.9975f;
    }

    for (int ch = 2; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
}

void MicWorker::audioDeviceAboutToStart(juce::AudioIODevice* dev)
{
    if (dev != nullptr && dev->getCurrentSampleRate() > 0.0)
        owner.synthSampleRate = dev->getCurrentSampleRate();
}

void MicWorker::audioDeviceStopped()
{
}

void MicWorker::openDevices()
{
    owner.setMicStatus(HwState::Starting, "Avvio microfono...");

    if (wasapiType == nullptr)
        wasapiType.reset(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared));

    if (wasapiType == nullptr)
    {
        owner.setMicStatus(HwState::Error, "Driver audio WASAPI non disponibile");
        return;
    }

    wasapiType->scanForDevices();
    const auto inNames = wasapiType->getDeviceNames(true);
    const auto outNames = wasapiType->getDeviceNames(false);

    {
        const juce::ScopedLock sl(owner.micInfoLock);
        owner.micDeviceList = inNames;
    }

    if (inNames.isEmpty())
    {
        owner.setMicStatus(HwState::Error, "Nessun microfono rilevato");
        return;
    }

    int desiredIdx = owner.micDeviceIndex.load();
    if (desiredIdx < 0 || desiredIdx >= inNames.size())
    {
        int bestScore = -9999;
        int bestIdx = 0;
        for (int i = 0; i < inNames.size(); ++i)
        {
            const int sc = scoreMicDeviceName(inNames[i]);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestIdx = i;
            }
        }
        desiredIdx = bestIdx;
        owner.micDeviceIndex.store(desiredIdx);
    }

    // Prova prima il dispositivo scelto, poi eventuali alternative se fallisce l'apertura
    int openedIdx = -1;
    for (int attempt = 0; attempt < inNames.size() && !threadShouldExit(); ++attempt)
    {
        const int cand = (desiredIdx + attempt) % inNames.size();
        if (attempt > 0 && scoreMicDeviceName(inNames[cand]) <= -100)
            continue;

        std::unique_ptr<juce::AudioIODevice> dev(wasapiType->createDevice({}, inNames[cand]));
        if (dev != nullptr)
        {
            juce::BigInteger inChans;
            inChans.setRange(0, 2, true);
            juce::BigInteger outChans;
            const auto err = dev->open(inChans, outChans, 0.0, 0);
            if (err.isEmpty() && dev->isOpen())
            {
                dev->start(&owner);
                inDevice = std::move(dev);
                openedIdx = cand;
                break;
            }
        }
    }

    if (inDevice == nullptr)
    {
        owner.setMicStatus(HwState::Error, "Accesso microfono negato (verifica Privacy Windows)");
        return;
    }

    owner.micDeviceIndex.store(openedIdx);
    {
        const juce::ScopedLock sl(owner.micInfoLock);
        owner.micDeviceName = inNames[openedIdx];
    }
    owner.setMicStatus(HwState::Running, "In ascolto");

    // Apre in modo indipendente anche l'uscita audio per far suonare i colpi di batteria
    if (!outNames.isEmpty() && !threadShouldExit())
    {
        const int defOut = juce::jlimit(0, outNames.size() - 1, wasapiType->getDefaultDeviceIndex(false));
        std::unique_ptr<juce::AudioIODevice> oDev(wasapiType->createDevice(outNames[defOut], {}));
        if (oDev != nullptr)
        {
            juce::BigInteger inC;
            juce::BigInteger outC;
            outC.setRange(0, 2, true);
            if (oDev->open(inC, outC, 0.0, 0).isEmpty() && oDev->isOpen())
            {
                oDev->start(this);
                outDevice = std::move(oDev);
            }
        }
    }
}

void MicWorker::closeDevices()
{
    if (inDevice != nullptr)
    {
        inDevice->stop();
        inDevice->close();
        inDevice.reset();
    }
    if (outDevice != nullptr)
    {
        outDevice->stop();
        outDevice->close();
        outDevice.reset();
    }
    owner.realMicLevel.store(0.0f);
    owner.realMicPeakRaw.store(0.0f);
    owner.realMicTriggered.store(false);
}

// =============================================================================
// CameraWorker
// =============================================================================
CameraWorker::CameraWorker(MainComponent& o)
    : juce::Thread("MiMO Camera Worker"), owner(o)
{
}

CameraWorker::~CameraWorker()
{
    wanted.store(false);
    signalThreadShouldExit();
    notify();
    stopThread(5000);
}

void CameraWorker::setWanted(bool shouldRun)
{
    if (shouldRun && !wanted.load())
        reopenGen.fetch_add(1);
    wanted.store(shouldRun);
    notify();
}

void CameraWorker::requestReopen()
{
    reopenGen.fetch_add(1);
    notify();
}

void CameraWorker::run()
{
   #if JUCE_WINDOWS
    const long hr = CoInitializeEx(nullptr, 0x2 /* COINIT_APARTMENTTHREADED */);
   #endif

    int handledGen = -1;
    int noFrameTicks = 0;
    bool fallbackMode = false;
    float simPhase = 0.0f;

    while (!threadShouldExit())
    {
        const bool want = wanted.load();
        const int gen = reopenGen.load();

        if (!want)
        {
            if (device != nullptr || fallbackMode || owner.cameraState.load() != static_cast<int>(HwState::Off))
            {
                closeDevice();
                fallbackMode = false;
                owner.setCameraStatus(HwState::Off, "Webcam spenta");
            }
            handledGen = -1;
            noFrameTicks = 0;
        }
        else if (gen != handledGen)
        {
            handledGen = gen;
            noFrameTicks = 0;
            fallbackMode = false;
            closeDevice();
            if (!openDevice())
            {
                // Nessuna webcam fisica disponibile o accesso bloccato: attiva sensore ottico simulato live
                fallbackMode = true;
                owner.setCameraStatus(HwState::Running, "Sensore Ottico Live (Sim)");
            }
        }
        else if (device != nullptr && !fallbackMode)
        {
            // Se la webcam sta inviando fotogrammi normalmente, azzera il watchdog
            if (owner.cameraFrameCounter.load() > 0)
            {
                noFrameTicks = 0;
            }
            else
            {
                // Se una videocamera non invia frame dopo oltre 8 secondi, ritenta la riapertura dello stesso dispositivo
                if (++noFrameTicks >= 55)
                {
                    noFrameTicks = 0;
                    reopenGen.fetch_add(1);
                }
            }
        }

        if (want && fallbackMode && !threadShouldExit())
        {
            generateSimulatedOpticalFrame(simPhase);
            simPhase += 0.09f;
            wait(33); // ~30 FPS
        }
        else
        {
            wait(150);
        }
    }

    closeDevice();

   #if JUCE_WINDOWS
    if (hr >= 0)
        CoUninitialize();
   #endif
}

bool CameraWorker::openDevice()
{
    owner.setCameraStatus(HwState::Starting, "Avvio webcam...");
    owner.cameraFrameCounter.store(0);

    const auto names = juce::CameraDevice::getAvailableDevices();
    {
        const juce::ScopedLock sl(owner.cameraInfoLock);
        owner.cameraDeviceList = names;
    }

    if (names.isEmpty())
        return false;

    int desiredIdx = owner.cameraDeviceIndex.load();
    if (desiredIdx < 0 || desiredIdx >= names.size())
    {
        int bestScore = -9999;
        int bestIdx = 0;
        for (int i = 0; i < names.size(); ++i)
        {
            const int sc = scoreCameraDeviceName(names[i]);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestIdx = i;
            }
        }
        desiredIdx = bestIdx;
        owner.cameraDeviceIndex.store(desiredIdx);
    }

    // 1. Prova ad aprire il dispositivo desiderato (HP Wide Vision HD / videocamera scelta)
    {
        std::unique_ptr<juce::CameraDevice> d(juce::CameraDevice::openDevice(desiredIdx, 640, 480, 1280, 720, true));
        if (d == nullptr && !threadShouldExit())
            d.reset(juce::CameraDevice::openDevice(desiredIdx, 320, 240, 1280, 720, false));
        if (d == nullptr && !threadShouldExit())
            d.reset(juce::CameraDevice::openDevice(desiredIdx, 320, 240, 640, 480, false));
        if (d == nullptr && !threadShouldExit())
            d.reset(juce::CameraDevice::openDevice(desiredIdx, 160, 120, 1920, 1080, false));

        if (d != nullptr)
        {
            d->addListener(&owner);
            device = std::move(d);
            owner.cameraDeviceIndex.store(desiredIdx);
            {
                const juce::ScopedLock sl(owner.cameraInfoLock);
                owner.cameraDeviceName = names[desiredIdx];
            }
            owner.setCameraStatus(HwState::Running, "Webcam attiva: " + names[desiredIdx]);
            return true;
        }
    }

    // 2. Solo se il dispositivo primario fallisce, prova gli altri dispositivi presenti nel sistema
    for (int i = 0; i < names.size() && !threadShouldExit(); ++i)
    {
        if (i == desiredIdx)
            continue;

        std::unique_ptr<juce::CameraDevice> d(juce::CameraDevice::openDevice(i, 640, 480, 1280, 720, true));
        if (d == nullptr && !threadShouldExit())
            d.reset(juce::CameraDevice::openDevice(i, 320, 240, 640, 480, false));

        if (d != nullptr)
        {
            d->addListener(&owner);
            device = std::move(d);
            owner.cameraDeviceIndex.store(i);
            {
                const juce::ScopedLock sl(owner.cameraInfoLock);
                owner.cameraDeviceName = names[i];
            }
            owner.setCameraStatus(HwState::Running, "Webcam attiva: " + names[i]);
            return true;
        }
    }

    return false;
}

void CameraWorker::generateSimulatedOpticalFrame(float phase)
{
    juce::Image sim(juce::Image::RGB, 160, 120, false);
    juce::Graphics g(sim);
    g.fillAll(juce::Colour(0xFF1A2226));

    const float cx = 80.0f + std::sin(phase * 1.1f) * 48.0f;
    const float cy = 60.0f + std::cos(phase * 0.8f) * 32.0f;

    juce::ColourGradient grad(MiMOLookAndFeel::crayonCyan.withAlpha(0.85f), cx, cy,
                              juce::Colour(0xFF1A2226), cx + 55.0f, cy + 55.0f, true);
    g.setGradientFill(grad);
    g.fillEllipse(cx - 42.0f, cy - 42.0f, 84.0f, 84.0f);

    g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.75f));
    g.fillEllipse(cx - 14.0f, cy - 14.0f, 28.0f, 28.0f);

    owner.imageReceived(sim);
}

void CameraWorker::closeDevice()
{
    if (device != nullptr)
    {
        device->removeListener(&owner);
        device.reset();
    }

    const juce::ScopedLock sl(owner.cameraLock);
    owner.latestWebcamImage = juce::Image();
    owner.prevWebcamImage = juce::Image();
    owner.realCameraMotion.store(0.0f);
    owner.realCameraBrightness.store(0.0f);
    owner.hasNewWebcamFrame.store(false);
    owner.cameraFrameCounter.store(0);
}

} // namespace mimo
