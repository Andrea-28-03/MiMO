#include "MainComponent.h"
#include <BinaryData.h>
#include <cmath>

namespace mimo
{

MainComponent::MainComponent()
{
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);

    // Inizializzazione Tema: di default in Modalità Notte (Dark Mode)
    // Rileva lo stato del tema di sistema (Windows / macOS / Linux) e sincronizza dinamicamente
    const bool sysDark = detectSystemDarkMode();
    setDarkMode(sysDark);
    juce::Desktop::getInstance().addDarkModeSettingListener(this);

    // Caricamento del render hardware MiMO (da BinaryData o disco)
    mimoImage = juce::ImageFileFormat::loadFrom(BinaryData::mimo_device_png, BinaryData::mimo_device_pngSize);
    if (mimoImage.isNull())
    {
        juce::Array<juce::File> searchList = {
            juce::File::getCurrentWorkingDirectory().getChildFile("Resources/mimo_device.png"),
            juce::File::getCurrentWorkingDirectory().getChildFile("mimo_hub_juce/Resources/mimo_device.png"),
            juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("Resources/mimo_device.png"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/Resources/mimo_device.png"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/Immagini/MIMO.png")
        };
        for (const auto& f : searchList)
        {
            if (f.existsAsFile())
            {
                mimoImage = juce::ImageFileFormat::loadFrom(f);
                if (mimoImage.isValid())
                    break;
            }
        }
    }

    // Ottimizzazione grafica: pre-scala l'immagine di MiMO (da 1403x874 a 640x400) una sola volta all'avvio
    if (mimoImage.isValid() && mimoImage.getWidth() > 680)
        mimoImage = mimoImage.rescaled(640, 400, juce::Graphics::highResamplingQuality);

    initCatalogs();
    initDefaultStudioGraph();
    initDefaultLivePresets();

    setSize(1280, 800);
    canvasOffset = { 500.0f, 320.0f };
    panVelocity = { 0.0f, 0.0f };
    timeSinceLastWheel = 1.0f;

    // Avvio Motore Audio Modulare DSP Master (Zero latenza, output sempre attivo)
    audioEngine.startAudio();
    loadSettings();
    setAppMasterVolume(appMasterVolume.load());

    // Worker threads in attesa (nessun dispositivo viene aperto finché il modulo non entra nel Canvas!)
    micWorker = std::make_unique<MicWorker>(*this);
    micWorker->startThread();

    cameraWorker = std::make_unique<CameraWorker>(*this);
    cameraWorker->startThread();

    {
        const auto initialDevs = juce::CameraDevice::getAvailableDevices();
        const juce::ScopedLock sl(cameraInfoLock);
        cameraDeviceList = initialDevs;
        int bestScore = -9999;
        int bestIdx = 0;
        for (int i = 0; i < initialDevs.size(); ++i)
        {
            const int sc = scoreCameraDeviceName(initialDevs[i]);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestIdx = i;
            }
        }
        if (!initialDevs.isEmpty())
        {
            cameraDeviceIndex.store(bestIdx);
            cameraDeviceName = initialDevs[bestIdx];
        }
    }

    // Server Micro-HTTP & WebSocket + Google MediaPipe HandLandmarker su PC
    eyeServer = std::make_unique<mimo::MiMOEyeServer>(8088, 8089);
    eyeServer->setSmartphoneExclusive(useSmartphoneEye);
    eyeServer->onPinchTrigger = [this](int fingerIdx, float vel, bool isSecondary)
    {
        if (!eyeGesturePinchTrig || eyePhalanxDetail < 1)
            return;
        if (!isHandTriggersEnabled(isSecondary))
            return;
        // Triggera direttamente via AudioEngine da "hw-eye" (Fan-in su tutti i nodi collegati)
        audioEngine.triggerFromSource("hw-eye", fingerIdx, vel, isSecondary);
        lastTriggerSourceCode.store(3);
        lastTriggerFingerIdx.store(fingerIdx);
        lastTriggerIsSecondary.store(isSecondary);
        triggerPulseAlpha.store(1.0f);
    };
    eyeServer->onSmartphoneFrame = [this](const juce::Image& phoneImg)
    {
        // Se l'utente sta usando la Webcam PC, ignora categoricamente i frame dello smartphone
        if (!useSmartphoneEye)
            return;

        if (phoneImg.isValid())
        {
            {
                const juce::ScopedLock sl(cameraLock);
                latestWebcamImage = phoneImg;
                hasNewWebcamFrame.store(true);
            }

            // Calcolo immediato dell'analisi scheletro mano dal flusso video dello Smartphone
            const auto handSt = analyzeHandFrame(phoneImg);
            const bool mpActive = (eyeServer != nullptr && eyeServer->isMediaPipeActive());
            if (!mpActive)
            {
                latestEyeState = handSt;
                if (eyeServer != nullptr)
                    eyeServer->updateFromLocalWebcam(handSt);
            }
        }
    };
    eyeServer->startServer();

    syncEyeNodeOutputsWithDetailSettings();

    // Timer a 60 Hz per animazioni fluide e scorrimento a 60 FPS
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    saveSettings();
    juce::Desktop::getInstance().removeDarkModeSettingListener(this);
    stopTimer();
    audioEngine.stopAudio();
    if (eyeServer != nullptr)
        eyeServer->stopServer();
    eyeServer.reset();
    cameraWorker.reset();
    micWorker.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (outputChannelData != nullptr)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }

    if (inputChannelData == nullptr || numInputChannels < 1 || numSamples <= 0)
        return;

    // Picco su tutti i canali d'ingresso attivi
    float peak = 0.0f;
    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        const float* in = inputChannelData[ch];
        if (in == nullptr)
            continue;
        const auto range = juce::FloatVectorOperations::findMinAndMax(in, numSamples);
        peak = std::max(peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
    }
    realMicPeakRaw.store(peak);

    // Scala logaritmica (-58 dB .. -2 dB) -> 0..1: reattiva a battiti sul tavolo, mani e voce
    const float db = juce::Decibels::gainToDecibels(peak, -80.0f);
    const float level = juce::jlimit(0.0f, 1.0f, (db + 58.0f) / 56.0f);

    const float current = realMicLevel.load();
    const float smoothed = (level > current) ? (current * 0.25f + level * 0.75f) : (current * 0.90f + level * 0.10f);
    realMicLevel.store(juce::jlimit(0.0f, 1.0f, smoothed));

    // Rilevamento colpo (onset) con isteresi e tempo di blocco anti-rimbalzo (75 ms)
    const float threshold = micThresholdAtomic.load();
    if (micHoldSamples > 0)
        micHoldSamples -= numSamples;

    if (level >= threshold && micArmed && micHoldSamples <= 0)
    {
        const float vel = juce::jlimit(0.25f, 1.0f, (level - threshold) / std::max(0.05f, 1.0f - threshold) * 0.75f + 0.25f);
        if (isMicInCanvas())
        {
            realMicVelocity.store(vel);
            realMicTriggered.store(true);
            audioEngine.triggerFromSource("hw-mic", 0, vel);
            lastTriggerSourceCode.store(2);
            triggerPulseAlpha.store(1.0f);
            synthHitVelocity.store(vel);
            synthTriggerPending.store(true);
        }
        micArmed = false;
        micHoldSamples = static_cast<int>(micSampleRate * 0.075);
    }
    else if (level < threshold - 0.06f)
    {
        micArmed = true;
    }
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr && device->getCurrentSampleRate() > 0.0)
        micSampleRate = device->getCurrentSampleRate();
    micHoldSamples = 0;
    micArmed = true;
}

void MainComponent::audioDeviceStopped() {}

mimo::HandTrackingState MainComponent::analyzeHandFrame(const juce::Image& frame)
{
    mimo::HandTrackingState st;
    if (!frame.isValid())
    {
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }

    // Se il motore neurale Google MediaPipe e' attivo, usa direttamente l'analisi IA ufficiale
    if (eyeServer != nullptr && eyeServer->isMediaPipeActive())
    {
        return eyeServer->getLatestState();
    }

    juce::Image small = frame.rescaled(160, 120, juce::Graphics::lowResamplingQuality);
    const int w = small.getWidth();
    const int h = small.getHeight();
    if (w <= 0 || h <= 0)
    {
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }

    juce::Image::BitmapData bmp(small, juce::Image::BitmapData::readOnly);

    double sumX = 0.0;
    double sumY = 0.0;
    int skinPixelCount = 0;

    int minSkinX = w, maxSkinX = 0;
    int minSkinY = h, maxSkinY = 0;

    for (int y = 0; y < h; y += 2)
    {
        for (int x = 0; x < w; x += 2)
        {
            const auto col = bmp.getPixelColour(x, y);
            const int r = col.getRed();
            const int g = col.getGreen();
            const int b = col.getBlue();

            // Rilevamento colore pelle umana robusto (YCbCr + fallback RGB)
            const bool isSkin = isSkinPixel(r, g, b);

            if (isSkin)
            {
                sumX += x;
                sumY += y;
                skinPixelCount++;

                if (x < minSkinX) minSkinX = x;
                if (x > maxSkinX) maxSkinX = x;
                if (y < minSkinY) minSkinY = y;
                if (y > maxSkinY) maxSkinY = y;
            }
        }
    }

    const int totalSampledPixels = (w / 2) * (h / 2);
    const float skinRatio = static_cast<float>(skinPixelCount) / static_cast<float>(std::max(1, totalSampledPixels));

    if (skinRatio < 0.012f || skinPixelCount < 35)
    {
        st.hasHand = false;
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }

    // Baricentro del candidato normalizzato (0.0 .. 1.0)
    const float rawPalmX = static_cast<float>(sumX / skinPixelCount) / static_cast<float>(w);
    const float rawPalmY = static_cast<float>(sumY / skinPixelCount) / static_cast<float>(h);

    const float spanX = static_cast<float>(std::max(1, maxSkinX - minSkinX)) / static_cast<float>(w);
    const float spanY = static_cast<float>(std::max(1, maxSkinY - minSkinY)) / static_cast<float>(h);
    const float palmSpan = std::max(spanX, spanY);
    const float rawPalmZ = juce::jlimit(0.12f, 0.95f, palmSpan * 1.8f);

    // =========================================================================
    // DISCRIMINATORE GEOMETRICO: MANO VS VOLTO / COLLO / CORPO
    // Una mano aperta presenta dita allungate con valli interdigitali evidenti.
    // Un volto e una forma solida, convessa ed ellittica priva di dita sporgenti.
    // =========================================================================
    constexpr int numRays = 16;
    float rayRadii[numRays];
    float maxRayR = 0.0f;
    float minRayR = 999.0f;

    for (int k = 0; k < numRays; ++k)
    {
        const float ang = static_cast<float>(k) * (juce::MathConstants<float>::twoPi / static_cast<float>(numRays));
        const float dx = std::cos(ang);
        const float dy = std::sin(ang);

        float lastSkinR = 2.0f;
        const float maxSearchR = std::min(static_cast<float>(std::min(w, h)) * 0.45f, 48.0f);

        for (float r = 3.0f; r <= maxSearchR; r += 1.5f)
        {
            const int px = static_cast<int>(rawPalmX * w + dx * r);
            const int py = static_cast<int>(rawPalmY * h + dy * r);
            if (px < 0 || px >= w || py < 0 || py >= h)
                break;

            const auto col = bmp.getPixelColour(px, py);
            if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
            {
                lastSkinR = r;
            }
            else
            {
                const int px2 = static_cast<int>(rawPalmX * w + dx * (r + 3.0f));
                const int py2 = static_cast<int>(rawPalmY * h + dy * (r + 3.0f));
                if (px2 >= 0 && px2 < w && py2 >= 0 && py2 < h)
                {
                    const auto c2 = bmp.getPixelColour(px2, py2);
                    if (!isSkinPixel(c2.getRed(), c2.getGreen(), c2.getBlue()))
                        break;
                }
                else
                {
                    break;
                }
            }
        }

        rayRadii[k] = lastSkinR;
        if (lastSkinR > maxRayR) maxRayR = lastSkinR;
        if (lastSkinR < minRayR) minRayR = lastSkinR;
    }

    int fingerPeaks = 0;
    for (int k = 0; k < numRays; ++k)
    {
        const float prevR = rayRadii[(k + numRays - 1) % numRays];
        const float nextR = rayRadii[(k + 1) % numRays];
        const float curR = rayRadii[k];

        if (curR > prevR && curR > nextR)
        {
            const float valleyR = std::min(rayRadii[(k + numRays - 2) % numRays], rayRadii[(k + 2) % numRays]);
            if (curR >= valleyR * 1.32f && curR >= 9.0f)
            {
                fingerPeaks++;
            }
        }
    }

    const bool isCenterTopFaceArea = (rawPalmY < 0.68f && rawPalmX > 0.22f && rawPalmX < 0.78f);
    const bool isVeryLargeSkinBlob = (skinRatio > 0.080f);

    // Solo una sagoma massiccia priva di dita puo' essere considerata un volto
    if (isVeryLargeSkinBlob || (isCenterTopFaceArea && skinRatio > 0.050f && fingerPeaks < 1))
    {
        st.hasHand = false;
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }

    // Se non ci sono dita sporgenti (fingerPeaks < 1), verifica se e' un pugno chiuso compatto
    if (fingerPeaks < 1)
    {
        if (skinPixelCount >= 50 && skinPixelCount <= 5500)
        {
            st.hasHand = true;
            st.palmX = rawPalmX;
            st.palmY = rawPalmY;
            st.palmZ = rawPalmZ;
            st.fist = 1.0f; // Pugno chiuso
            st.handTilt = 0.5f;
            for (int f = 0; f < 5; ++f)
                st.fingerFlex[f] = 1.0f;
            st.handedness = "Right";

            if (hasSmoothedHandState)
            {
                const float alpha = 0.65f;
                st.palmX = smoothedHandState.palmX * (1.0f - alpha) + st.palmX * alpha;
                st.palmY = smoothedHandState.palmY * (1.0f - alpha) + st.palmY * alpha;
                st.palmZ = smoothedHandState.palmZ * (1.0f - alpha) + st.palmZ * alpha;
                st.fist  = smoothedHandState.fist  * (1.0f - alpha) + st.fist  * alpha;
            }
            smoothedHandState = st;
            hasSmoothedHandState = true;
            return st;
        }
        else
        {
            st.hasHand = false;
            hasSmoothedHandState = false;
            smoothedHandState.hasHand = false;
            return st;
        }
    }

    // Superati i test: mano rilevata!
    st.hasHand = true;

    // Momenti del secondo ordine per orientamento / asse principale
    double m11 = 0.0, m20 = 0.0, m02 = 0.0;
    const double cx = sumX / skinPixelCount;
    const double cy = sumY / skinPixelCount;

    for (int y = minSkinY; y <= maxSkinY; y += 3)
    {
        for (int x = minSkinX; x <= maxSkinX; x += 3)
        {
            const auto col = bmp.getPixelColour(x, y);
            if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
            {
                const double dx = x - cx;
                const double dy = y - cy;
                m20 += dx * dx;
                m02 += dy * dy;
                m11 += dx * dy;
            }
        }
    }

    const double theta = 0.5 * std::atan2(2.0 * m11, m20 - m02);
    const float rawTilt = juce::jlimit(0.0f, 1.0f, static_cast<float>(0.5 + (theta / juce::MathConstants<double>::pi)));

    float mainAng = static_cast<float>(theta) - juce::MathConstants<float>::halfPi;
    // In coordinate immagine (Y verso il basso), una mano sollevata davanti alla webcam ha le dita verso l'alto (-Y)
    if (std::sin(mainAng) > 0.15f)
        mainAng += juce::MathConstants<float>::pi;

    const float baseHandRadius = std::max(0.09f, palmSpan * 0.45f);

    // Angoli delle 5 dita rispetto alla direzione principale (Thumb, Index, Middle, Ring, Pinky)
    const float fingerAngles[5] = { -0.92f, -0.40f, 0.0f, 0.38f, 0.76f };
    const float baseTipLengths[5] = { 0.85f, 1.05f, 1.15f, 1.02f, 0.82f };

    float fingerExt[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    juce::Point<float> rawTips[5];

    for (int f = 0; f < 5; ++f)
    {
        const float ang = mainAng + fingerAngles[f];
        const float dirX = std::cos(ang);
        const float dirY = std::sin(ang);

        const float maxR = baseHandRadius * baseTipLengths[f];
        float bestR = baseHandRadius * 0.48f;

        for (float rStep = 0.04f; rStep <= maxR * 1.45f; rStep += 0.02f)
        {
            const int px = static_cast<int>((rawPalmX + dirX * rStep) * w);
            const int py = static_cast<int>((rawPalmY + dirY * rStep) * h);

            if (px >= 0 && px < w && py >= 0 && py < h)
            {
                const auto col = bmp.getPixelColour(px, py);
                if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
                {
                    bestR = rStep;
                }
            }
        }

        const float normExt = juce::jlimit(0.25f, 1.0f, bestR / maxR);
        fingerExt[f] = normExt;
        rawTips[f] = {
            juce::jlimit(0.03f, 0.97f, rawPalmX + dirX * bestR),
            juce::jlimit(0.03f, 0.97f, rawPalmY + dirY * bestR)
        };
        st.fingerFlex[f] = juce::jlimit(0.0f, 1.0f, (1.0f - normExt) * 1.4f);
    }

    st.fist = juce::jlimit(0.0f, 1.0f, (st.fingerFlex[1] + st.fingerFlex[2] + st.fingerFlex[3] + st.fingerFlex[4]) * 0.25f);

    const float wristAng = mainAng + juce::MathConstants<float>::pi;
    const juce::Point<float> rawWrist = {
        juce::jlimit(0.03f, 0.97f, rawPalmX + std::cos(wristAng) * baseHandRadius * 0.88f),
        juce::jlimit(0.03f, 0.97f, rawPalmY + std::sin(wristAng) * baseHandRadius * 0.88f)
    };

    // 0: Polso
    st.landmarks[0] = { rawWrist.x, rawWrist.y, 0.0f };

    const int tipIndices[5] = { 4, 8, 12, 16, 20 };
    const int mcpIndices[5] = { 1, 5, 9, 13, 17 };

    for (int f = 0; f < 5; ++f)
    {
        const int mcpIdx = mcpIndices[f];
        const int tipIdx = tipIndices[f];

        const float mcpFrac = (f == 0) ? 0.35f : 0.44f;
        const juce::Point<float> mcpPt = {
            rawWrist.x * (1.0f - mcpFrac) + rawTips[f].x * mcpFrac,
            rawWrist.y * (1.0f - mcpFrac) + rawTips[f].y * mcpFrac
        };
        st.landmarks[mcpIdx] = { mcpPt.x, mcpPt.y, 0.0f };

        const juce::Point<float> pipPt = {
            mcpPt.x * 0.60f + rawTips[f].x * 0.40f,
            mcpPt.y * 0.60f + rawTips[f].y * 0.40f
        };
        st.landmarks[mcpIdx + 1] = { pipPt.x, pipPt.y, 0.0f };

        const juce::Point<float> dipPt = {
            mcpPt.x * 0.25f + rawTips[f].x * 0.75f,
            mcpPt.y * 0.25f + rawTips[f].y * 0.75f
        };
        st.landmarks[mcpIdx + 2] = { dipPt.x, dipPt.y, 0.0f };

        st.landmarks[tipIdx] = { rawTips[f].x, rawTips[f].y, 0.0f };
    }

    // 4 Pinch indipendenti (Pollice [4] con Indice [8], Medio [12], Anulare [16], Mignolo [20])
    const juce::Point<float> thumbTip(st.landmarks[4].x, st.landmarks[4].y);
    for (int p = 0; p < 4; ++p)
    {
        const int targetTipIdx = tipIndices[p + 1];
        const juce::Point<float> targetTip(st.landmarks[targetTipIdx].x, st.landmarks[targetTipIdx].y);
        const float dist = thumbTip.getDistanceFrom(targetTip);

        const float pinchStrength = juce::jlimit(0.0f, 1.0f, 1.0f - (dist / 0.09f));
        st.pinches[p] = pinchStrength;
        st.pinchActive[p] = (pinchStrength > 0.52f);
    }

    st.palmX = rawPalmX;
    st.palmY = rawPalmY;
    st.palmZ = rawPalmZ;
    st.handTilt = rawTilt;
    st.handedness = (rawTips[0].x < rawTips[4].x) ? "Right" : "Left";

    // Filtro Mano Destra / Sinistra impostato dall'utente
    if (eyeHandFilter == 1 && st.handedness != "Right")
    {
        st.hasHand = false;
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }
    if (eyeHandFilter == 2 && st.handedness != "Left")
    {
        st.hasHand = false;
        hasSmoothedHandState = false;
        smoothedHandState.hasHand = false;
        return st;
    }

    // Temporal Smoothing (EMA) per fluidita estrema e zero jitter a 60 FPS
    if (hasSmoothedHandState)
    {
        const float alpha = 0.65f;
        st.palmX = smoothedHandState.palmX * (1.0f - alpha) + st.palmX * alpha;
        st.palmY = smoothedHandState.palmY * (1.0f - alpha) + st.palmY * alpha;
        st.palmZ = smoothedHandState.palmZ * (1.0f - alpha) + st.palmZ * alpha;
        st.fist  = smoothedHandState.fist  * (1.0f - alpha) + st.fist  * alpha;
        st.handTilt = smoothedHandState.handTilt * (1.0f - alpha) + st.handTilt * alpha;

        for (size_t i = 0; i < 21; ++i)
        {
            st.landmarks[i].x = smoothedHandState.landmarks[i].x * (1.0f - alpha) + st.landmarks[i].x * alpha;
            st.landmarks[i].y = smoothedHandState.landmarks[i].y * (1.0f - alpha) + st.landmarks[i].y * alpha;
            st.landmarks[i].z = smoothedHandState.landmarks[i].z * (1.0f - alpha) + st.landmarks[i].z * alpha;
        }

        for (size_t p = 0; p < 4; ++p)
        {
            if (st.pinchActive[p] && !smoothedHandState.pinchActive[p])
            {
                st.pinchTrigger[p] = true;
                st.pinchVelocity[p] = juce::jlimit(0.4f, 1.0f, 0.7f + st.pinches[p] * 0.3f);

                if (eyeGesturePinchTrig && isHandTriggersEnabled(false))
                {
                    audioEngine.triggerFromSource("hw-eye", static_cast<int>(p), st.pinchVelocity[p], false);
                    lastTriggerSourceCode.store(3);
                    lastTriggerFingerIdx.store(static_cast<int>(p));
                    lastTriggerIsSecondary.store(false);
                    triggerPulseAlpha.store(1.0f);
                }
            }
        }
    }

    smoothedHandState = st;
    hasSmoothedHandState = true;
    return st;
}

void MainComponent::imageReceived (const juce::Image& image)
{
    if (!image.isValid())
        return;

    // Se l'utente ha selezionato lo Smartphone, scarta categoricamente qualsiasi frame dalla webcam PC!
    if (useSmartphoneEye)
        return;

    // Risoluzione proporzionale senza deformazione (mantiene l'aspect ratio nativo della fotocamera: 16:9 o 4:3)
    const int srcW = image.getWidth();
    const int srcH = image.getHeight();
    const int targetW = 320;
    const int targetH = (srcW > 0) ? std::max(120, (320 * srcH) / srcW) : 240;

    juce::Image analysisFrame = image.rescaled(targetW, targetH, juce::Graphics::mediumResamplingQuality);
    if (cameraMirror)
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
        juce::ScopedLock sl(cameraLock);
        const bool hasPrev = (prevWebcamImage.isValid() && prevWebcamImage.getWidth() == w && prevWebcamImage.getHeight() == h);
        std::unique_ptr<juce::Image::BitmapData> prevBmpPtr;
        if (hasPrev)
            prevBmpPtr = std::make_unique<juce::Image::BitmapData>(prevWebcamImage, juce::Image::BitmapData::readOnly);

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
        realCameraBrightness.store(sampleCount > 0.0f ? (totalBrightness / sampleCount) : 0.0f);
        const float normMotion = juce::jlimit(0.0f, 1.0f, (sampleCount > 0.0f ? (motionDiff / sampleCount) : 0.0f) * 12.0f * cameraSensitivity);
        realCameraMotion.store(normMotion);

        if (motionWeightTotal > 0.01f)
        {
            const float cx = (weightedX / motionWeightTotal) * invW;
            const float cy = (weightedY / motionWeightTotal) * invH;
            realCameraCentroidX.store(cx * 2.0f - 1.0f);
            realCameraCentroidY.store(cy * 2.0f - 1.0f);
        }

        prevWebcamImage = small.createCopy();
        latestWebcamImage = analysisFrame;
        cameraFrameCounter.fetch_add(1);
        hasNewWebcamFrame.store(true);
    }

    // Analisi della mano ultra-rapida a 21 punti in C++
    const auto handSt = analyzeHandFrame(analysisFrame);
    const bool mpActive = (eyeServer != nullptr && eyeServer->isMediaPipeActive());

    if (!mpActive)
    {
        if (handSt.hasHand)
        {
            latestEyeState = handSt;
            if (eyeServer != nullptr)
                eyeServer->updateFromLocalWebcam(handSt);
        }
        else
        {
            latestEyeState.hasHand = false;
        }
    }

    // Invia sempre il frame al worker locale MediaPipe per l'analisi neurale a 21 punti
    if (eyeServer != nullptr)
    {
        eyeServer->pushVideoFrameForAnalysis(analysisFrame, false);
    }
}

bool MainComponent::isDrumInCanvas() const
{
    for (const auto& n : nodes)
        if (n.id == "hw-drum")
            return true;
    return false;
}

bool MainComponent::isMicInCanvas() const
{
    for (const auto& n : nodes)
        if (n.isIntegratedMic || n.id == "hw-mic")
            return true;
    return false;
}

bool MainComponent::isCameraInCanvas() const
{
    for (const auto& n : nodes)
        if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
            return true;
    return false;
}

bool MainComponent::isHandEnabled(bool isSecondary) const
{
    const juce::String rootId = isSecondary ? "eye_sx" : "eye_dx";
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera)
        {
            for (const auto& grp : n.outputGroups)
            {
                if (grp.id == rootId)
                    return grp.isEnabled;
            }
        }
    }
    return true;
}

bool MainComponent::isHandTriggersEnabled(bool isSecondary) const
{
    if (!isHandEnabled(isSecondary))
        return false;

    const juce::String trigId = isSecondary ? "eye_sx_pinches" : "eye_dx_pinches";
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera)
        {
            for (const auto& grp : n.outputGroups)
            {
                if (grp.id == trigId)
                    return grp.isEnabled;
            }
        }
    }
    return true;
}

bool MainComponent::isGroupEnabled(const juce::String& groupId) const
{
    for (const auto& n : nodes)
    {
        if (n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera || n.id == "hw-drum")
        {
            for (const auto& grp : n.outputGroups)
            {
                if (grp.id == groupId)
                {
                    if (!grp.parentGroup.isEmpty())
                    {
                        for (const auto& pGrp : n.outputGroups)
                        {
                            if (pGrp.id == grp.parentGroup && !pGrp.isEnabled)
                                return false;
                        }
                    }
                    return grp.isEnabled;
                }
            }
        }
    }
    return true;
}

void MainComponent::updateHardwareDeviceStates()
{
    const bool micInCanvas = isMicInCanvas();
    const bool cameraInCanvas = isCameraInCanvas();

    if (micInCanvas && !isMicRecording)
        startRealMicrophone();
    else if (!micInCanvas && isMicRecording)
        stopRealMicrophone();

    if (cameraInCanvas && !isCameraRecording)
        startRealCamera();
    else if (!cameraInCanvas && isCameraRecording)
        stopRealCamera();
}

void MainComponent::startRealMicrophone()
{
    isMicRecording = true;
    setMicStatus(HwState::Starting, "Avvio microfono...");
    if (micWorker != nullptr)
        micWorker->setWanted(true);
}

void MainComponent::stopRealMicrophone()
{
    isMicRecording = false;
    if (micWorker != nullptr)
        micWorker->setWanted(false);
}

void MainComponent::cycleMicDevice()
{
    juce::StringArray list;
    {
        const juce::ScopedLock sl(micInfoLock);
        list = micDeviceList;
    }
    if (list.isEmpty())
    {
        if (micWorker != nullptr && isMicRecording)
            micWorker->requestReopen();
        return;
    }

    const int nextIdx = ( std::max(0, micDeviceIndex.load()) + 1 ) % list.size();
    micDeviceIndex.store(nextIdx);
    {
        const juce::ScopedLock sl(micInfoLock);
        micDeviceName = list[nextIdx];
    }
    if (micWorker != nullptr && isMicRecording)
        micWorker->requestReopen();
    repaint();
}

void MainComponent::setMicStatus(HwState st, const juce::String& text)
{
    {
        const juce::ScopedLock sl(micInfoLock);
        micStatusText = text;
    }
    micStateAtomic.store(static_cast<int>(st));
}

juce::String MainComponent::getMicStatusText() const
{
    const juce::ScopedLock sl(micInfoLock);
    return micStatusText.isNotEmpty() ? micStatusText : juce::String("In ascolto");
}

juce::String MainComponent::getMicDeviceLabel() const
{
    const juce::ScopedLock sl(micInfoLock);
    if (micDeviceName.isNotEmpty())
        return micDeviceName;
    if (micDeviceList.size() > 0)
        return micDeviceList[juce::jlimit(0, micDeviceList.size() - 1, std::max(0, micDeviceIndex.load()))];
    return "Microfono predefinito";
}

void MainComponent::startRealCamera()
{
    isCameraRecording = true;
    if (useSmartphoneEye)
    {
        if (eyeServer != nullptr)
            eyeServer->setSmartphoneExclusive(true);
        if (cameraWorker != nullptr)
            cameraWorker->setWanted(false);
        const bool isStreaming = (eyeServer != nullptr && eyeServer->isSmartphoneStreaming());
        setCameraStatus(HwState::Running, isStreaming ? "Smartphone Connesso (Wi-Fi)" : "Smartphone in attesa stream...");
    }
    else
    {
        if (eyeServer != nullptr)
            eyeServer->setSmartphoneExclusive(false);
        const juce::String label = getCameraDeviceLabel();
        setCameraStatus(HwState::Starting, "Avvio webcam: " + label);
        if (cameraWorker != nullptr)
        {
            cameraWorker->setWanted(true);
            cameraWorker->requestReopen();
        }
    }
}

void MainComponent::stopRealCamera()
{
    isCameraRecording = false;
    if (cameraWorker != nullptr)
        cameraWorker->setWanted(false);
}

void MainComponent::cycleCameraDevice()
{
    juce::StringArray list;
    {
        const juce::ScopedLock sl(cameraInfoLock);
        list = cameraDeviceList;
    }
    if (list.size() <= 1)
    {
        selectVideoSource(false, 0, list.size() > 0 ? list[0] : juce::String());
        return;
    }

    const int nextIdx = (std::max(0, cameraDeviceIndex.load()) + 1) % list.size();
    selectVideoSource(false, nextIdx, list[nextIdx]);
}

void MainComponent::setCameraStatus(HwState st, const juce::String& text)
{
    {
        const juce::ScopedLock sl(cameraInfoLock);
        cameraStatusText = text;
    }
    cameraState.store(static_cast<int>(st));
}

juce::String MainComponent::getCameraStatusText() const
{
    const juce::ScopedLock sl(cameraInfoLock);
    return cameraStatusText.isNotEmpty() ? cameraStatusText : juce::String("Avvio webcam...");
}

juce::String MainComponent::getCameraDeviceLabel() const
{
    if (useSmartphoneEye)
        return "Smartphone (Fotocamera Wi-Fi)";

    const juce::ScopedLock sl(cameraInfoLock);
    juce::String name = cameraDeviceName;
    if (name.isEmpty() && cameraDeviceList.size() > 0)
        name = cameraDeviceList[juce::jlimit(0, cameraDeviceList.size() - 1, std::max(0, cameraDeviceIndex.load()))];
    if (name.isEmpty())
        return "HP Wide Vision HD (PC)";

    if (name.containsIgnoreCase("camo"))
        return "Camo (Smartphone 60 FPS)";
    if (name.containsIgnoreCase("hp") || name.containsIgnoreCase("wide vision") || name.containsIgnoreCase("nvidia"))
        return "HP Wide Vision HD (PC)";
    return name;
}

juce::Rectangle<float> MainComponent::getIntegratedDeviceRowBounds(const juce::Rectangle<float>& nodeBox)
{
    return { nodeBox.getX() + 12.0f, nodeBox.getBottom() - 26.0f, nodeBox.getWidth() - 24.0f, 19.0f };
}

std::vector<MainComponent::VideoSourceItem> MainComponent::getVideoSourceItems() const
{
    std::vector<VideoSourceItem> items;

    juce::StringArray devList = juce::CameraDevice::getAvailableDevices();
    if (devList.isEmpty())
    {
        const juce::ScopedLock sl(cameraInfoLock);
        devList = cameraDeviceList;
    }

    // 1. Trova l'indice hardware o bridge della webcam nativa del PC (HP Wide Vision / integrata / NVIDIA bridge)
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
    // Voce HP Wide Vision SEMPRE presente in prima posizione come scelta predefinita assoluta!
    items.push_back({ "HP Wide Vision HD (Webcam PC Predefinita)", false, pcDefaultIdx });

    // 2. Opzione per lo Smartphone Web App (via Wi-Fi / QR)
    const bool phoneStreaming = (eyeServer != nullptr && eyeServer->isSmartphoneStreaming());
    const juce::String phoneLabel = phoneStreaming
        ? "Smartphone (Web App Wi-Fi - Connesso)"
        : "Smartphone (Web App Fotocamera Wi-Fi)";
    items.push_back({ phoneLabel, true, -1 });

    // 3. Camo (Smartphone via USB / Wi-Fi)
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
    {
        items.push_back({ "Camo (Smartphone USB/Wi-Fi 60 FPS)", false, camoIdx });
    }

    // 4. Tutte le altre webcam PC hardware o virtuali (esclusi il default e Camo)
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

void MainComponent::selectVideoSource(bool useSmartphone, int pcDeviceIdx, const juce::String& pcDeviceName)
{
    useSmartphoneEye = useSmartphone;

    if (eyeServer != nullptr)
        eyeServer->setSmartphoneExclusive(useSmartphone);

    {
        const juce::ScopedLock sl(cameraLock);
        latestWebcamImage = juce::Image();
        prevWebcamImage = juce::Image();
        hasNewWebcamFrame.store(false);
    }

    smoothedHandState = mimo::HandTrackingState{};
    latestEyeState = mimo::HandTrackingState{};
    hasSmoothedHandState = false;

    if (useSmartphone)
    {
        if (cameraWorker != nullptr)
            cameraWorker->setWanted(false);

        const bool isStreaming = (eyeServer != nullptr && eyeServer->isSmartphoneStreaming());
        setCameraStatus(HwState::Running, isStreaming ? "Smartphone Connesso (Wi-Fi)" : "Smartphone in attesa stream...");
    }
    else
    {
        if (pcDeviceIdx >= 0)
            cameraDeviceIndex.store(pcDeviceIdx);

        if (pcDeviceName.isNotEmpty())
        {
            const juce::ScopedLock sl(cameraInfoLock);
            cameraDeviceName = pcDeviceName;
        }

        isCameraRecording = true;
        if (cameraWorker != nullptr)
        {
            cameraWorker->setWanted(true);
            cameraWorker->requestReopen();
        }

        const juce::String label = getCameraDeviceLabel();
        setCameraStatus(HwState::Starting, "Avvio webcam: " + label);
    }

    repaint();
}

juce::StringArray MainComponent::getCameraDropdownOptions()
{
    const auto items = getVideoSourceItems();
    juce::StringArray opts;
    for (const auto& it : items)
        opts.add(it.label);
    return opts;
}

juce::StringArray MainComponent::getMicDropdownOptions()
{
    juce::StringArray opts;
    {
        const juce::ScopedLock sl(micInfoLock);
        opts = micDeviceList;
    }
    if (opts.isEmpty())
        opts.add("Microfono Integrato PC");
    return opts;
}

void MainComponent::toggleDrumInCanvas()
{
    if (isDrumInCanvas())
    {
        cables.erase(std::remove_if(cables.begin(), cables.end(),
            [](const CanvasCable& c) {
                return c.sourceNodeId == "hw-drum" || c.targetNodeId == "hw-drum";
            }), cables.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
            [](const CanvasNode& n) {
                return n.id == "hw-drum";
            }), nodes.end());
        if (selectedNodeId == "hw-drum")
            selectedNodeId.clear();
    }
    else
    {
        CanvasNode drum;
        drum.id = "hw-drum";
        drum.type = NodeType::HardwareModule;
        drum.label = "Modulo Batteria";
        drum.subtitle = "Sensori Piezo - I2C Ch1";
        drum.portBadge = "P1";
        drum.pos = { -280.0f, 175.0f };
        drum.size = { 255.0f, 215.0f };
        drum.crayonColour = MiMOLookAndFeel::crayonSage;

        NodeOutputGroup gTrig;
        gTrig.id = "drum_trig";
        gTrig.name = "Impulsi Trigger (Colpo)";
        gTrig.parentGroup = "";
        gTrig.isExpanded = true;
        gTrig.items = {
            { "trigger:out", "Trigger Colpo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        NodeOutputGroup gMod;
        gMod.id = "drum_mod";
        gMod.name = "Modulazione & Dinamica";
        gMod.parentGroup = "";
        gMod.isExpanded = true;
        gMod.items = {
            { "mod:vel",   "Velocity",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:press", "Pressione", MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };
        drum.outputGroups = { gTrig, gMod };
        drum.size.y = getNodeCalculatedHeight(drum);
        nodes.push_back(drum);

        cables.push_back({ "c-drum-pi", "hw-drum", "audio:out", "rockpi", "audio:p1", false, MiMOLookAndFeel::fountainInk, {} });
    }

    updateHardwareDeviceStates();
    syncAudioRoutes();
    repaint();
}

void MainComponent::toggleMicInCanvas()
{
    micDropdownOpen = false;
    if (isMicInCanvas())
    {
        stopRealMicrophone();
        cables.erase(std::remove_if(cables.begin(), cables.end(),
            [](const CanvasCable& c) {
                return c.sourceNodeId == "hw-mic" || c.targetNodeId == "hw-mic";
            }), cables.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
            [](const CanvasNode& n) {
                return n.id == "hw-mic" || n.isIntegratedMic;
            }), nodes.end());
        if (selectedNodeId == "hw-mic")
            selectedNodeId.clear();
    }
    else
    {
        CanvasNode micNode;
        micNode.id = "hw-mic";
        micNode.type = NodeType::HardwareModule;
        micNode.isIntegratedMic = true;
        micNode.label = "Microfono PC (Batteria)";
        micNode.portBadge = "MIC";
        micNode.subtitle = "Laptop Mic - Trigger Batteria";
        micNode.pos = { -560.0f, 175.0f };
        micNode.size = { 245.0f, 230.0f };
        micNode.crayonColour = MiMOLookAndFeel::crayonOchre;
        micNode.modOutputs = {
            { "vel",   "Velocity",  MiMOLookAndFeel::crayonCherry },
            { "press", "Pressione", MiMOLookAndFeel::crayonPeach }
        };
        nodes.push_back(micNode);

        startRealMicrophone();
    }

    updateHardwareDeviceStates();
    syncAudioRoutes();
    repaint();
}

void MainComponent::syncEyeNodeOutputsWithDetailSettings()
{
    if (eyeServer != nullptr)
    {
        eyeServer->setHandFilter(0); // Entrambe le mani
        eyeServer->setPinchTriggerEnabled(true);
    }

    for (auto& n : nodes)
    {
        if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
        {
            MiMOEyeDrawer::syncEyeNodeOutputsWithDetailSettings(n, eyePhalanxDetail, eyeHandFilter,
                                                               eyeGesturePos3D, eyeGestureFistTilt,
                                                               eyeGestureFingerFlex, eyeGesturePinchTrig);
        }
    }
}

void MainComponent::toggleCameraInCanvas()
{
    camDropdownOpen = false;
    if (isCameraInCanvas())
    {
        stopRealCamera();
        cables.erase(std::remove_if(cables.begin(), cables.end(),
            [](const CanvasCable& c) {
                return c.sourceNodeId == "hw-eye" || c.targetNodeId == "hw-eye" ||
                       c.sourceNodeId == "hw-webcam" || c.targetNodeId == "hw-webcam";
            }), cables.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
            [](const CanvasNode& n) {
                return n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera;
            }), nodes.end());
        if (selectedNodeId == "hw-eye" || selectedNodeId == "hw-webcam")
            selectedNodeId.clear();
    }
    else
    {
        CanvasNode eye;
        eye.id = "hw-eye";
        eye.type = NodeType::HardwareModule;
        eye.isIntegratedCamera = true;
        eye.label = "Modulo Occhio";
        eye.subtitle = "Tracking Mano 3D - 60 FPS";
        eye.portBadge = "P2";
        eye.pos = { 35.0f, 175.0f };
        eye.size = { 255.0f, 380.0f };
        eye.crayonColour = MiMOLookAndFeel::crayonSage;
        nodes.push_back(eye);

        cables.push_back({ "c-eye-pi", "hw-eye", "audio:out", "rockpi", "audio:p2", false, MiMOLookAndFeel::fountainInk, {} });
        syncEyeNodeOutputsWithDetailSettings();
        startRealCamera();
    }

    updateHardwareDeviceStates();
    syncAudioRoutes();
    repaint();
}

void MainComponent::timerCallback()
{
    bool needsRepaint = false;
    ++frameCounter;

    // 0. Decelerazione cinetica e inerzia fisica a 60 FPS al rilascio del touchpad/rotellina
    timeSinceLastWheel += 1.0f / 60.0f;
    if (timeSinceLastWheel > 0.032f)
    {
        const float currentSpeed = std::hypot(panVelocity.x, panVelocity.y);
        if (currentSpeed > 0.12f)
        {
            canvasOffset.x += panVelocity.x;
            canvasOffset.y += panVelocity.y;
            panVelocity *= 0.90f;
            needsRepaint = true;
        }
        else if (!panVelocity.isOrigin())
        {
            panVelocity = { 0.0f, 0.0f };
        }
    }

    // 1. Animazione allargamento fluido dei nodi (Spring / LERP a 60 FPS)
    bool anyNodeAnimating = false;
    for (auto& n : nodes)
    {
        if (n.isAnimatingPos)
        {
            const float nDiffX = n.targetPos.x - n.pos.x;
            const float nDiffY = n.targetPos.y - n.pos.y;
            if (std::abs(nDiffX) > 0.8f || std::abs(nDiffY) > 0.8f)
            {
                n.pos.x += nDiffX * 0.32f;
                n.pos.y += nDiffY * 0.32f;
                anyNodeAnimating = true;
            }
            else
            {
                n.pos = n.targetPos;
                n.isAnimatingPos = false;
            }
        }
    }

    if (anyNodeAnimating)
        needsRepaint = true;

    // 2. Countdown Polaroid nello Studio (aggiornato a ~6 Hz)
    if (hasPendingPolaroid && !polaroidHasTyped)
    {
        if (frameCounter % 10 == 0)
        {
            --polaroidCountdownTicks;
            if (polaroidCountdownTicks <= 0)
                commitPolaroidToAlbum(pendingPolaroid);
            needsRepaint = true;
        }
    }

    // 3. Line-boil dei cavi d'inchiostro se siamo nello Studio (aggiornato a ~12 Hz per disegno naif)
    if (activeTab == ActiveTab::Studio && !cables.empty())
    {
        if (frameCounter % 5 == 0)
        {
            boilTick = (boilTick + 1) % 12;
            needsRepaint = true;
        }
    }

    // 4. Animazioni e micro-interazioni Workspace LIVE (Feedback I2C & Looper)
    if (activeTab == ActiveTab::Live)
    {
        if (looperState != LooperState::Idle)
        {
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const double elapsed = now - looperStartTime;
            looperSweepProgress = static_cast<float>(std::fmod(elapsed, looperDurationSec) / looperDurationSec);
            needsRepaint = true;
        }

        if (holdSlotIndex >= 0)
        {
            holdProgress += 1.0f / 60.0f;
            if (holdProgress >= 1.0f)
            {
                if (selectedAlbumIndex >= 0 && selectedAlbumIndex < static_cast<int>(photoAlbum.size()))
                {
                    assignPolaroidToSlot(holdSlotIndex, photoAlbum[static_cast<size_t>(selectedAlbumIndex)]);
                    selectedAlbumIndex = -1;
                }
                else
                {
                    triggerSceneSlot(holdSlotIndex);
                }
                holdSlotIndex = -1;
                holdProgress = 0.0f;
            }
            needsRepaint = true;
        }

        if (snapAnimScale > 1.002f)
        {
            snapAnimScale += (1.0f - snapAnimScale) * 0.16f;
            needsRepaint = true;
        }
    }

    // 5. Aggiornamento e animazione Hardware Reale del Laptop (Microfono & Webcam)
    updateHardwareDeviceStates();
    micThresholdAtomic.store(getMicThreshold());

    // Sincronizza i parametri del sintetizzatore interno con i nodi del Canvas
    for (const auto& n : nodes)
    {
        if (n.isIntegratedMic || n.id == "hw-mic" || n.id == "hw-drum")
        {
            if (n.instrumentName.containsIgnoreCase("Giocattolo"))
                synthInstrumentType.store(1);
            else if (n.instrumentName.containsIgnoreCase("Piano") || n.instrumentName.containsIgnoreCase("Carillon"))
                synthInstrumentType.store(2);
            else if (n.instrumentName.containsIgnoreCase("Theremin") || n.instrumentName.containsIgnoreCase("Pad"))
                synthInstrumentType.store(3);
            else
                synthInstrumentType.store(0);
        }
        else if (n.type == NodeType::NestedEffect)
        {
            for (const auto& b : n.innerBlocks)
            {
                for (const auto& p : b.params)
                {
                    if (p.key == "cutoff" || p.unit.containsIgnoreCase("Hz"))
                        synthFilterCutoffHz.store(p.value);
                    else if (p.key == "res" || p.key == "q")
                        synthFilterReso.store(p.getNormalized());
                }
            }
        }
        else if (n.type == NodeType::MasterOut && !n.directParams.empty())
        {
            synthMasterGain.store(juce::Decibels::decibelsToGain(n.directParams[0].value, -48.0f));
        }
    }

    {
        static int lastMicSt = -1;
        const int ms = micStateAtomic.load();
        if (ms != lastMicSt)
        {
            lastMicSt = ms;
            needsRepaint = true;
        }
    }

    if (isMicRecording)
    {
        bool micChanged = false;
        if (realMicTriggered.load())
        {
            micTriggerFired = true;
            micTriggerTicks = 8;
            realMicTriggered.store(false);
            micChanged = true;
        }
        else if (micTriggerTicks > 0)
        {
            --micTriggerTicks;
            if (micTriggerTicks == 0)
            {
                micTriggerFired = false;
                micChanged = true;
            }
        }

        if (micChanged)
        {
            for (auto& n : nodes)
                if (n.isIntegratedMic || n.id == "hw-mic")
                    n.crayonColour = micTriggerFired ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::crayonOchre;
        }

        const float lvl = realMicLevel.load();
        if (micChanged || std::abs(lvl - lastDrawnMicLevel) > 0.010f)
        {
            lastDrawnMicLevel = lvl;
            needsRepaint = true;
        }
    }
    else
    {
        micTriggerFired = false;
        micTriggerTicks = 0;
    }

    {
        static int lastCamState = -1;
        const int cs = cameraState.load();
        if (cs != lastCamState)
        {
            lastCamState = cs;
            needsRepaint = true;
        }
    }

    if (isCameraRecording)
    {
        if (hasNewWebcamFrame.load())
        {
            hasNewWebcamFrame.store(false);
            needsRepaint = true;
        }
    }

    // 5b. Polling e Aggiornamento MiMO Eye AI Server (Google MediaPipe HandLandmarker su PC)
    if (eyeServer != nullptr)
    {
        if (eyeServer->isMediaPipeActive())
            latestEyeState = eyeServer->getLatestState();
        else
            latestEyeState = smoothedHandState;

        auto pinchTrigs = eyeServer->consumePinchTriggers();
        if (eyeGesturePinchTrig && eyePhalanxDetail >= 1)
        {
            if (!pinchTrigs.empty())
                needsRepaint = true;
        }

        if ((latestEyeState.hasHand && isCameraInCanvas()) || (isCameraRecording && isCameraInCanvas()) || (eyeDrawerOpen && frameCounter % 10 == 0))
            needsRepaint = true;

        if (isMicInCanvas() && isMicRecording && (realMicLevel.load() > 0.005f || realMicVelocity.load() > 0.005f))
            needsRepaint = true;
    }

    // 6. Modulazione Reale Live tramite Cavi Pastello (Microfono, Webcam/Eye, LFO & Modulatori DSP -> Strumenti ed Effetti)
    if (dragMode != DragMode::TurnKnob && dragMode != DragMode::TurnInnerKnob)
    {
        for (const auto& c : cables)
        {
            if (!c.isModulation || (!c.targetPinId.startsWith("param:") && c.targetPinId != "inst:tone"))
                continue;

            float modVal = -1.0f;
            float dspMod = -1.0f;

            // 1. Identifica il nodo sorgente
            const CanvasNode* srcNode = nullptr;
            for (const auto& n : nodes)
            {
                if (n.id == c.sourceNodeId)
                {
                    srcNode = &n;
                    break;
                }
            }

            // 2. Moduli Hardware / Sensori diretti (Webcam / Occhio 3D, Microfono PC, Batteria)
            if (srcNode != nullptr && (srcNode->isIntegratedCamera || srcNode->id == "hw-webcam" || srcNode->id == "hw-eye" ||
                                       srcNode->isIntegratedMic || srcNode->id == "hw-mic" || srcNode->id == "hw-drum"))
            {
                if ((srcNode->isIntegratedMic || srcNode->id == "hw-mic") && isMicRecording)
                {
                    if (c.sourcePinId == "mod:vel")
                        modVal = realMicVelocity.load();
                    else if (c.sourcePinId == "mod:press")
                        modVal = realMicLevel.load();
                }
                else if (srcNode->isIntegratedCamera || srcNode->id == "hw-webcam" || srcNode->id == "hw-eye")
                {
                    const bool dxOn = isHandEnabled(false);
                    const bool sxOn = isHandEnabled(true);

                    const bool hasPrimary = latestEyeState.hasHand;
                    const bool hasSecondary = latestEyeState.hasSecondaryHand;

                    if (hasPrimary || hasSecondary)
                    {
                        // Se una sola mano e' visibile inquadrata dalla camera, usala in modo intelligente
                        // sia per i controlli collegati a DX che a SX (evita disconnessioni dovute a specchiatura)
                        const bool useSecondaryForDx = !hasPrimary && hasSecondary;
                        const bool usePrimaryForSx = !hasSecondary && hasPrimary;

                        const float pX = hasPrimary ? latestEyeState.palmX : (useSecondaryForDx ? latestEyeState.secondaryPalmX : -1.0f);
                        const float pY = hasPrimary ? latestEyeState.palmY : (useSecondaryForDx ? latestEyeState.secondaryPalmY : -1.0f);
                        const float pZ = hasPrimary ? latestEyeState.palmZ : (useSecondaryForDx ? latestEyeState.secondaryPalmZ : -1.0f);
                        const float pFist = hasPrimary ? latestEyeState.fist : (useSecondaryForDx ? latestEyeState.secondaryFist : -1.0f);
                        const float pTilt = hasPrimary ? latestEyeState.handTilt : (useSecondaryForDx ? latestEyeState.secondaryHandTilt : -1.0f);

                        const float sX = hasSecondary ? latestEyeState.secondaryPalmX : (usePrimaryForSx ? latestEyeState.palmX : -1.0f);
                        const float sY = hasSecondary ? latestEyeState.secondaryPalmY : (usePrimaryForSx ? latestEyeState.palmY : -1.0f);
                        const float sZ = hasSecondary ? latestEyeState.secondaryPalmZ : (usePrimaryForSx ? latestEyeState.palmZ : -1.0f);
                        const float sFist = hasSecondary ? latestEyeState.secondaryFist : (usePrimaryForSx ? latestEyeState.fist : -1.0f);
                        const float sTilt = hasSecondary ? latestEyeState.secondaryHandTilt : (usePrimaryForSx ? latestEyeState.handTilt : -1.0f);

                        auto getDxFinger = [&](int idx) -> float {
                            if (hasPrimary) return latestEyeState.fingerFlex[static_cast<size_t>(idx)];
                            if (useSecondaryForDx) return latestEyeState.secondaryFingerFlex[static_cast<size_t>(idx)];
                            return -1.0f;
                        };
                        auto getSxFinger = [&](int idx) -> float {
                            if (hasSecondary) return latestEyeState.secondaryFingerFlex[static_cast<size_t>(idx)];
                            if (usePrimaryForSx) return latestEyeState.fingerFlex[static_cast<size_t>(idx)];
                            return -1.0f;
                        };

                        // Mano Destra (DX)
                        if (dxOn)
                        {
                            if (c.sourcePinId == "mod:dx_x" || c.sourcePinId == "mod:x")
                                modVal = isGroupEnabled("eye_dx_palm") ? pX : -1.0f;
                            else if (c.sourcePinId == "mod:dx_y" || c.sourcePinId == "mod:y")
                                modVal = isGroupEnabled("eye_dx_palm") ? pY : -1.0f;
                            else if (c.sourcePinId == "mod:dx_z" || c.sourcePinId == "mod:z" || c.sourcePinId == "mod:mot")
                                modVal = isGroupEnabled("eye_dx_palm") ? pZ : -1.0f;
                            else if (c.sourcePinId == "mod:dx_fist" || c.sourcePinId == "mod:fist")
                                modVal = isGroupEnabled("eye_dx_palm") ? pFist : -1.0f;
                            else if (c.sourcePinId == "mod:dx_tilt" || c.sourcePinId == "mod:tilt")
                                modVal = isGroupEnabled("eye_dx_palm") ? pTilt : -1.0f;
                            else if (c.sourcePinId == "mod:dx_thumb" || c.sourcePinId == "mod:thumb")
                                modVal = isGroupEnabled("eye_dx_fingers") ? getDxFinger(0) : -1.0f;
                            else if (c.sourcePinId == "mod:dx_idx" || c.sourcePinId == "mod:idx" || c.sourcePinId == "mod:pinch_idx")
                                modVal = isGroupEnabled("eye_dx_fingers") ? getDxFinger(1) : -1.0f;
                            else if (c.sourcePinId == "mod:dx_mid" || c.sourcePinId == "mod:mid" || c.sourcePinId == "mod:pinch_mid")
                                modVal = isGroupEnabled("eye_dx_fingers") ? getDxFinger(2) : -1.0f;
                            else if (c.sourcePinId == "mod:dx_rng" || c.sourcePinId == "mod:rng" || c.sourcePinId == "mod:pinch_rng")
                                modVal = isGroupEnabled("eye_dx_fingers") ? getDxFinger(3) : -1.0f;
                            else if (c.sourcePinId == "mod:dx_pky" || c.sourcePinId == "mod:pky" || c.sourcePinId == "mod:pinch_pky")
                                modVal = isGroupEnabled("eye_dx_fingers") ? getDxFinger(4) : -1.0f;
                        }

                        // Mano Sinistra (SX)
                        if (sxOn && modVal < 0.0f)
                        {
                            if (c.sourcePinId == "mod:sx_x")
                                modVal = isGroupEnabled("eye_sx_palm") ? sX : -1.0f;
                            else if (c.sourcePinId == "mod:sx_y")
                                modVal = isGroupEnabled("eye_sx_palm") ? sY : -1.0f;
                            else if (c.sourcePinId == "mod:sx_z")
                                modVal = isGroupEnabled("eye_sx_palm") ? sZ : -1.0f;
                            else if (c.sourcePinId == "mod:sx_fist")
                                modVal = isGroupEnabled("eye_sx_palm") ? sFist : -1.0f;
                            else if (c.sourcePinId == "mod:sx_tilt")
                                modVal = isGroupEnabled("eye_sx_palm") ? sTilt : -1.0f;
                            else if (c.sourcePinId == "mod:sx_thumb")
                                modVal = isGroupEnabled("eye_sx_fingers") ? getSxFinger(0) : -1.0f;
                            else if (c.sourcePinId == "mod:sx_idx")
                                modVal = isGroupEnabled("eye_sx_fingers") ? getSxFinger(1) : -1.0f;
                            else if (c.sourcePinId == "mod:sx_mid")
                                modVal = isGroupEnabled("eye_sx_fingers") ? getSxFinger(2) : -1.0f;
                            else if (c.sourcePinId == "mod:sx_rng")
                                modVal = isGroupEnabled("eye_sx_fingers") ? getSxFinger(3) : -1.0f;
                            else if (c.sourcePinId == "mod:sx_pky")
                                modVal = isGroupEnabled("eye_sx_fingers") ? getSxFinger(4) : -1.0f;
                        }
                    }
                    else if (isCameraRecording) // Fallback al tracking ottico se nessuna mano e' visibile
                    {
                        const float optX = juce::jlimit(0.0f, 1.0f, (realCameraCentroidX.load() + 1.0f) * 0.5f);
                        const float optY = juce::jlimit(0.0f, 1.0f, 1.0f - (realCameraCentroidY.load() + 1.0f) * 0.5f);
                        float optZ = realCameraMotion.load();
                        if (cameraTrackingMode == CameraTrackingMode::Light)
                            optZ = realCameraBrightness.load();
                        else if (cameraTrackingMode == CameraTrackingMode::Presence)
                            optZ = juce::jlimit(0.0f, 1.0f, 1.0f - std::hypot(realCameraCentroidX.load(), realCameraCentroidY.load()) * 0.65f);

                        const float optFist = realCameraMotion.load();

                        if (dxOn)
                        {
                            if (c.sourcePinId == "mod:dx_x" || c.sourcePinId == "mod:x")
                                modVal = optX;
                            else if (c.sourcePinId == "mod:dx_y" || c.sourcePinId == "mod:y")
                                modVal = optY;
                            else if (c.sourcePinId == "mod:dx_z" || c.sourcePinId == "mod:z" || c.sourcePinId == "mod:mot")
                                modVal = optZ;
                            else if (c.sourcePinId == "mod:dx_fist" || c.sourcePinId == "mod:fist")
                                modVal = optFist;
                            else if (c.sourcePinId == "mod:dx_tilt" || c.sourcePinId == "mod:tilt")
                                modVal = optX;
                            else if (c.sourcePinId.startsWith("mod:dx_"))
                                modVal = optFist;
                        }
                        if (sxOn && modVal < 0.0f)
                        {
                            if (c.sourcePinId == "mod:sx_x")
                                modVal = optX;
                            else if (c.sourcePinId == "mod:sx_y")
                                modVal = optY;
                            else if (c.sourcePinId == "mod:sx_z")
                                modVal = optZ;
                            else if (c.sourcePinId == "mod:sx_fist")
                                modVal = optFist;
                            else if (c.sourcePinId == "mod:sx_tilt")
                                modVal = optX;
                            else if (c.sourcePinId.startsWith("mod:sx_"))
                                modVal = optFist;
                        }
                    }
                }
            }
            else
            {
                // 3. Modulatori DSP interni nel grafo audio (LFO, Follower, ecc.)
                dspMod = audioEngine.getNodeModulationValue(c.sourceNodeId, c.sourcePinId);
                if (dspMod >= 0.0f)
                    modVal = dspMod;
            }

            if (modVal >= 0.0f)
            {
                modVal = juce::jlimit(0.0f, 1.0f, modVal);
                if (c.targetPinId == "inst:tone")
                {
                    // Modulazione diretta di uno Strumento (es. Pitch / Timbro / Trigger dinamico)
                    const float targetHz = 120.0f + modVal * 6800.0f;
                    synthFilterCutoffHz.store(targetHz);
                }
                else
                {
                    const auto paramSpec = c.targetPinId.fromFirstOccurrenceOf("param:", false, false);
                    juce::String blockId;
                    juce::String paramKey;
                    if (paramSpec.contains("."))
                    {
                        blockId = paramSpec.upToFirstOccurrenceOf(".", false, false);
                        paramKey = paramSpec.fromFirstOccurrenceOf(".", false, false);
                    }
                    else
                    {
                        blockId = "";
                        paramKey = paramSpec;
                    }

                    for (auto& dst : nodes)
                    {
                        if (dst.id != c.targetNodeId)
                            continue;

                        // Se e' un Nodo Strumento o Master con directParams
                        for (auto& dp : dst.directParams)
                        {
                            if (dp.key == paramKey)
                            {
                                const float curNorm = dp.getNormalized();
                                const float nextNorm = (dspMod >= 0.0f) ? modVal : (curNorm * 0.35f + modVal * 0.65f);
                                if (std::abs(nextNorm - curNorm) > 0.0005f)
                                {
                                    dp.setFromNormalized(nextNorm);
                                    audioEngine.setModulation(dst.id, dp.key, nextNorm);
                                    needsRepaint = true;
                                }
                            }
                        }

                        // Se e' un NestedEffect con innerBlocks
                        for (auto& b : dst.innerBlocks)
                        {
                            if (!blockId.isEmpty() && blockId != "direct" && b.id != blockId)
                                continue;
                            for (auto& p : b.params)
                            {
                                if (p.key == paramKey)
                                {
                                    const float curNorm = p.getNormalized();
                                    const float nextNorm = (dspMod >= 0.0f) ? modVal : (curNorm * 0.35f + modVal * 0.65f);
                                    if (std::abs(nextNorm - curNorm) > 0.0005f)
                                    {
                                        p.setFromNormalized(nextNorm);
                                        audioEngine.setModulation(dst.id, p.key, nextNorm);
                                        needsRepaint = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Aggiornamento VU-Meter con decadimento analogico fluido
    for (auto& n : nodes)
    {
        const float pk = audioEngine.getNodePeak(n.id);
        const float prev = n.peakLevel;
        n.peakLevel = std::max(pk, prev * 0.82f);
        if (std::abs(n.peakLevel - prev) > 0.012f)
            needsRepaint = true;
    }

    // Controllo 3D continuo per Theremin
    if (latestEyeState.hasHand)
        audioEngine.setHand3D(latestEyeState.palmX, latestEyeState.palmY, latestEyeState.palmZ, true);
    else
        audioEngine.setHand3D(0.5f, 0.5f, 0.5f, false);

    const float pulse = triggerPulseAlpha.load();
    if (pulse > 0.02f)
    {
        triggerPulseAlpha.store(pulse * 0.88f);
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}
void MainComponent::initCatalogs()
{
    instrumentCatalog = {
        { "ins-drum-analog",   "Percussioni",       "Percussioni",          "Faust", "4 voci discrete con sound design granulare", MiMOLookAndFeel::crayonCherry, false, {} },
        { "ins-theremin-3d",   "Sintesi Gestuale",  "Theremin 3D",          "Faust", "Controllo spaziale fluido",                  MiMOLookAndFeel::crayonCyan,   false, {} },
        { "ins-karplus-pluck", "Corde & Lamelle",   "Kalimba & Pluck",      "Faust", "Waveguide fisico a corda",                   MiMOLookAndFeel::crayonMint,   false, {} },
        { "ins-felt-piano",    "Tastiere & Feltro", "Piano Feltro",         "Faust", "Pianoforte intimo con feltro & decay naturale", MiMOLookAndFeel::crayonLilac, false, {} },
        { "ins-synth-va",      "Sintesi VA",        "Virtual Analog Synth", "Faust", "PolyBLEP lead & sub-bass",                  MiMOLookAndFeel::crayonPeach,  false, {} }
    };

    utilityCatalog = {
        { "ut-scale-lock",    "Armonia & Scale",  "Smart Scale Lock",  "Faust", "Quantizzatore tonale anti-stonatura", MiMOLookAndFeel::crayonMint,   false, {} },
        { "ut-looper-tape",   "Performance",      "Tape Looper",       "Faust", "1-Button Looper a nastro (Sound-on-Sound)", MiMOLookAndFeel::crayonCherry, false, {} },
        { "mod-env-follower", "Modulazione",      "Envelope Follower", "Faust", "Seguitore dinamico inviluppo audio", MiMOLookAndFeel::crayonOchre,  false, {} },
        { "mod-lfo-poly",     "Modulazione",      "LFO Multi-Onda",    "Faust", "Oscillatore CV continuo multi-forma", MiMOLookAndFeel::crayonPeach,  false, {} }
    };

    // Catalogo Effetti Standard V2 (Mix Dry/Wet e Volume su TUTTI gli effetti)
    {
        CatalogItem filtro;
        filtro.id = "fx-filtro-svf";
        filtro.category = "Filtri";
        filtro.name = "Filtro ZDF SVF";
        filtro.engine = "Faust";
        filtro.hint = "SVF risonante analogico";
        filtro.colour = MiMOLookAndFeel::crayonCyan;
        filtro.isEffect = true;
        filtro.templateBlocks = {
            { "b0", "SVF Filtro", "process = fi.resonlp(cutoff, q, 1);", MiMOLookAndFeel::crayonCyan, false, { 320.0f, 150.0f },
              { { "cutoff", "Cutoff", 20.0f, 18000.0f, 1800.0f, 1800.0f, " Hz", true },
                { "res",    "Risonanza", 0.1f, 10.0f, 1.4f, 1.4f, "", true } } },
            { "b1", "Mix & Vol", "process = _ <: *(1-mix), *(mix) :> _ * vol;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 160.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(filtro);
    }

    {
        CatalogItem sat;
        sat.id = "fx-saturazione-soft";
        sat.category = "Colore";
        sat.name = "Saturazione Pastello";
        sat.engine = "Faust";
        sat.hint = "Overdrive a nastro morbido";
        sat.colour = MiMOLookAndFeel::crayonPeach;
        sat.isEffect = true;
        sat.templateBlocks = {
            { "b0", "Saturatore", "process = ef.cubicnl(drive, 0);", MiMOLookAndFeel::crayonPeach, false, { 320.0f, 180.0f },
              { { "drive", "Drive", 0.0f, 36.0f, 12.0f, 12.0f, " dB", true },
                { "tone",  "Tono",  400.0f, 16000.0f, 6500.0f, 6500.0f, " Hz", true } } },
            { "b1", "Mix & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 180.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(sat);
    }

    {
        CatalogItem delay;
        delay.id = "fx-eco-nastro";
        delay.category = "Spazio";
        delay.name = "Eco a Nastro Ping-Pong";
        delay.engine = "Faust";
        delay.hint = "Delay analogico con smorzamento";
        delay.colour = MiMOLookAndFeel::crayonMint;
        delay.isEffect = true;
        delay.templateBlocks = {
            { "b0", "Delay a Nastro", "process = +~(de.fdelay(48000, time) * fb);", MiMOLookAndFeel::crayonMint, false, { 320.0f, 170.0f },
              { { "time", "Tempo", 20.0f, 1200.0f, 380.0f, 380.0f, " ms", true },
                { "fb",   "Feedback", 0.0f, 95.0f, 45.0f, 45.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = fi.lowpass(2, tone);", MiMOLookAndFeel::crayonOchre, false, { 620.0f, 170.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 35.0f, 35.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(delay);
    }

    {
        CatalogItem rev;
        rev.id = "fx-riverbero-fdn";
        rev.category = "Spazio";
        rev.name = "Riverbero Lush FDN";
        rev.engine = "Faust";
        rev.hint = "Ambiente diffuso denso";
        rev.colour = MiMOLookAndFeel::crayonLilac;
        rev.isEffect = true;
        rev.templateBlocks = {
            { "b0", "Ambiente", "process = re.mono_freeverb(room, 0.5, damp, 23);", MiMOLookAndFeel::crayonLilac, false, { 320.0f, 170.0f },
              { { "room", "Stanza", 0.0f, 100.0f, 75.0f, 75.0f, " %", true },
                { "damp", "Smorzam.", 0.0f, 100.0f, 40.0f, 40.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = _ <: *(1-mix), *(mix) :> _;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 180.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 35.0f, 35.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(rev);
    }

    {
        CatalogItem moog;
        moog.id = "fx-moog-ladder";
        moog.category = "Filtri";
        moog.name = "Moog Ladder 24dB";
        moog.engine = "Faust";
        moog.hint = "Filtro analogico a scala con transistor drive";
        moog.colour = MiMOLookAndFeel::crayonOchre;
        moog.isEffect = true;
        moog.templateBlocks = {
            { "b0", "Ladder", "process = moog_ladder(cutoff, res, drive);", MiMOLookAndFeel::crayonOchre, false, { 320.0f, 170.0f },
              { { "cutoff", "Cutoff", 20.0f, 18000.0f, 1200.0f, 1200.0f, " Hz", true },
                { "res",    "Risonanza", 0.0f, 100.0f, 45.0f, 45.0f, " %", true } } },
            { "b1", "Drive & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonPeach, false, { 620.0f, 170.0f },
              { { "drive", "Drive", 0.0f, 24.0f, 6.0f, 6.0f, " dB", true },
                { "mix",   "Mix",   0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol",   "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(moog);
    }

    {
        CatalogItem chorus;
        chorus.id = "fx-chorus-bbd";
        chorus.category = "Modulazione";
        chorus.name = "Chorus BBD Analogico";
        chorus.engine = "Faust";
        chorus.hint = "Stereo ensemble a ritardo variabile vintage";
        chorus.colour = MiMOLookAndFeel::crayonSage;
        chorus.isEffect = true;
        chorus.templateBlocks = {
            { "b0", "BBD Ensemble", "process = bbd_chorus(rate, depth);", MiMOLookAndFeel::crayonSage, false, { 320.0f, 170.0f },
              { { "rate",  "Velocita", 0.1f, 8.0f, 1.25f, 1.25f, " Hz", true },
                { "depth", "Profond.", 0.0f, 100.0f, 65.0f, 65.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonMint, false, { 620.0f, 170.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 50.0f, 50.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(chorus);
    }
}

void MainComponent::syncAudioRoutes()
{
    std::vector<mimo::audio::NodeAudioRoute> routes;
    for (const auto& c : cables)
    {
        mimo::audio::NodeAudioRoute r;
        r.sourceNodeId = c.sourceNodeId;
        r.sourcePinId = c.sourcePinId;
        r.targetNodeId = c.targetNodeId;
        r.targetPinId = c.targetPinId;
        r.isModulation = c.isModulation;
        routes.push_back(r);
    }

    std::vector<mimo::audio::NodeGraphDescriptor> descriptors;
    for (const auto& n : nodes)
    {
        mimo::audio::NodeGraphDescriptor d;
        d.id = n.id;
        d.label = n.label;
        d.category = n.subtitle;
        d.engine = n.instrumentEngine;
        d.isUtility = n.id.startsWithIgnoreCase("ut-") || n.id.startsWithIgnoreCase("mod-") ||
                      n.id.containsIgnoreCase("lfo") || n.id.containsIgnoreCase("follower") ||
                      n.id.containsIgnoreCase("scale") || n.id.containsIgnoreCase("looper");
        d.isEffect = (n.type == NodeType::NestedEffect);
        d.isInstrument = !d.isUtility && !d.isEffect && (n.type == NodeType::InstrumentNode || n.hasInstrument);
        d.isBypassed = n.isBypassed;
        descriptors.push_back(d);
    }

    audioEngine.syncGraph(descriptors, routes);
}

void MainComponent::initDefaultStudioGraph()
{
    nodes.clear();
    cables.clear();

    // 1. Base Centrale Rock Pi 4 (Fissa al centro del foglio)
    CanvasNode rockPi;
    rockPi.id = "rockpi";
    rockPi.type = NodeType::RockPiCentral;
    rockPi.label = "MiMO Core";
    rockPi.subtitle = "Rock Pi 4 Plus - Hub I2C + Audio";
    rockPi.pos = { -175.0f, -135.0f };
    rockPi.size = { 350.0f, 218.0f };
    rockPi.crayonColour = MiMOLookAndFeel::crayonOchre;
    nodes.push_back(rockPi);

    // 2. Modulo Periferico P1: Modulo Batteria (Posizionato in basso sotto al connettore P1)
    CanvasNode drum;
    drum.id = "hw-drum";
    drum.type = NodeType::HardwareModule;
    drum.label = "Modulo Batteria";
    drum.subtitle = "Sensori Piezo - I2C Ch1";
    drum.portBadge = "P1";
    drum.pos = { -280.0f, 175.0f };
    drum.size = { 255.0f, 215.0f };
    {
        NodeOutputGroup gTrig;
        gTrig.id = "drum_trig";
        gTrig.name = "Impulsi Trigger (Colpo)";
        gTrig.isExpanded = true;
        gTrig.items = {
            { "trigger:out", "Trigger Colpo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        NodeOutputGroup gMod;
        gMod.id = "drum_mod";
        gMod.name = "Modulazione & Dinamica";
        gMod.isExpanded = true;
        gMod.items = {
            { "mod:vel",   "Velocity",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:press", "Pressione", MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };
        drum.outputGroups = { gTrig, gMod };
        drum.size.y = getNodeCalculatedHeight(drum);
    }
    nodes.push_back(drum);

    // 3. Modulo Periferico P2: Modulo Occhio (Posizionato in basso sotto al connettore P2)
    CanvasNode eye;
    eye.id = "hw-eye";
    eye.type = NodeType::HardwareModule;
    eye.isIntegratedCamera = true;
    eye.label = "Modulo Occhio";
    eye.subtitle = "Tracking Mano 3D - 60 FPS";
    eye.portBadge = "P2";
    eye.pos = { 35.0f, 175.0f };
    eye.size = { 255.0f, 380.0f };
    eye.crayonColour = MiMOLookAndFeel::crayonSage;
    {
        // 1. TOP-LEVEL: MANO DESTRA (DX)
        NodeOutputGroup grpDxRoot;
        grpDxRoot.id = "eye_dx";
        grpDxRoot.name = "Mano Destra (DX)";
        grpDxRoot.parentGroup = "";
        grpDxRoot.isExpanded = true;

        // 1.1 Palmo 3D (DX)
        NodeOutputGroup grpDxPalm;
        grpDxPalm.id = "eye_dx_palm";
        grpDxPalm.name = "Palmo 3D";
        grpDxPalm.parentGroup = "eye_dx";
        grpDxPalm.isExpanded = true;
        grpDxPalm.items = {
            { "mod:dx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:dx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        // 1.2 Dita & Falangi (DX)
        NodeOutputGroup grpDxFingers;
        grpDxFingers.id = "eye_dx_fingers";
        grpDxFingers.name = "Dita & Falangi";
        grpDxFingers.parentGroup = "eye_dx";
        grpDxFingers.isExpanded = false;
        grpDxFingers.items = {
            { "mod:dx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" }
        };

        // 1.3 Trigger Pinch (DX)
        NodeOutputGroup grpDxPinches;
        grpDxPinches.id = "eye_dx_pinches";
        grpDxPinches.name = "Trigger Pinch";
        grpDxPinches.parentGroup = "eye_dx";
        grpDxPinches.isExpanded = false;
        grpDxPinches.items = {
            { "trigger:dx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        // 2. TOP-LEVEL: MANO SINISTRA (SX)
        NodeOutputGroup grpSxRoot;
        grpSxRoot.id = "eye_sx";
        grpSxRoot.name = "Mano Sinistra (SX)";
        grpSxRoot.parentGroup = "";
        grpSxRoot.isExpanded = false;

        // 2.1 Palmo 3D (SX)
        NodeOutputGroup grpSxPalm;
        grpSxPalm.id = "eye_sx_palm";
        grpSxPalm.name = "Palmo 3D";
        grpSxPalm.parentGroup = "eye_sx";
        grpSxPalm.isExpanded = true;
        grpSxPalm.items = {
            { "mod:sx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        // 2.2 Dita & Falangi (SX)
        NodeOutputGroup grpSxFingers;
        grpSxFingers.id = "eye_sx_fingers";
        grpSxFingers.name = "Dita & Falangi";
        grpSxFingers.parentGroup = "eye_sx";
        grpSxFingers.isExpanded = false;
        grpSxFingers.items = {
            { "mod:sx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        // 2.3 Trigger Pinch (SX)
        NodeOutputGroup grpSxPinches;
        grpSxPinches.id = "eye_sx_pinches";
        grpSxPinches.name = "Trigger Pinch";
        grpSxPinches.parentGroup = "eye_sx";
        grpSxPinches.isExpanded = false;
        grpSxPinches.items = {
            { "trigger:sx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        eye.outputGroups = {
            grpDxRoot, grpDxPalm, grpDxFingers, grpDxPinches,
            grpSxRoot, grpSxPalm, grpSxFingers, grpSxPinches
        };
        eye.size.y = getNodeCalculatedHeight(eye);
    }
    nodes.push_back(eye);

    // 4. Effetto Iniziale Nested: Filtro Naif
    CanvasNode filtro = createFxNodeFromCatalog(effectCatalog[0], { 205.0f, -95.0f }, 1);
    nodes.push_back(filtro);

    // 5. Nodo Master Output
    CanvasNode master;
    master.id = "master";
    master.type = NodeType::MasterOut;
    master.label = "MASTER OUT";
    master.subtitle = "ASIO - 64 campioni";
    master.pos = { 525.0f, -80.0f };
    master.size = { 165.0f, 155.0f };
    master.crayonColour = MiMOLookAndFeel::crayonSage;
    master.directParams = {
        { "gain", "Master Vol", -48.0f, 6.0f, 0.0f, 0.0f, " dB", true }
    };
    nodes.push_back(master);

    // Cavi audio iniziali a penna stilografica
    cables.push_back({ "c-drum-pi", "hw-drum", "audio:out", "rockpi",   "audio:p1", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-eye-pi",  "hw-eye",  "audio:out", "rockpi",   "audio:p2", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-pi-fx",   "rockpi",  "audio:out", filtro.id,  "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-fx-mst",  filtro.id, "audio:out", "master",   "audio:in", false, MiMOLookAndFeel::fountainInk, {} });

    for (auto& n : nodes)
    {
        n.targetPos = n.pos;
        n.isAnimatingPos = false;
    }
    syncAudioRoutes();
}

// =============================================================================
void MainComponent::initDefaultLivePresets()
{
    photoAlbum.clear();

    // Preset 1: Batte-Funk 70 & Tape (Hybrid)
    {
        PolaroidItem p;
        p.id = "pol-vintage-beat";
        p.autoName = "Batte-Funk 70 & Tape";
        p.title = "Batte-Funk 70 & Tape";
        p.timestamp = "14:20";
        p.engineType = "hybrid";
        p.bpm = 112;
        p.rotationDeg = -1.2f;
        p.badges = {
            { "Percussioni 808 (Faust)", MiMOLookAndFeel::crayonCherry },
            { "Tape Delay (Faust)", MiMOLookAndFeel::crayonCyan },
            { "Vel -> Drive", MiMOLookAndFeel::crayonOchre }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 2: Theremin Nuvola 3D (Faust)
    {
        PolaroidItem p;
        p.id = "pol-theremin-nuvola";
        p.autoName = "Theremin Nuvola 3D";
        p.title = "Theremin Nuvola 3D";
        p.timestamp = "14:24";
        p.engineType = "faust";
        p.bpm = 96;
        p.rotationDeg = 1.4f;
        p.badges = {
            { "Occhio AI 60 FPS", MiMOLookAndFeel::crayonCyan },
            { "Theremin Faust", MiMOLookAndFeel::crayonMint },
            { "Reverb 3D", MiMOLookAndFeel::crayonLilac }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 3: Piano Feltro Ambient (Faust)
    {
        PolaroidItem p;
        p.id = "pol-piano-feltro";
        p.autoName = "Piano Feltro Ambient";
        p.title = "Piano Feltro Ambient";
        p.timestamp = "14:28";
        p.engineType = "faust";
        p.bpm = 84;
        p.rotationDeg = -0.8f;
        p.badges = {
            { "Piano Feltro (Faust)", MiMOLookAndFeel::crayonLilac },
            { "Riverbero Carta", MiMOLookAndFeel::crayonCherry },
            { "Dist -> Damp", MiMOLookAndFeel::crayonSage }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 4: Percussioni Lofi & Fuzz (Faust)
    {
        PolaroidItem p;
        p.id = "pol-toy-drums";
        p.autoName = "Percussioni Lofi & Fuzz";
        p.title = "Percussioni Lofi & Fuzz";
        p.timestamp = "14:32";
        p.engineType = "faust";
        p.bpm = 126;
        p.rotationDeg = 1.1f;
        p.badges = {
            { "Percussioni Lofi (Faust)", MiMOLookAndFeel::crayonPeach },
            { "Filtro Naif SVF", MiMOLookAndFeel::crayonCyan }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 5: Pad Gessetto Granulare (Faust)
    {
        PolaroidItem p;
        p.id = "pol-pad-gessetto";
        p.autoName = "Pad Gessetto Granulare";
        p.title = "Pad Gessetto Granulare";
        p.timestamp = "14:35";
        p.engineType = "faust";
        p.bpm = 108;
        p.rotationDeg = -1.5f;
        p.badges = {
            { "Pad Gessetto (Faust)", MiMOLookAndFeel::crayonMint },
            { "Delay Spaziale", MiMOLookAndFeel::crayonCyan }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 6: Acid Bass 303 & Sub (Faust)
    {
        PolaroidItem p;
        p.id = "pol-acid-bass";
        p.autoName = "Acid Bass 303 & Sub";
        p.title = "Acid Bass 303 & Sub";
        p.timestamp = "14:40";
        p.engineType = "faust";
        p.bpm = 128;
        p.rotationDeg = 0.9f;
        p.badges = {
            { "TB-303 Acid (Faust)", MiMOLookAndFeel::crayonCherry },
            { "Drive Analog", MiMOLookAndFeel::crayonPeach }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Pre-assegnazione dei primi 3 slot della Dock Hardware
    sceneSlots[0] = photoAlbum[0];
    sceneSlots[1] = photoAlbum[1];
    sceneSlots[2] = photoAlbum[2];
    sceneSlots[3].isValid = false;
    sceneSlots[4].isValid = false;
    activeSceneSlot = 0;
}

void MainComponent::toggleLooper()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (looperState == LooperState::Idle)
    {
        looperStartTime = now;
        looperState = LooperState::Recording;
    }
    else if (looperState == LooperState::Recording)
    {
        looperDurationSec = std::max(0.5, now - looperStartTime);
        looperStartTime = now;
        looperState = LooperState::Playing;
    }
    else if (looperState == LooperState::Playing)
    {
        looperState = LooperState::Overdubbing;
    }
    else if (looperState == LooperState::Overdubbing)
    {
        looperState = LooperState::Playing;
    }
    repaint();
}

void MainComponent::clearLooper()
{
    looperState = LooperState::Idle;
    looperSweepProgress = 0.0f;
    repaint();
}

void MainComponent::triggerSceneSlot(int slotIdx)
{
    if (slotIdx >= 0 && slotIdx < 5)
    {
        activeSceneSlot = slotIdx;
        repaint();
    }
}

void MainComponent::insertEffectOnCable(const CatalogItem& fx, const juce::String& cableId, juce::Point<float> worldPos)
{
    // 1. Individua il cavo audio da dividere
    CanvasCable orig;
    bool foundCable = false;
    for (auto it = cables.begin(); it != cables.end(); ++it)
    {
        if (it->id == cableId)
        {
            orig = *it;
            cables.erase(it);
            foundCable = true;
            break;
        }
    }

    if (!foundCable)
        return;

    // 2. Identifica i nodi prima e dopo l'inserimento
    CanvasNode* sourceNode = nullptr;
    CanvasNode* targetNode = nullptr;
    for (auto& n : nodes)
    {
        if (n.id == orig.sourceNodeId) sourceNode = &n;
        if (n.id == orig.targetNodeId) targetNode = &n;
    }

    if (sourceNode == nullptr || targetNode == nullptr)
        return;

    const float spacing = 50.0f;
    const float effectWidth = 260.0f;
    const float shiftAmount = effectWidth + spacing; // 310 px

    // Trova la posizione della Base Centrale Rock Pi (MIMO)
    juce::Point<float> rockPiPos { -145.0f, -150.0f };
    juce::Point<float> rockPiSize { 290.0f, 260.0f };
    for (const auto& n : nodes)
    {
        if (n.id == "rockpi")
        {
            rockPiPos = n.pos;
            rockPiSize = n.size;
            break;
        }
    }

    // 3. Determina se il cavo e' VERTICALE (dal basso verso l'alto: modulo hardware -> MIMO)
    //    oppure ORIZZONTALE (da sinistra a destra / laterale: MIMO -> Master Out)
    bool isVerticalCable = false;
    if (orig.targetPinId.startsWith("audio:p") && orig.targetNodeId == "rockpi")
    {
        isVerticalCable = true;
    }
    else if (sourceNode->type == NodeType::HardwareModule || (sourceNode->type == NodeType::NestedEffect && sourceNode->isVerticalFlow))
    {
        isVerticalCable = true;
    }
    else
    {
        const auto p1 = getPinWorldPos(orig.sourceNodeId, orig.sourcePinId);
        const auto p2 = getPinWorldPos(orig.targetNodeId, orig.targetPinId);
        if (std::abs(p2.y - p1.y) > std::abs(p2.x - p1.x) * 1.1f && (p1.y > rockPiPos.y || p2.y > rockPiPos.y))
            isVerticalCable = true;
    }

    juce::Point<float> insertPos;

    if (isVerticalCable)
    {
        // =====================================================================
        // RAMO VERTICALE (DAL BASSO VERSO L'ALTO):
        // MIMO resta rigorosamente FISSO nella sua posizione.
        // L'effetto si inserisce verticalmente tra il modulo e MIMO (o altro effetto),
        // e tutti i moduli/effetti sottostanti si spostano PIÙ IN BASSO (targetPos.y)!
        // =====================================================================
        const float effectHeight = 165.0f;
        const float spacingY = 45.0f;
        const float shiftAmountY = effectHeight + spacingY; // 210 px

        insertPos.x = sourceNode->pos.x;
        if (targetNode->id == "rockpi")
        {
            insertPos.y = (rockPiPos.y + rockPiSize.y) + 40.0f;
        }
        else
        {
            insertPos.y = targetNode->pos.y + targetNode->size.y + 40.0f;
        }

        // Sposta verso il basso il nodo sorgente e qualsiasi modulo sottostante nella stessa colonna
        for (auto& n : nodes)
        {
            if (n.id == "rockpi")
                continue; // MIMO e' rigorosamente fissa!

            if (std::abs(n.pos.x - sourceNode->pos.x) < 120.0f && n.pos.y >= insertPos.y - 10.0f)
            {
                n.targetPos.y = n.pos.y + shiftAmountY;
                n.isAnimatingPos = true;
            }
        }

        // Crea il nuovo nodo effetto con orientamento verticale (isVertical = true)
        CanvasNode newNode = createFxNodeFromCatalog(fx, insertPos, fxCounter++, true);
        newNode.targetPos = insertPos;
        newNode.isAnimatingPos = false;
        nodes.push_back(newNode);

        // Ricollega i cavi audio: Source (basso) -> NewNode:audio:in (basso) | NewNode:audio:out (alto) -> Target (alto)
        cables.push_back({ "c-" + newNode.id + "-in",  orig.sourceNodeId, orig.sourcePinId, newNode.id, "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
        cables.push_back({ "c-" + newNode.id + "-out", newNode.id, "audio:out", orig.targetNodeId, orig.targetPinId, false, MiMOLookAndFeel::fountainInk, {} });
    }
    else
    {
        // =====================================================================
        // RAMO ORIZZONTALE (DA DESTRA A SINISTRA / LATERALE):
        // Mantiene l'allargamento laterale (targetPos.x)
        // =====================================================================
        const float spacing = 50.0f;
        const float effectWidth = 260.0f;
        const float shiftAmount = effectWidth + spacing; // 310 px

        const bool isLeftSide = (targetNode->id == "rockpi" || (sourceNode->pos.x < rockPiPos.x && targetNode->pos.x <= rockPiPos.x));

        if (isLeftSide)
        {
            insertPos.x = targetNode->pos.x - spacing - effectWidth;
            insertPos.y = sourceNode->pos.y + (sourceNode->size.y - 165.0f) * 0.5f;

            for (auto& n : nodes)
            {
                if (n.id == "rockpi" || n.pos.x >= rockPiPos.x)
                    continue;

                if (n.pos.x <= sourceNode->pos.x + 10.0f)
                {
                    n.targetPos.x = n.pos.x - shiftAmount;
                    n.isAnimatingPos = true;
                }
            }
        }
        else
        {
            if (sourceNode->id == "rockpi")
            {
                insertPos.x = sourceNode->pos.x + sourceNode->size.x + spacing;
                insertPos.y = -95.0f;
            }
            else
            {
                insertPos.x = sourceNode->pos.x + sourceNode->size.x + spacing;
                insertPos.y = sourceNode->pos.y;
            }

            for (auto& n : nodes)
            {
                if (n.id == "rockpi" || n.pos.x <= rockPiPos.x)
                    continue;

                if (n.pos.x >= targetNode->pos.x - 10.0f)
                {
                    n.targetPos.x = n.pos.x + shiftAmount;
                    n.isAnimatingPos = true;
                }
            }
        }

        // Crea il nuovo nodo effetto con orientamento orizzontale (isVertical = false)
        CanvasNode newNode = createFxNodeFromCatalog(fx, insertPos, fxCounter++, false);
        newNode.targetPos = insertPos;
        newNode.isAnimatingPos = false;
        nodes.push_back(newNode);

        // Ricollega i cavi audio: Source -> NewNode -> Target
        cables.push_back({ "c-" + newNode.id + "-in",  orig.sourceNodeId, orig.sourcePinId, newNode.id, "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
        cables.push_back({ "c-" + newNode.id + "-out", newNode.id, "audio:out", orig.targetNodeId, orig.targetPinId, false, MiMOLookAndFeel::fountainInk, {} });
    }

    // 6. Avvia l'animazione fluida (60 FPS) per mostrare lo scorrimento degli elementi
    startTimerHz(60);
    repaint();
}

void MainComponent::deleteNode(const juce::String& nodeId)
{
    if (nodeId.isEmpty() || nodeId == "rockpi" || nodeId == "master")
        return;

    auto it = std::find_if(nodes.begin(), nodes.end(), [&](const CanvasNode& n) { return n.id == nodeId; });
    if (it == nodes.end())
        return;

    const auto nodeToDelete = *it;

    // Se è un modulo hardware periferico o integrato, rimuovilo correttamente
    if (nodeToDelete.id == "hw-drum")
    {
        toggleDrumInCanvas();
        if (selectedNodeId == nodeId)
            selectedNodeId.clear();
        syncAudioRoutes();
        repaint();
        return;
    }
    if (nodeToDelete.isIntegratedMic || nodeToDelete.id == "hw-mic")
    {
        toggleMicInCanvas();
        if (selectedNodeId == nodeId)
            selectedNodeId.clear();
        syncAudioRoutes();
        repaint();
        return;
    }
    if (nodeToDelete.isIntegratedCamera || nodeToDelete.id == "hw-webcam" || nodeToDelete.id == "hw-eye")
    {
        toggleCameraInCanvas();
        if (selectedNodeId == nodeId)
            selectedNodeId.clear();
        syncAudioRoutes();
        repaint();
        return;
    }

    // Se è un effetto nested collegato a monte e a valle, prova a ricucire il flusso audio (bypass)
    if (nodeToDelete.type == NodeType::NestedEffect)
    {
        juce::String prevSourceNode, prevSourcePin;
        juce::String nextTargetNode, nextTargetPin;

        for (const auto& c : cables)
        {
            if (!c.isModulation)
            {
                if (c.targetNodeId == nodeId && c.targetPinId == "audio:in")
                {
                    prevSourceNode = c.sourceNodeId;
                    prevSourcePin = c.sourcePinId;
                }
                else if (c.sourceNodeId == nodeId && c.sourcePinId == "audio:out")
                {
                    nextTargetNode = c.targetNodeId;
                    nextTargetPin = c.targetPinId;
                }
            }
        }

        cables.erase(std::remove_if(cables.begin(), cables.end(),
            [&](const CanvasCable& c) { return c.sourceNodeId == nodeId || c.targetNodeId == nodeId; }),
            cables.end());

        if (prevSourceNode.isNotEmpty() && nextTargetNode.isNotEmpty())
        {
            cables.push_back({
                "c-bypass-" + juce::String(juce::Time::getMillisecondCounter()),
                prevSourceNode, prevSourcePin,
                nextTargetNode, nextTargetPin,
                false, MiMOLookAndFeel::fountainInk, {}
            });
        }
    }
    else
    {
        // Per strumenti, moduli hardware o altri blocchi: rimuovi tutti i cavi collegati
        cables.erase(std::remove_if(cables.begin(), cables.end(),
            [&](const CanvasCable& c) { return c.sourceNodeId == nodeId || c.targetNodeId == nodeId; }),
            cables.end());
    }

    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
        [&](const CanvasNode& n) { return n.id == nodeId; }),
        nodes.end());

    if (selectedNodeId == nodeId)
        selectedNodeId.clear();

    syncAudioRoutes();
    repaint();
}

void MainComponent::deleteSelectedEffect()
{
    if (selectedNodeId.isNotEmpty())
        deleteNode(selectedNodeId);
}

// =============================================================================
// Rettangoli UI in Coordinate Schermo
// =============================================================================

juce::Rectangle<float> MainComponent::getSidebarToggleBounds() const
{
    if (sidebarOpen)
        return { static_cast<float>(getWidth() - 300), 16.0f, 32.0f, 32.0f };
    return { static_cast<float>(getWidth() - 44), 16.0f, 32.0f, 32.0f };
}

juce::Rectangle<float> MainComponent::getCameraButtonBounds() const
{
    // Agganciato a sinistra subito a fianco del quadratino con la freccia
    const float x = static_cast<float>(getWidth() - 300) + 38.0f;
    const float w = 284.0f - 38.0f - 38.0f; // 208 px
    return { x, 16.0f, w, 32.0f };
}

juce::Rectangle<float> MainComponent::getSettingsButtonBounds() const
{
    // Quadratino ingranaggio a destra
    return { static_cast<float>(getWidth() - 48), 16.0f, 32.0f, 32.0f };
}

juce::Rectangle<float> MainComponent::getSettingsPanelBounds() const
{
    return { static_cast<float>(getWidth() - 316), 54.0f, 300.0f, 236.0f };
}

juce::Rectangle<float> MainComponent::getSettingsVolumeTrackBounds() const
{
    const auto p = getSettingsPanelBounds();
    return { p.getX() + 14.0f, p.getY() + 102.0f, p.getWidth() - 28.0f, 20.0f };
}

juce::Rectangle<float> MainComponent::getSettingsVolDownBtnBounds() const
{
    const auto p = getSettingsPanelBounds();
    return { p.getX() + 14.0f, p.getY() + 130.0f, 54.0f, 26.0f };
}

juce::Rectangle<float> MainComponent::getSettingsVol100BtnBounds() const
{
    const auto p = getSettingsPanelBounds();
    return { p.getX() + 74.0f, p.getY() + 130.0f, 68.0f, 26.0f };
}

juce::Rectangle<float> MainComponent::getSettingsVol150BtnBounds() const
{
    const auto p = getSettingsPanelBounds();
    return { p.getX() + 148.0f, p.getY() + 130.0f, 78.0f, 26.0f };
}

juce::Rectangle<float> MainComponent::getSettingsVolUpBtnBounds() const
{
    const auto p = getSettingsPanelBounds();
    return { p.getX() + 232.0f, p.getY() + 130.0f, 54.0f, 26.0f };
}

juce::String MainComponent::getSoundNameForId(int soundId)
{
    switch (soundId)
    {
        case 0: return "Cassa";
        case 1: return "Rullante";
        case 2: return "Charleston";
        case 3: return "Percussione";
        case 4: return "Piano Do3";
        case 5: return "Piano Re3";
        case 6: return "Piano Mi3";
        case 7: return "Piano Sol3";
        case 8: return "Piano La3";
        case 9: return "Piano Do4";
        case 10: return "Synth Lead";
        case 11: return "Pad Gessetto";
        case 12: return "808 Sub";
        case 13: return "Latta Giocattolo";
        case 14: return "Carillon";
        default: return "Voce " + juce::String(soundId);
    }
}

void MainComponent::setAppMasterVolume(float newVolume)
{
    const float clamped = juce::jlimit(0.0f, 2.0f, newVolume);
    appMasterVolume.store(clamped);
    audioEngine.setMasterVolume(clamped);
    synthMasterGain.store(clamped);

    for (auto& n : nodes)
    {
        if (n.type == NodeType::MasterOut && !n.directParams.empty())
        {
            n.directParams[0].value = juce::jlimit(-48.0f, 6.0f, juce::Decibels::gainToDecibels(clamped, -48.0f));
            break;
        }
    }
    repaint();
}

void MainComponent::saveSettings()
{
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MiMOHub");
    dir.createDirectory();
    auto f = dir.getChildFile("settings.txt");
    f.replaceWithText("volume=" + juce::String(appMasterVolume.load(), 3) + "\ndarkMode=" + juce::String(isDarkMode ? 1 : 0));
}

void MainComponent::loadSettings()
{
    const auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MiMOHub").getChildFile("settings.txt");
    if (f.existsAsFile())
    {
        const auto lines = juce::StringArray::fromLines(f.loadFileAsString());
        for (const auto& l : lines)
        {
            if (l.startsWithIgnoreCase("volume="))
            {
                const float v = l.fromFirstOccurrenceOf("volume=", false, false).getFloatValue();
                if (v >= 0.05f && v <= 2.0f)
                    appMasterVolume.store(v);
            }
            else if (l.startsWithIgnoreCase("darkMode="))
            {
                isDarkMode = (l.fromFirstOccurrenceOf("darkMode=", false, false).getIntValue() != 0);
                MiMOLookAndFeel::setDarkMode(isDarkMode);
            }
        }
    }
}

bool MainComponent::detectSystemDarkMode()
{
   #if JUCE_WINDOWS
    const juce::String regKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\AppsUseLightTheme";
    if (juce::WindowsRegistry::valueExists(regKey))
    {
        const juce::String val = juce::WindowsRegistry::getValue(regKey, "0");
        return (val.getIntValue() == 0); // 0 = Dark Mode di sistema, 1 = Light Mode
    }
   #endif

    if (juce::Desktop::getInstance().isDarkModeActive())
        return true;

    // Default per MiMO Hub: true (Modalità Notte)
    return true;
}

void MainComponent::darkModeSettingChanged()
{
    setDarkMode(detectSystemDarkMode());
}

void MainComponent::setDarkMode(bool dark)
{
    isDarkMode = dark;
    MiMOLookAndFeel::setDarkMode(isDarkMode);
    paperTextureCache = juce::Image(); // Rigenera cache texture carta con la tavolozza aggiornata
    repaint();
}

void MainComponent::toggleDarkMode()
{
    setDarkMode(!isDarkMode);
}

juce::Rectangle<float> MainComponent::getIntegratedHeaderBounds() const
{
    const float x = static_cast<float>(getWidth() - 300);
    const float y = 58.0f - sidebarScrollY; // Offset dinamico di scorrimento verticale
    return { x, y, 284.0f, 32.0f };
}

float MainComponent::getSidebarContentHeight() const
{
    float h = 0.0f;
    // 0. Moduli integrati
    h += 32.0f + 4.0f + (integratedSectionOpen ? (cameraSettingsOpen ? 300.0f : 156.0f) : 0.0f) + 10.0f;
    // 1. Strumenti
    h += 32.0f + 4.0f + (instrumentsSectionOpen ? (static_cast<float>(instrumentCatalog.size()) * 40.0f + 8.0f) : 0.0f) + 10.0f;
    // 2. Effetti
    h += 32.0f + 4.0f + (effectsSectionOpen ? (static_cast<float>(effectCatalog.size()) * 42.0f + 8.0f) : 0.0f) + 10.0f;
    // 3. Utility
    h += 32.0f + 4.0f + (utilitiesSectionOpen ? (static_cast<float>(utilityCatalog.size()) * 40.0f + 8.0f) : 0.0f) + 24.0f;
    return h;
}

float MainComponent::getSidebarMaxScroll() const
{
    const float viewportH = static_cast<float>(getHeight()) - 58.0f - 16.0f;
    return std::max(0.0f, getSidebarContentHeight() - viewportH);
}

juce::Rectangle<float> MainComponent::getSidebarViewportBounds() const
{
    const float x = static_cast<float>(getWidth() - 308);
    const float y = 54.0f;
    const float w = 308.0f;
    const float h = std::max(10.0f, static_cast<float>(getHeight()) - 54.0f);
    return { x, y, w, h };
}

juce::Rectangle<float> MainComponent::getIntegratedBodyBounds() const
{
    const auto header = getIntegratedHeaderBounds();
    const float h = cameraSettingsOpen ? 300.0f : 156.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> MainComponent::getDrumSwitchBounds() const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float drumY = body.getY() + 6.0f;
    return { cardX + cardW - 74.0f, drumY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> MainComponent::getMicSwitchBounds() const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float micY = body.getY() + 54.0f;
    return { cardX + cardW - 74.0f, micY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> MainComponent::getMicSliderBounds() const
{
    return {};
}

juce::Rectangle<float> MainComponent::getCameraSwitchBounds() const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + cardW - 74.0f, camY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> MainComponent::getCameraSettingsButtonBounds() const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + cardW - 74.0f, camY + 24.0f, 68.0f, 16.0f };
}

juce::Rectangle<float> MainComponent::getCameraSliderBounds() const
{
    return {};
}

juce::Rectangle<float> MainComponent::getCameraModePillBounds(int modeIdx) const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float camY = body.getY() + 102.0f;
    if (modeIdx == 0) // Movimento
        return { cardX + 66.0f,  camY + 46.0f, 62.0f, 18.0f };
    if (modeIdx == 1) // Luce
        return { cardX + 132.0f, camY + 46.0f, 44.0f, 18.0f };
    // Presenza
    return { cardX + 180.0f, camY + 46.0f, 78.0f, 18.0f };
}

juce::Rectangle<float> MainComponent::getCameraMirrorCheckboxBounds() const
{
    const auto body = getIntegratedBodyBounds();
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + 10.0f, camY + 70.0f, cardW - 20.0f, 16.0f };
}

juce::Rectangle<float> MainComponent::getInstrumentsHeaderBounds() const
{
    const float x = static_cast<float>(getWidth() - 300);
    const auto integH = getIntegratedHeaderBounds();
    float y = integH.getBottom();
    if (integratedSectionOpen)
    {
        const auto integB = getIntegratedBodyBounds();
        y = integB.getBottom();
    }
    y += 10.0f;
    return { x, y, 284.0f, 32.0f };
}

juce::Rectangle<float> MainComponent::getInstrumentsBodyBounds() const
{
    const auto header = getInstrumentsHeaderBounds();
    const float count = static_cast<float>(instrumentCatalog.size());
    const float h = count * 40.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> MainComponent::getEffectsHeaderBounds() const
{
    const auto instH = getInstrumentsHeaderBounds();
    float y = instH.getBottom();
    if (instrumentsSectionOpen)
    {
        const auto instB = getInstrumentsBodyBounds();
        y = instB.getBottom();
    }
    y += 10.0f;
    return { instH.getX(), y, 284.0f, 32.0f };
}

juce::Rectangle<float> MainComponent::getEffectsBodyBounds() const
{
    const auto header = getEffectsHeaderBounds();
    const float count = static_cast<float>(effectCatalog.size());
    const float h = count * 42.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> MainComponent::getUtilitiesHeaderBounds() const
{
    const auto fxH = getEffectsHeaderBounds();
    float y = fxH.getBottom();
    if (effectsSectionOpen)
    {
        const auto fxB = getEffectsBodyBounds();
        y = fxB.getBottom();
    }
    y += 10.0f;
    return { fxH.getX(), y, 284.0f, 32.0f };
}

juce::Rectangle<float> MainComponent::getUtilitiesBodyBounds() const
{
    const auto header = getUtilitiesHeaderBounds();
    const float count = static_cast<float>(utilityCatalog.size());
    const float h = count * 40.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> MainComponent::getSlotBounds(int slotIdx) const
{
    if (activeTab == ActiveTab::Live)
    {
        // Nel Workspace LIVE: 5 Grandi Slot Rettangolari affiancati nella Dock Inferiore
        const float totalW = static_cast<float>(getWidth() - 170); // Riserva spazio a destra per il Looper
        const float startX = 24.0f;
        const float gap = 12.0f;
        const float slotW = (totalW - 4.0f * gap) / 5.0f;
        const float y = static_cast<float>(getHeight() - 250);
        const float slotH = 186.0f;
        return { startX + static_cast<float>(slotIdx) * (slotW + gap), y, slotW, slotH };
    }

    const float totalW = std::min(880.0f, static_cast<float>(getWidth() - 40));
    const float startX = (static_cast<float>(getWidth()) - totalW) * 0.5f;
    const float slotW = (totalW - 4.0f * 10.0f) / 5.0f;
    const float y = static_cast<float>(getHeight() - 108);
    return { startX + static_cast<float>(slotIdx) * (slotW + 10.0f), y, slotW, 48.0f };
}

juce::Rectangle<float> MainComponent::getLooperBounds() const
{
    if (activeTab == ActiveTab::Live)
    {
        const float x = static_cast<float>(getWidth() - 142);
        const float y = static_cast<float>(getHeight() - 250);
        return { x, y, 118.0f, 186.0f };
    }
    return { static_cast<float>(getWidth()) * 0.5f - 230.0f, static_cast<float>(getHeight() - 166), 460.0f, 46.0f };
}

juce::Rectangle<float> MainComponent::getDockTabBounds(int tabIdx) const
{
    const float dockW = 450.0f;
    const float startX = (static_cast<float>(getWidth()) - dockW) * 0.5f + 10.0f;
    const float tabW = (dockW - 20.0f) / 3.0f;
    const float y = static_cast<float>(getHeight() - 48);
    return { startX + static_cast<float>(tabIdx) * tabW, y, tabW - 6.0f, 34.0f };
}

juce::Rectangle<float> MainComponent::getPolaroidCardBounds() const
{
    return { static_cast<float>(getWidth()) * 0.5f - 175.0f,
             static_cast<float>(getHeight()) * 0.5f - 210.0f,
             350.0f, 390.0f };
}

// =============================================================================
// Sistema Fotocamera "Polaroid"
// =============================================================================
void MainComponent::triggerSnapPolaroid()
{
    PolaroidItem pol;
    pol.id = "pol-" + juce::String(juce::Time::getMillisecondCounter());
    pol.timestamp = juce::Time::getCurrentTime().formatted("%H:%M");
    pol.nodeCount = static_cast<int>(nodes.size());
    pol.cableCount = static_cast<int>(cables.size());
    pol.modCount = 0;
    for (const auto& c : cables)
        if (c.isModulation)
            ++pol.modCount;

    juce::StringArray parts;
    for (const auto& n : nodes)
    {
        if (n.type == NodeType::HardwareModule && n.hasInstrument)
        {
            parts.add(n.instrumentName.upToFirstOccurrenceOf(" ", false, false));
            pol.badges.push_back({ n.instrumentName, n.crayonColour });
        }
        else if (n.type == NodeType::NestedEffect)
        {
            parts.add(n.label.upToFirstOccurrenceOf(" ", false, false));
            pol.badges.push_back({ n.label, n.crayonColour });
        }
    }

    if (parts.isEmpty())
        pol.autoName = "Scena Studio Naif";
    else
        pol.autoName = parts.joinIntoString(" + ");

    pol.title = pol.autoName;
    pol.snapshot = { nodes, cables };
    pol.isValid = true;

    // Cattura grafica reale del canvas attuale
    const juce::Rectangle<int> captureArea(20, 70, std::max(200, getWidth() - 340), std::max(160, getHeight() - 190));
    pol.previewImage = createComponentSnapshot(captureArea, true, 0.5f);

    pendingPolaroid = pol;
    hasPendingPolaroid = true;
    polaroidHasTyped = false;
    polaroidCountdownTicks = 18; // 3 secondi a 6 Hz
    grabKeyboardFocus();
    repaint();
}

void MainComponent::commitPolaroidToAlbum(const PolaroidItem& item)
{
    PolaroidItem saved = item;
    if (saved.title.trim().isEmpty())
        saved.title = saved.autoName;

    bool exists = false;
    for (auto& existing : photoAlbum)
    {
        if (existing.id == saved.id)
        {
            existing = saved;
            exists = true;
            break;
        }
    }
    if (!exists)
        photoAlbum.insert(photoAlbum.begin(), saved);

    hasPendingPolaroid = false;
    repaint();
}

void MainComponent::assignPolaroidToSlot(int slotIdx, const PolaroidItem& item)
{
    if (slotIdx < 0 || slotIdx >= 5)
        return;

    const auto oldItem = sceneSlots[slotIdx];

    PolaroidItem saved = item;
    if (saved.title.trim().isEmpty())
        saved.title = saved.autoName;
    saved.isValid = true;

    sceneSlots[slotIdx] = saved;
    activeSceneSlot = slotIdx;
    snappedSlotIndex = slotIdx;
    snapAnimScale = 1.18f;

    // Sostituzione Intuitiva: se c'era una vecchia Polaroid diversa, torna dolcemente nell'Album
    if (oldItem.isValid && oldItem.id != saved.id)
    {
        bool inAlbum = false;
        for (const auto& p : photoAlbum)
        {
            if (p.id == oldItem.id) { inAlbum = true; break; }
        }
        if (!inAlbum)
            photoAlbum.insert(photoAlbum.begin(), oldItem);
    }

    commitPolaroidToAlbum(saved);
}

// =============================================================================
// Rendering Principale (paint)
// =============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // 1. Carta ruvida da disegno su tutto lo schermo (nessuna griglia!) - con cache bitmap ad alte prestazioni
    MiMOLookAndFeel::drawPaperTexture(g, getLocalBounds(), paperTextureCache);

    // 2. Vista principale in base al Tab selezionato nel Dock
    if (activeTab == ActiveTab::Studio)
    {
        paintStudioCanvas(g);
        paintRightSidebar(g);
    }
    else if (activeTab == ActiveTab::Live)
    {
        paintLiveAlbumView(g);
    }
    else
    {
        paintCommunityView(g);
    }

    // 3. Banner Fluttuante Quick-Edit (se siamo tornati in Studio per una modifica d'emergenza da doppio clic su uno slot)
    if (editingSlotIndex >= 0)
    {
        juce::Rectangle<float> saveBtn(static_cast<float>(getWidth()) * 0.5f - 195.0f, 16.0f, 390.0f, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, saveBtn, 99, MiMOLookAndFeel::crayonSage, 0.90f,
                                       MiMOLookAndFeel::fountainInk, 2.4f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("[OK] TORNA AL LIVE  //  Scena S" + juce::String(editingSlotIndex + 1) + " Salvata",
                   saveBtn.toNearestInt(), juce::Justification::centred);
    }

    // 4. Editor Fullscreen Nodo Nested (se aperto con doppio clic)
    if (expandedNodeId.isNotEmpty())
    {
        for (const auto& n : nodes)
        {
            if (n.id == expandedNodeId)
            {
                paintNestedModal(g, n);
                break;
            }
        }
    }

    // 5. Overlay Polaroid appena scattata
    if (hasPendingPolaroid)
    {
        paintPolaroidOverlay(g);
    }

    // 6. Fantasma di trascinamento (Sidebar Item o Polaroid)
    if (dragMode == DragMode::DragSidebarItem)
    {
        juce::Rectangle<float> ghost(currentMousePos.x - 85.0f, currentMousePos.y - 22.0f, 170.0f, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, ghost, 777, draggedCatalogItem.colour, 0.85f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(draggedCatalogItem.name, ghost.toNearestInt(), juce::Justification::centred);
    }
    else if (dragMode == DragMode::DragPolaroid)
    {
        juce::Rectangle<float> ghost(currentMousePos.x - 70.0f, currentMousePos.y - 45.0f, 140.0f, 90.0f);
        MiMOLookAndFeel::drawSketchBox(g, ghost, 778, MiMOLookAndFeel::crayonOchre, 0.65f,
                                       MiMOLookAndFeel::crayonOchre, 2.0f, false, MiMOLookAndFeel::polaroidWhite);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(draggedPolaroid.title, ghost.toNearestInt(), juce::Justification::centred);
    }

    // 7. Barra Inferiore: 5 Slot Scena + Dock Navigazione (Home/Studio, Live, Community)
    paintBottomDockAndSlots(g);

    // 8. Finestra Modale MiMO Eye AI Controller (Coerente con Giorno/Notte)
    if (eyeDrawerOpen)
    {
        drawModernFlatEyeModal(g);
    }
}

void MainComponent::paintStudioCanvas(juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(canvasZoom).translated(canvasOffset.x, canvasOffset.y));

    const auto allPins = collectAllPins();
    auto findPinInfo = [&allPins](const juce::String& nId, const juce::String& pId, juce::Point<float>& outPos, juce::Point<float>& outNorm)
    {
        for (const auto& p : allPins)
        {
            if (p.nodeId == nId && p.pinId == pId)
            {
                outPos = p.worldPos;
                outNorm = p.normal;
                return;
            }
        }
        outPos = {};
        outNorm = {};
    };

    // 1. Disegna tutti i Cavi con evidenziazione selettiva del modulo che ha scattato il colpo
    const int trigSrc = lastTriggerSourceCode.load();
    const float pulseVal = triggerPulseAlpha.load();
    const bool isPulse = (pulseVal > 0.08f);

    for (size_t i = 0; i < cables.size(); ++i)
    {
        const auto& c = cables[i];
        juce::Point<float> p1, p2, n1, n2;
        findPinInfo(c.sourceNodeId, c.sourcePinId, p1, n1);
        findPinInfo(c.targetNodeId, c.targetPinId, p2, n2);
        const bool isHover = (c.id == hoveredAudioCableId || c.id == selectedCableId);
        const bool isTrigCable = (c.isTrigger || c.sourcePinId.startsWith("trigger:") || c.targetPinId.startsWith("trigger:"));

        bool cableMatches = false;
        if (isPulse)
        {
            if (trigSrc == 1) // hw-drum
            {
                if (c.sourceNodeId == "hw-drum" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p1"))
                    cableMatches = true;
            }
            else if (trigSrc == 2) // hw-mic
            {
                if (c.sourceNodeId == "hw-mic" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p4"))
                    cableMatches = true;
            }
            else if (trigSrc == 3) // hw-eye
            {
                if (c.sourceNodeId == "hw-eye" || c.sourceNodeId == "hw-webcam" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p2"))
                {
                    const int fIdx = lastTriggerFingerIdx.load();
                    const bool isSec = lastTriggerIsSecondary.load();
                    if (fIdx >= 0)
                    {
                        if (!isSec)
                        {
                            if (fIdx == 0 && (c.sourcePinId == "trigger:dx_pinch_idx" || c.sourcePinId == "trigger:out")) cableMatches = true;
                            else if (fIdx == 1 && c.sourcePinId == "trigger:dx_pinch_mid") cableMatches = true;
                            else if (fIdx == 2 && c.sourcePinId == "trigger:dx_pinch_rng") cableMatches = true;
                            else if (fIdx == 3 && c.sourcePinId == "trigger:dx_pinch_pky") cableMatches = true;
                        }
                        else
                        {
                            if (fIdx == 0 && (c.sourcePinId == "trigger:sx_pinch_idx" || c.sourcePinId == "trigger:out")) cableMatches = true;
                            else if (fIdx == 1 && c.sourcePinId == "trigger:sx_pinch_mid") cableMatches = true;
                            else if (fIdx == 2 && c.sourcePinId == "trigger:sx_pinch_rng") cableMatches = true;
                            else if (fIdx == 3 && c.sourcePinId == "trigger:sx_pinch_pky") cableMatches = true;
                        }
                    }
                    else
                    {
                        cableMatches = true;
                    }
                }
            }
            else if (trigSrc == 4) // instrument
            {
                if (c.targetNodeId == selectedNodeId || c.sourceNodeId == selectedNodeId)
                    cableMatches = true;
            }
        }

        MiMOLookAndFeel::drawInkCable(g, p1, p2, c.freehandNorm,
                                      static_cast<int>(i) * 37 + 11, boilTick,
                                      c.isModulation,
                                      c.colour,
                                      isHover, false,
                                      n1, n2,
                                      isTrigCable,
                                      (cableMatches && isTrigCable) ? pulseVal : 0.0f);
    }

    // 2. Cavo in fase di disegno a mano libera
    if (dragMode == DragMode::DrawCable)
    {
        juce::Path livePath;
        livePath.startNewSubPath(cableStartWorld);
        for (const auto& pt : cableFreehandWorld)
            livePath.lineTo(pt);
        livePath.lineTo(cableCurrentWorld);

        if (cableSourceIsTrigger)
        {
            g.setColour(juce::Colour(0xFFFF7A18).withAlpha(0.9f));
            g.strokePath(livePath, juce::PathStrokeType(4.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (cableSourceIsMod)
        {
            g.setColour(cableSourceColour.withAlpha(0.85f));
            g.strokePath(livePath, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.strokePath(livePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Disegna tutti i Nodi del Canvas
    for (const auto& node : nodes)
        paintCanvasNode(g, node);

    // 4. Disegna i Pin di Connessione con forme, color coding e animazioni
    for (const auto& pin : allPins)
    {
        const float r = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond) ? 7.5f : (pin.isMod ? 6.5f : 6.0f);
        const bool isHovered = (hasHoveredPin && hoveredPinNodeId == pin.nodeId && hoveredPinId == pin.pinId);
        const bool isSelected = (selectedPinNodeId == pin.nodeId && selectedPinId == pin.pinId);

        const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
        const bool typeCompatible = cableSourceIsTrigger ? pinIsTrig : (cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

        const bool isValidTarget = (dragMode == DragMode::DrawCable && pin.nodeId != cableSourceNodeId &&
                                    (pin.isOutput != cableSourceIsOutput) && typeCompatible);

        bool pinMatches = false;
        if (isPulse)
        {
            if (trigSrc == 1 && pin.nodeId == "hw-drum") pinMatches = true;
            else if (trigSrc == 2 && (pin.nodeId == "hw-mic")) pinMatches = true;
            else if (trigSrc == 3 && (pin.nodeId == "hw-eye" || pin.nodeId == "hw-webcam"))
            {
                const int fIdx = lastTriggerFingerIdx.load();
                const bool isSec = lastTriggerIsSecondary.load();
                if (fIdx >= 0)
                {
                    if (!isSec)
                    {
                        if (fIdx == 0 && pin.pinId == "trigger:dx_pinch_idx") pinMatches = true;
                        else if (fIdx == 1 && pin.pinId == "trigger:dx_pinch_mid") pinMatches = true;
                        else if (fIdx == 2 && pin.pinId == "trigger:dx_pinch_rng") pinMatches = true;
                        else if (fIdx == 3 && pin.pinId == "trigger:dx_pinch_pky") pinMatches = true;
                    }
                    else
                    {
                        if (fIdx == 0 && pin.pinId == "trigger:sx_pinch_idx") pinMatches = true;
                        else if (fIdx == 1 && pin.pinId == "trigger:sx_pinch_mid") pinMatches = true;
                        else if (fIdx == 2 && pin.pinId == "trigger:sx_pinch_rng") pinMatches = true;
                        else if (fIdx == 3 && pin.pinId == "trigger:sx_pinch_pky") pinMatches = true;
                    }
                }
                else
                {
                    pinMatches = true;
                }
            }
        }
        const bool isFiring = (pinIsTrig && pinMatches && isPulse);

        MiMOLookAndFeel::drawPinJack(g, pin.worldPos, r, pin.shapeType, pin.colour,
                                     isHovered, isSelected, isValidTarget, isFiring);
    }

    // 5. Tendine interattive per la selezione diretta di Webcam / Microfono
    paintCanvasDropdowns(g);
}

void MainComponent::paintCanvasNode(juce::Graphics& g, const CanvasNode& node)
{
    const juce::Rectangle<float> box(node.pos.x, node.pos.y, node.size.x, node.size.y);
    const int seed = node.id.hashCode();
    const bool isNodeSelected = (node.id == selectedNodeId);
    const juce::Colour glow = isNodeSelected ? MiMOLookAndFeel::crayonCherry
                                             : ((node.id == hoveredHwNodeId) ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack);

    if (node.type == NodeType::RockPiCentral)
    {
        if (mimoImage.isValid())
        {
            if (node.id == hoveredHwNodeId)
            {
                g.setColour(glow.withAlpha(0.35f));
                g.fillRoundedRectangle(box.expanded(8.0f), 18.0f);
            }

            g.setColour(juce::Colours::black.withAlpha(isDarkMode ? 0.45f : 0.22f));
            g.fillRoundedRectangle(box.translated(0.0f, 6.0f).reduced(2.0f), 16.0f);

            g.setColour(juce::Colour(0xFF1E1E1E));
            g.fillRoundedRectangle(box.reduced(1.0f), 14.0f);

            if (isDarkMode)
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawRoundedRectangle(box.expanded(1.0f), 16.0f, 1.2f);
            }

            g.setOpacity(1.0f);
            g.drawImage(mimoImage, box, juce::RectanglePlacement::stretchToFit);

            // Badge porte P1..P5 sotto ai tasti
            static constexpr float keyXFractions[5] = { 0.1490f, 0.2958f, 0.4426f, 0.5895f, 0.7349f };
            for (int i = 0; i < 5; ++i)
            {
                const float cx = box.getX() + box.getWidth() * keyXFractions[i];
                const float cy = box.getBottom() + 4.0f;
                const juce::String portPinId = "audio:p" + juce::String(i + 1);

                bool isConnected = false;
                juce::Colour connColour = MiMOLookAndFeel::crayonOchre;
                for (const auto& c : cables)
                {
                    if (c.targetNodeId == "rockpi" && c.targetPinId == portPinId)
                    {
                        isConnected = true;
                        for (const auto& n : nodes)
                        {
                            if (n.id == c.sourceNodeId)
                            {
                                connColour = n.hasInstrument ? n.crayonColour : MiMOLookAndFeel::crayonOchre;
                                break;
                            }
                        }
                        break;
                    }
                }

                g.setColour(isConnected ? connColour : MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(9.5f, juce::Font::bold));
                g.drawText("P" + juce::String(i + 1), static_cast<int>(cx - 12), static_cast<int>(cy + 8), 24, 12, juce::Justification::centred);
            }

            // Badge OUT sul lato destro
            const float outX = box.getRight() + 4.0f;
            const float outY = box.getY() + box.getHeight() * 0.52f;
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("OUT", static_cast<int>(outX - 12), static_cast<int>(outY + 8), 24, 12, juce::Justification::centred);
        }
        else
        {
            MiMOLookAndFeel::drawSketchBox(g, box, seed, MiMOLookAndFeel::crayonOchre, 0.25f, glow, 2.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(15.5f, juce::Font::bold));
            g.drawText(node.label, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 10), 200, 20, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(11.5f, juce::Font::plain));
            g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 28), 220, 16, juce::Justification::centredLeft);
        }
    }
    else if (node.type == NodeType::HardwareModule)
    {
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

        const juce::Colour fillCol = node.hasInstrument ? node.crayonColour : juce::Colours::transparentBlack;
        MiMOLookAndFeel::drawSketchBox(g, box, seed, fillCol, node.hasInstrument ? 0.72f : 0.0f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

        // Presa fisica superiore verso MiMO
        juce::Rectangle<float> plug(box.getX() + 18.0f, box.getY() - 4.0f, 22.0f, 8.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillRoundedRectangle(plug, 2.0f);

        // Badge Porta
        juce::Rectangle<float> badge(box.getX() + 12.0f, box.getY() + 10.0f, 34.0f, 22.0f);
        const juce::Colour badgeCol = (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
            ? MiMOLookAndFeel::crayonCyan
            : MiMOLookAndFeel::crayonOchre;
        MiMOLookAndFeel::drawSketchBox(g, badge, seed + 1, badgeCol, 0.45f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText(node.portBadge, badge.toNearestInt(), juce::Justification::centred);

        g.setFont(juce::Font(14.5f, juce::Font::bold));
        g.drawText(node.label, static_cast<int>(box.getX() + 52), static_cast<int>(box.getY() + 8), 135, 20, juce::Justification::centredLeft);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText(node.subtitle, static_cast<int>(box.getX() + 52), static_cast<int>(box.getY() + 25), 140, 16, juce::Justification::centredLeft);

        // Pulsante di eliminazione [X] per TUTTI i moduli hardware
        juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
        MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

        // Modulo Occhio / Webcam pulsanti QR e VID
        if (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
        {
            const auto cfgBtn = getEyeCardConfigButtonBounds(box);
            MiMOLookAndFeel::drawSketchBox(g, cfgBtn, seed + 98, MiMOLookAndFeel::crayonCyan, 0.50f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText("QR", cfgBtn.toNearestInt(), juce::Justification::centred);

            const auto vidBtn = getEyeCardViewToggleButtonBounds(box);
            MiMOLookAndFeel::drawSketchBox(g, vidBtn, seed + 97,
                                           eyeShowVideo ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                           eyeShowVideo ? 0.60f : 0.40f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.drawText(eyeShowVideo ? "VID" : "SKEL", vidBtn.toNearestInt(), juce::Justification::centred);
        }

        if (node.isIntegratedMic || node.id == "hw-mic")
        {
            // VU Meter Microfono reale
            juce::Rectangle<float> vuBox(box.getX() + 12.0f, box.getY() + 46.0f, box.getWidth() - 24.0f, 18.0f);
            MiMOLookAndFeel::drawSketchBox(g, vuBox, seed + 101, micTriggerFired ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::paperBg,
                                           micTriggerFired ? 0.75f : 0.40f, juce::Colours::transparentBlack, 1.2f);
            if (micTriggerFired)
            {
                g.setColour(MiMOLookAndFeel::polaroidWhite);
                g.setFont(juce::Font(9.5f, juce::Font::bold));
                g.drawText("TRIG! (COLPO RILEVATO)", vuBox.toNearestInt(), juce::Justification::centred);
            }
            else
            {
                const float curLevel = realMicLevel.load();
                const float lvlW = (vuBox.getWidth() - 4.0f) * juce::jlimit(0.0f, 1.0f, curLevel);
                if (lvlW > 1.0f)
                {
                    g.setColour(curLevel > 0.6f ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::crayonSage);
                    g.fillRect(vuBox.getX() + 2.0f, vuBox.getY() + 2.0f, lvlW, vuBox.getHeight() - 4.0f);
                }
                const float thX = vuBox.getX() + 2.0f + (vuBox.getWidth() - 4.0f) * getMicThreshold();
                g.setColour(MiMOLookAndFeel::crayonCherry);
                g.drawLine(thX, vuBox.getY() + 1.0f, thX, vuBox.getBottom() - 1.0f, 1.5f);
            }

            // Slider sensibilità microfono
            juce::Rectangle<float> slTrack(box.getX() + 12.0f, box.getY() + 68.0f, box.getWidth() - 24.0f, 14.0f);
            MiMOLookAndFeel::drawSketchBox(g, slTrack, seed + 102, MiMOLookAndFeel::paperBg, 0.5f, juce::Colours::transparentBlack, 1.0f);
            const float fillW = slTrack.getWidth() * micSensitivity;
            if (fillW > 2.0f)
            {
                juce::Rectangle<float> fillRect(slTrack.getX(), slTrack.getY(), fillW, slTrack.getHeight());
                g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.65f));
                g.fillRoundedRectangle(fillRect, 3.0f);
            }
            const float knobX = slTrack.getX() + fillW;
            const float knobY = slTrack.getCentreY();
            g.setColour(MiMOLookAndFeel::paperCard);
            g.fillEllipse(knobX - 6.0f, knobY - 6.0f, 12.0f, 12.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawEllipse(knobX - 6.0f, knobY - 6.0f, 12.0f, 12.0f, 1.4f);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText("Sens: " + juce::String(static_cast<int>(micSensitivity * 100)) + "%  |  " + getMicStatusText(),
                       static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 84), static_cast<int>(box.getWidth() - 24), 13, juce::Justification::centredLeft);

            // Riga Trigger Out
            juce::Rectangle<float> trigRow(box.getX() + 12.0f, box.getY() + 102.0f, box.getWidth() - 24.0f, 26.0f);
            g.setColour(juce::Colour(0xFFFF7A18).withAlpha(0.18f));
            g.fillRoundedRectangle(trigRow, 4.0f);
            g.setColour(juce::Colour(0xFFFF7A18));
            g.drawRoundedRectangle(trigRow, 4.0f, 1.4f);

            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("Trigger Colpo", static_cast<int>(trigRow.getX() + 8), static_cast<int>(trigRow.getY()), 110, static_cast<int>(trigRow.getHeight()), juce::Justification::centredLeft);

            // Tasto Test
            juce::Rectangle<float> testBtn(trigRow.getRight() - 65.0f, trigRow.getY() + 3.0f, 40.0f, 20.0f);
            MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, MiMOLookAndFeel::crayonOchre, 0.40f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText("TEST", testBtn.toNearestInt(), juce::Justification::centred);

            // Righe Modulazione (Velocity e Pressione)
            float my = box.getY() + 134.0f;
            for (const auto& m : node.modOutputs)
            {
                juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 26.0f);
                g.setColour(m.colour.withAlpha(0.20f));
                g.fillRoundedRectangle(row, 4.0f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.drawRoundedRectangle(row, 4.0f, 1.2f);

                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.drawText(m.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 90, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);
                g.setFont(juce::Font(9.5f, juce::Font::plain));
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.drawText("(verso manopola)", static_cast<int>(row.getX() + 90), static_cast<int>(row.getY()), static_cast<int>(row.getWidth() - 95), static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                my += 32.0f;
            }
        }
        else if (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
        {
            juce::Rectangle<float> camBox(box.getX() + 12.0f, box.getY() + 46.0f, box.getWidth() - 24.0f, 85.0f);
            MiMOLookAndFeel::drawSketchBox(g, camBox, seed + 201, MiMOLookAndFeel::paperBg, 0.85f, juce::Colours::transparentBlack, 1.3f);

            juce::Image camFrame;
            {
                juce::ScopedLock sl(cameraLock);
                if (latestWebcamImage.isValid())
                    camFrame = latestWebcamImage.createCopy();
            }

            const auto innerCamBox = camBox.reduced(2.0f);

            if (eyeShowVideo && camFrame.isValid())
            {
                const float imgW = static_cast<float>(camFrame.getWidth());
                const float imgH = static_cast<float>(camFrame.getHeight());
                const auto videoBox = juce::RectanglePlacement(juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
                    .appliedTo(juce::Rectangle<float>(0.0f, 0.0f, imgW, imgH), innerCamBox);

                g.saveState();
                g.reduceClipRegion(innerCamBox.toNearestInt());
                g.setColour(juce::Colour(0xff181a20));
                g.fillRect(innerCamBox);
                g.drawImage(camFrame, videoBox, juce::RectanglePlacement::centred);
                drawHandSkeleton(g, videoBox, latestEyeState, true);
                g.restoreState();
            }
            else if (latestEyeState.hasHand)
            {
                g.saveState();
                g.reduceClipRegion(innerCamBox.toNearestInt());
                drawHandSkeleton(g, innerCamBox, latestEyeState, false);
                g.restoreState();
            }
            else
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.0f, juce::Font::italic));
                g.drawText(getCameraStatusText(), camBox.reduced(6.0f).toNearestInt(), juce::Justification::centred);
            }

            // Render uscite modulo
            if (!node.outputGroups.empty())
            {
                float curY = box.getY() + 168.0f;
                for (size_t gIdx = 0; gIdx < node.outputGroups.size(); ++gIdx)
                {
                    const auto& grp = node.outputGroups[gIdx];
                    if (!grp.parentGroup.isEmpty())
                        continue; // Subgroup renderizzato sotto il proprio genitore

                    const bool hasSubgroups = grp.items.empty();
                    const bool handOn = grp.isEnabled;
                    juce::Rectangle<float> hdrBox(box.getX() + 8.0f, curY, box.getWidth() - 16.0f, 24.0f);

                    int totalItemsInTree = 0;
                    int activeCablesInTree = 0;

                    if (hasSubgroups)
                    {
                        for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                        {
                            const auto& sub = node.outputGroups[sIdx];
                            if (sub.parentGroup == grp.id)
                            {
                                totalItemsInTree += static_cast<int>(sub.items.size());
                                for (const auto& item : sub.items)
                                    for (const auto& c : cables)
                                        if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                            activeCablesInTree++;
                            }
                        }
                    }
                    else
                    {
                        totalItemsInTree = static_cast<int>(grp.items.size());
                        for (const auto& item : grp.items)
                            for (const auto& c : cables)
                                if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                    activeCablesInTree++;
                    }

                    // Top-Level Header
                    g.setColour(juce::Colour(0xFF1E2129));
                    g.fillRoundedRectangle(hdrBox, 4.0f);
                    g.setColour(activeCablesInTree > 0 ? MiMOLookAndFeel::crayonOchre : (isDarkMode ? juce::Colours::white.withAlpha(0.24f) : juce::Colours::black.withAlpha(0.20f)));
                    g.drawRoundedRectangle(hdrBox, 4.0f, 1.2f);

                    // Interruttore ON/OFF a sinistra dell'header principale
                    const juce::Rectangle<float> switchBox(hdrBox.getX() + 5.0f, hdrBox.getCentreY() - 7.0f, 29.0f, 14.0f);
                    g.setColour(handOn ? juce::Colour(0xFF2E8540) : juce::Colour(0xFF333842));
                    g.fillRoundedRectangle(switchBox, 7.0f);
                    g.setColour(handOn ? juce::Colour(0xFF55E678).withAlpha(0.6f) : juce::Colours::white.withAlpha(0.15f));
                    g.drawRoundedRectangle(switchBox, 7.0f, 1.0f);

                    const float knobRadius = 5.0f;
                    const float knobX = handOn ? (switchBox.getRight() - knobRadius - 2.0f) : (switchBox.getX() + knobRadius + 2.0f);
                    g.setColour(handOn ? juce::Colours::white : juce::Colour(0xFF888E9B));
                    g.fillEllipse(knobX - knobRadius, switchBox.getCentreY() - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

                    g.setFont(juce::Font(7.5f, juce::Font::bold));
                    g.setColour(handOn ? juce::Colours::white : juce::Colour(0xFF757B85));
                    if (handOn)
                        g.drawText("ON", static_cast<int>(switchBox.getX() + 2), static_cast<int>(switchBox.getY()), 13, static_cast<int>(switchBox.getHeight()), juce::Justification::centred);
                    else
                        g.drawText("OFF", static_cast<int>(switchBox.getRight() - 15), static_cast<int>(switchBox.getY()), 13, static_cast<int>(switchBox.getHeight()), juce::Justification::centred);

                    // Freccia toggle espandi/comprimi [▼] / [▶]
                    const juce::Rectangle<float> chevBox(switchBox.getRight() + 4.0f, hdrBox.getY(), 13.0f, hdrBox.getHeight());
                    g.setColour(handOn ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(10.0f, juce::Font::bold));
                    g.drawText(grp.isExpanded ? juce::String::charToString(0x25BC) : juce::String::charToString(0x25B6),
                               chevBox.toNearestInt(), juce::Justification::centred);

                    // Titolo gruppo principale (es. "Mano Destra (DX)")
                    const float titleX = chevBox.getRight() + 3.0f;
                    g.setColour(handOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(10.5f, juce::Font::bold));
                    juce::String mainTitle = grp.name;
                    if (!handOn)
                        mainTitle += " (SPENTA)";
                    g.drawText(mainTitle, static_cast<int>(titleX), static_cast<int>(hdrBox.getY()),
                               static_cast<int>(hdrBox.getWidth() - 95.0f), static_cast<int>(hdrBox.getHeight()), juce::Justification::centredLeft);

                    // Conteggio totale
                    g.setColour(MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(9.0f, juce::Font::plain));
                    g.drawText("(" + juce::String(totalItemsInTree) + ")",
                               static_cast<int>(hdrBox.getRight() - 52), static_cast<int>(hdrBox.getY()), 24, static_cast<int>(hdrBox.getHeight()), juce::Justification::centredRight);

                    if (!grp.isExpanded && activeCablesInTree > 0)
                    {
                        g.setColour(MiMOLookAndFeel::crayonOchre);
                        g.setFont(juce::Font(8.5f, juce::Font::bold));
                        g.drawText(juce::String(activeCablesInTree) + " cavi",
                                   static_cast<int>(hdrBox.getRight() - 28), static_cast<int>(hdrBox.getY()), 26, static_cast<int>(hdrBox.getHeight()), juce::Justification::centred);
                    }

                    curY += 26.0f;

                    if (hasSubgroups)
                    {
                        if (grp.isExpanded)
                        {
                            for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                            {
                                const auto& sub = node.outputGroups[sIdx];
                                if (sub.parentGroup != grp.id)
                                    continue;

                                juce::Rectangle<float> subHdrBox(box.getX() + 14.0f, curY, box.getWidth() - 28.0f, 22.0f);
                                int activeSubCables = 0;
                                for (const auto& item : sub.items)
                                    for (const auto& c : cables)
                                        if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                            activeSubCables++;

                                const bool isTrigGroup = sub.id.contains("pinches") || sub.id.contains("trig");
                                const bool subOn = sub.isEnabled && handOn;

                                g.setColour(juce::Colour(0xFF16181F));
                                g.fillRoundedRectangle(subHdrBox, 3.5f);
                                g.setColour(activeSubCables > 0 ? MiMOLookAndFeel::crayonOchre : juce::Colours::white.withAlpha(0.12f));
                                g.drawRoundedRectangle(subHdrBox, 3.5f, 0.8f);

                                // Switch ON/OFF a sinistra del sottogruppo
                                const juce::Rectangle<float> subSwitchBox(subHdrBox.getX() + 4.0f, subHdrBox.getCentreY() - 6.0f, 26.0f, 12.0f);
                                const juce::Colour subOnCol = isTrigGroup ? juce::Colour(0xFFFF7A18) : juce::Colour(0xFF2E8540);
                                g.setColour(subOn ? subOnCol : juce::Colour(0xFF2B2F38));
                                g.fillRoundedRectangle(subSwitchBox, 6.0f);
                                g.setColour(subOn ? subOnCol.brighter(0.2f) : juce::Colours::white.withAlpha(0.12f));
                                g.drawRoundedRectangle(subSwitchBox, 6.0f, 0.8f);

                                const float subKnobR = 4.0f;
                                const float subKnobX = subOn ? (subSwitchBox.getRight() - subKnobR - 2.0f) : (subSwitchBox.getX() + subKnobR + 2.0f);
                                g.setColour(subOn ? juce::Colours::white : juce::Colour(0xFF888E9B));
                                g.fillEllipse(knobRadius > 0.0f ? (subKnobX - subKnobR) : 0.0f, subSwitchBox.getCentreY() - subKnobR, subKnobR * 2.0f, subKnobR * 2.0f);

                                g.setFont(juce::Font(6.5f, juce::Font::bold));
                                g.setColour(subOn ? juce::Colours::white : juce::Colour(0xFF6E747F));
                                if (subOn)
                                    g.drawText("ON", static_cast<int>(subSwitchBox.getX() + 2), static_cast<int>(subSwitchBox.getY()), 11, static_cast<int>(subSwitchBox.getHeight()), juce::Justification::centred);
                                else
                                    g.drawText("OFF", static_cast<int>(subSwitchBox.getRight() - 13), static_cast<int>(subSwitchBox.getY()), 11, static_cast<int>(subSwitchBox.getHeight()), juce::Justification::centred);

                                // Freccia toggle espandi/comprimi sottogruppo [▼] / [▶]
                                const juce::Rectangle<float> subChevBox(subSwitchBox.getRight() + 3.0f, subHdrBox.getY(), 12.0f, subHdrBox.getHeight());
                                g.setColour(subOn ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::inkMuted);
                                g.setFont(juce::Font(9.0f, juce::Font::bold));
                                g.drawText(sub.isExpanded ? juce::String::charToString(0x25BC) : juce::String::charToString(0x25B6),
                                           subChevBox.toNearestInt(), juce::Justification::centred);

                                // Titolo sottogruppo
                                const float subTitleX = subChevBox.getRight() + 3.0f;
                                g.setColour(subOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                g.setFont(juce::Font(9.5f, juce::Font::bold));
                                juce::String subDisplayName = sub.name;
                                if (!sub.isEnabled && isTrigGroup)
                                    subDisplayName += " [TRIG OFF]";
                                else if (!sub.isEnabled)
                                    subDisplayName += " [OFF]";
                                g.drawText(subDisplayName, static_cast<int>(subTitleX), static_cast<int>(subHdrBox.getY()),
                                           static_cast<int>(subHdrBox.getWidth() - 85.0f), static_cast<int>(subHdrBox.getHeight()), juce::Justification::centredLeft);

                                // Conteggio uscite
                                g.setColour(MiMOLookAndFeel::inkMuted);
                                g.setFont(juce::Font(8.5f, juce::Font::plain));
                                g.drawText("(" + juce::String(sub.items.size()) + ")",
                                           static_cast<int>(subHdrBox.getRight() - 44), static_cast<int>(subHdrBox.getY()), 20, static_cast<int>(subHdrBox.getHeight()), juce::Justification::centredRight);

                                if (!sub.isExpanded && activeSubCables > 0)
                                {
                                    g.setColour(MiMOLookAndFeel::crayonOchre);
                                    g.setFont(juce::Font(8.0f, juce::Font::bold));
                                    g.drawText(juce::String(activeSubCables) + " cavi",
                                               static_cast<int>(subHdrBox.getRight() - 24), static_cast<int>(subHdrBox.getY()), 22, static_cast<int>(subHdrBox.getHeight()), juce::Justification::centred);
                                }

                                curY += 24.0f;

                                if (sub.isExpanded)
                                {
                                    for (const auto& item : sub.items)
                                    {
                                        juce::Rectangle<float> row(box.getX() + 18.0f, curY, box.getWidth() - 36.0f, 24.0f);
                                        const bool itemActive = subOn;
                                        const juce::Colour itemCol = item.isTrigger ? juce::Colour(0xFFFF7A18) : item.colour;

                                        g.setColour(itemActive ? itemCol.withAlpha(0.18f) : juce::Colour(0xFF262A32).withAlpha(0.25f));
                                        g.fillRoundedRectangle(row, 3.5f);
                                        g.setColour(itemActive ? itemCol.withAlpha(0.85f) : juce::Colours::white.withAlpha(0.12f));
                                        g.drawRoundedRectangle(row, 3.5f, 0.9f);

                                        g.setFont(juce::Font(9.5f, juce::Font::bold));
                                        g.setColour(itemActive ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);

                                        if (item.isTrigger)
                                        {
                                            g.drawText(item.label, static_cast<int>(row.getX() + 6), static_cast<int>(row.getY()), 68, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                                            const bool isSec = (sub.parentGroup == "eye_sx" || sub.id.contains("sx"));
                                            int fSub = 0;
                                            if (item.pinId.endsWith("_mid")) fSub = 1;
                                            else if (item.pinId.endsWith("_rng")) fSub = 2;
                                            else if (item.pinId.endsWith("_pky")) fSub = 3;
                                            const int gIdx = isSec ? (fSub + 4) : fSub;

                                            // Controlla se questo pin ha un cavo collegato a uno strumento del canvas
                                            juce::String connectedInstName;
                                            for (const auto& c : cables)
                                            {
                                                if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                                {
                                                    for (const auto& otherNode : nodes)
                                                    {
                                                        if (otherNode.id == c.targetNodeId)
                                                        {
                                                            connectedInstName = otherNode.label;
                                                            break;
                                                        }
                                                    }
                                                    break;
                                                }
                                            }

                                            // Tasto TEST
                                            juce::Rectangle<float> testBtn(row.getRight() - 40.0f, row.getY() + 2.5f, 32.0f, 19.0f);
                                            MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, itemActive ? MiMOLookAndFeel::crayonOchre : juce::Colour(0xFF3B404D), itemActive ? 0.40f : 0.15f);
                                            g.setColour(itemActive ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                            g.setFont(juce::Font(7.5f, juce::Font::bold));
                                            g.drawText(itemActive ? "TEST" : "MUTE", testBtn.toNearestInt(), juce::Justification::centred);

                                            // Selettore Strumento / Suono (Pillola cliccabile)
                                            const float soundBtnX = row.getX() + 76.0f;
                                            const float soundBtnW = testBtn.getX() - soundBtnX - 4.0f;
                                            juce::Rectangle<float> soundBtn(soundBtnX, row.getY() + 2.5f, soundBtnW, 19.0f);

                                            if (connectedInstName.isNotEmpty())
                                            {
                                                g.setColour(juce::Colour(0xFF1E2822));
                                                g.fillRoundedRectangle(soundBtn, 3.0f);
                                                g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.85f));
                                                g.drawRoundedRectangle(soundBtn, 3.0f, 0.8f);
                                                g.setColour(MiMOLookAndFeel::crayonOchre);
                                                g.setFont(juce::Font(8.0f, juce::Font::bold));
                                                g.drawText("-> " + connectedInstName, soundBtn.toNearestInt(), juce::Justification::centred);
                                            }
                                            else
                                            {
                                                const int sndId = audioEngine.getEyeFingerSound(gIdx);
                                                const juce::String sndName = getSoundNameForId(sndId);
                                                g.setColour(juce::Colour(0xFF222630));
                                                g.fillRoundedRectangle(soundBtn, 3.0f);
                                                g.setColour(juce::Colours::white.withAlpha(0.18f));
                                                g.drawRoundedRectangle(soundBtn, 3.0f, 0.8f);
                                                g.setColour(MiMOLookAndFeel::fountainInk);
                                                g.setFont(juce::Font(8.0f, juce::Font::bold));
                                                g.drawText(sndName + juce::String(juce::CharPointer_UTF8(" \xe2\x96\xbe")), soundBtn.toNearestInt(), juce::Justification::centred);
                                            }
                                        }
                                        else
                                        {
                                            // 1. Calcola il valore live normalizzato [0.0 .. 1.0] per questo asse / parametro
                                            const bool hasPrimary = latestEyeState.hasHand;
                                            const bool hasSecondary = latestEyeState.hasSecondaryHand;
                                            const bool hasAnyHand = hasPrimary || hasSecondary;

                                            float liveVal = -1.0f;
                                            if (itemActive)
                                            {
                                                const bool isSx = (sub.parentGroup == "eye_sx" || sub.id.contains("sx") || item.pinId.startsWith("mod:sx_"));
                                                const bool useSecondaryForDx = !hasPrimary && hasSecondary;
                                                const bool usePrimaryForSx = !hasSecondary && hasPrimary;

                                                const float pX = hasPrimary ? latestEyeState.palmX : (useSecondaryForDx ? latestEyeState.secondaryPalmX : -1.0f);
                                                const float pY = hasPrimary ? latestEyeState.palmY : (useSecondaryForDx ? latestEyeState.secondaryPalmY : -1.0f);
                                                const float pZ = hasPrimary ? latestEyeState.palmZ : (useSecondaryForDx ? latestEyeState.secondaryPalmZ : -1.0f);
                                                const float pFist = hasPrimary ? latestEyeState.fist : (useSecondaryForDx ? latestEyeState.secondaryFist : -1.0f);
                                                const float pTilt = hasPrimary ? latestEyeState.handTilt : (useSecondaryForDx ? latestEyeState.secondaryHandTilt : -1.0f);

                                                const float sX = hasSecondary ? latestEyeState.secondaryPalmX : (usePrimaryForSx ? latestEyeState.palmX : -1.0f);
                                                const float sY = hasSecondary ? latestEyeState.secondaryPalmY : (usePrimaryForSx ? latestEyeState.palmY : -1.0f);
                                                const float sZ = hasSecondary ? latestEyeState.secondaryPalmZ : (usePrimaryForSx ? latestEyeState.palmZ : -1.0f);
                                                const float sFist = hasSecondary ? latestEyeState.secondaryFist : (usePrimaryForSx ? latestEyeState.fist : -1.0f);
                                                const float sTilt = hasSecondary ? latestEyeState.secondaryHandTilt : (usePrimaryForSx ? latestEyeState.handTilt : -1.0f);

                                                auto getFinger = [&](int fIdx, bool sx) -> float {
                                                    if (!sx)
                                                    {
                                                        if (hasPrimary) return latestEyeState.fingerFlex[static_cast<size_t>(fIdx)];
                                                        if (useSecondaryForDx) return latestEyeState.secondaryFingerFlex[static_cast<size_t>(fIdx)];
                                                    }
                                                    else
                                                    {
                                                        if (hasSecondary) return latestEyeState.secondaryFingerFlex[static_cast<size_t>(fIdx)];
                                                        if (usePrimaryForSx) return latestEyeState.fingerFlex[static_cast<size_t>(fIdx)];
                                                    }
                                                    return -1.0f;
                                                };

                                                if (hasAnyHand)
                                                {
                                                    if (!isSx)
                                                    {
                                                        if (item.pinId == "mod:dx_x" || item.pinId == "mod:x") liveVal = pX;
                                                        else if (item.pinId == "mod:dx_y" || item.pinId == "mod:y") liveVal = pY;
                                                        else if (item.pinId == "mod:dx_z" || item.pinId == "mod:z" || item.pinId == "mod:mot") liveVal = pZ;
                                                        else if (item.pinId == "mod:dx_fist" || item.pinId == "mod:fist") liveVal = pFist;
                                                        else if (item.pinId == "mod:dx_tilt" || item.pinId == "mod:tilt") liveVal = pTilt;
                                                        else if (item.pinId == "mod:dx_thumb" || item.pinId == "mod:thumb") liveVal = getFinger(0, false);
                                                        else if (item.pinId == "mod:dx_idx" || item.pinId == "mod:idx" || item.pinId == "mod:pinch_idx") liveVal = getFinger(1, false);
                                                        else if (item.pinId == "mod:dx_mid" || item.pinId == "mod:mid" || item.pinId == "mod:pinch_mid") liveVal = getFinger(2, false);
                                                        else if (item.pinId == "mod:dx_rng" || item.pinId == "mod:rng" || item.pinId == "mod:pinch_rng") liveVal = getFinger(3, false);
                                                        else if (item.pinId == "mod:dx_pky" || item.pinId == "mod:pky" || item.pinId == "mod:pinch_pky") liveVal = getFinger(4, false);
                                                    }
                                                    else
                                                    {
                                                        if (item.pinId == "mod:sx_x") liveVal = sX;
                                                        else if (item.pinId == "mod:sx_y") liveVal = sY;
                                                        else if (item.pinId == "mod:sx_z") liveVal = sZ;
                                                        else if (item.pinId == "mod:sx_fist") liveVal = sFist;
                                                        else if (item.pinId == "mod:sx_tilt") liveVal = sTilt;
                                                        else if (item.pinId == "mod:sx_thumb") liveVal = getFinger(0, true);
                                                        else if (item.pinId == "mod:sx_idx") liveVal = getFinger(1, true);
                                                        else if (item.pinId == "mod:sx_mid") liveVal = getFinger(2, true);
                                                        else if (item.pinId == "mod:sx_rng") liveVal = getFinger(3, true);
                                                        else if (item.pinId == "mod:sx_pky") liveVal = getFinger(4, true);
                                                    }
                                                }
                                                else if (isCameraRecording)
                                                {
                                                    const float optX = juce::jlimit(0.0f, 1.0f, (realCameraCentroidX.load() + 1.0f) * 0.5f);
                                                    const float optY = juce::jlimit(0.0f, 1.0f, 1.0f - (realCameraCentroidY.load() + 1.0f) * 0.5f);
                                                    float optZ = realCameraMotion.load();
                                                    if (cameraTrackingMode == CameraTrackingMode::Light)
                                                        optZ = realCameraBrightness.load();
                                                    else if (cameraTrackingMode == CameraTrackingMode::Presence)
                                                        optZ = juce::jlimit(0.0f, 1.0f, 1.0f - std::hypot(realCameraCentroidX.load(), realCameraCentroidY.load()) * 0.65f);
                                                    const float optFist = realCameraMotion.load();

                                                    if (item.pinId.contains("_x") || item.pinId == "mod:x") liveVal = optX;
                                                    else if (item.pinId.contains("_y") || item.pinId == "mod:y") liveVal = optY;
                                                    else if (item.pinId.contains("_z") || item.pinId == "mod:z" || item.pinId == "mod:mot") liveVal = optZ;
                                                    else if (item.pinId.contains("_fist") || item.pinId == "mod:fist") liveVal = optFist;
                                                    else if (item.pinId.contains("_tilt") || item.pinId == "mod:tilt") liveVal = optX;
                                                    else liveVal = optFist;
                                                }
                                            }

                                            // 2. Etichetta del nodo modulazione
                                            g.setFont(juce::Font(9.0f, juce::Font::bold));
                                            g.setColour(itemActive ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                            g.drawText(item.label, static_cast<int>(row.getX() + 6), static_cast<int>(row.getY()), 105, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                                            // 3. Verifica se c'e' un cavo collegato a questo pin
                                            juce::String connDest;
                                            for (const auto& c : cables)
                                            {
                                                if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                                {
                                                    if (c.targetPinId == "inst:tone")
                                                    {
                                                        connDest = "Timbro";
                                                        break;
                                                    }
                                                    const auto paramSpec = c.targetPinId.fromFirstOccurrenceOf("param:", false, false);
                                                    const auto pKey = paramSpec.contains(".") ? paramSpec.fromFirstOccurrenceOf(".", false, false) : paramSpec;
                                                    for (const auto& on : nodes)
                                                    {
                                                        if (on.id == c.targetNodeId)
                                                        {
                                                            for (const auto& dp : on.directParams)
                                                            {
                                                                if (dp.key == pKey) { connDest = dp.label; break; }
                                                            }
                                                            if (connDest.isEmpty())
                                                            {
                                                                for (const auto& b : on.innerBlocks)
                                                                {
                                                                    for (const auto& ip : b.params)
                                                                    {
                                                                        if (ip.key == pKey) { connDest = ip.label; break; }
                                                                    }
                                                                }
                                                            }
                                                            if (connDest.isEmpty()) connDest = on.label;
                                                            break;
                                                        }
                                                    }
                                                    break;
                                                }
                                            }

                                            // 4. Disegna Mini Gauge Bar & Percentuale Live
                                            const float rightPad = 4.0f;
                                            const float pctW = 32.0f;
                                            const float gaugeW = 50.0f;
                                            const float gaugeH = 9.0f;

                                            const float pctX = row.getRight() - rightPad - pctW;
                                            const float gaugeX = pctX - 4.0f - gaugeW;
                                            const float gaugeY = row.getCentreY() - gaugeH * 0.5f;

                                            if (connDest.isNotEmpty())
                                            {
                                                g.setFont(juce::Font(7.5f, juce::Font::bold));
                                                g.setColour(itemCol.withAlpha(0.95f));
                                                const float tagX = row.getX() + 106.0f;
                                                const float tagW = gaugeX - tagX - 2.0f;
                                                if (tagW > 18.0f)
                                                    g.drawText("-> " + connDest,
                                                               static_cast<int>(tagX), static_cast<int>(row.getY()), static_cast<int>(tagW), static_cast<int>(row.getHeight()),
                                                               juce::Justification::centredRight, true);
                                            }

                                            juce::Rectangle<float> gaugeTrack(gaugeX, gaugeY, gaugeW, gaugeH);
                                            g.setColour(juce::Colour(0xFF14171E));
                                            g.fillRoundedRectangle(gaugeTrack, 3.0f);
                                            g.setColour(juce::Colours::white.withAlpha(0.12f));
                                            g.drawRoundedRectangle(gaugeTrack, 3.0f, 0.7f);

                                            if (liveVal >= 0.0f && itemActive)
                                            {
                                                const float clampedVal = juce::jlimit(0.0f, 1.0f, liveVal);
                                                juce::Rectangle<float> fillBar(gaugeTrack.getX(), gaugeTrack.getY(), gaugeTrack.getWidth() * clampedVal, gaugeTrack.getHeight());
                                                g.setColour(itemCol.withAlpha(0.85f));
                                                g.fillRoundedRectangle(fillBar, 3.0f);

                                                const int pct = juce::roundToInt(clampedVal * 100.0f);
                                                g.setFont(juce::Font(8.5f, juce::Font::bold));
                                                g.setColour(MiMOLookAndFeel::fountainInk);
                                                g.drawText(juce::String(pct) + "%", static_cast<int>(pctX), static_cast<int>(row.getY()), static_cast<int>(pctW), static_cast<int>(row.getHeight()), juce::Justification::centredRight);
                                            }
                                            else
                                            {
                                                g.setFont(juce::Font(8.0f, juce::Font::plain));
                                                g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.5f));
                                                g.drawText(itemActive ? "--" : "off", static_cast<int>(pctX), static_cast<int>(row.getY()), static_cast<int>(pctW), static_cast<int>(row.getHeight()), juce::Justification::centredRight);
                                            }
                                        }

                                        curY += 26.0f;
                                    }
                                }
                            }
                        }
                    }
                    else // Gruppo lineare diretto (es. Modulo Batteria)
                    {
                        if (grp.isExpanded)
                        {
                            for (const auto& item : grp.items)
                            {
                                juce::Rectangle<float> row(box.getX() + 10.0f, curY, box.getWidth() - 20.0f, 24.0f);
                                const bool itemActive = handOn;
                                const juce::Colour itemCol = item.isTrigger ? juce::Colour(0xFFFF7A18) : item.colour;

                                g.setColour(itemActive ? itemCol.withAlpha(0.20f) : juce::Colour(0xFF262A32).withAlpha(0.25f));
                                g.fillRoundedRectangle(row, 4.0f);
                                g.setColour(itemActive ? itemCol.withAlpha(0.90f) : juce::Colours::white.withAlpha(0.12f));
                                g.drawRoundedRectangle(row, 4.0f, 1.0f);

                                g.setFont(juce::Font(10.0f, juce::Font::bold));
                                g.setColour(itemActive ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                g.drawText(item.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 110, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                                if (item.isTrigger)
                                {
                                    juce::Rectangle<float> testBtn(row.getRight() - 55.0f, row.getY() + 3.0f, 38.0f, 20.0f);
                                    MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, itemActive ? MiMOLookAndFeel::crayonOchre : juce::Colour(0xFF3B404D), itemActive ? 0.40f : 0.15f);
                                    g.setColour(itemActive ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                    g.setFont(juce::Font(9.0f, juce::Font::bold));
                                    g.drawText(itemActive ? "TEST" : "MUTE", testBtn.toNearestInt(), juce::Justification::centred);
                                }
                                else
                                {
                                    float micVal = -1.0f;
                                    if (item.pinId == "mod:vel") micVal = realMicVelocity.load();
                                    else if (item.pinId == "mod:press") micVal = realMicLevel.load();

                                    const float rightPad = 6.0f;
                                    const float pctW = 34.0f;
                                    const float gaugeW = 56.0f;
                                    const float gaugeH = 9.0f;
                                    const float pctX = row.getRight() - rightPad - pctW;
                                    const float gaugeX = pctX - 4.0f - gaugeW;
                                    const float gaugeY = row.getCentreY() - gaugeH * 0.5f;

                                    juce::String connDest;
                                    for (const auto& c : cables)
                                    {
                                        if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                        {
                                            if (c.targetPinId == "inst:tone")
                                            {
                                                connDest = "Timbro";
                                                break;
                                            }
                                            const auto paramSpec = c.targetPinId.fromFirstOccurrenceOf("param:", false, false);
                                            const auto pKey = paramSpec.contains(".") ? paramSpec.fromFirstOccurrenceOf(".", false, false) : paramSpec;
                                            for (const auto& on : nodes)
                                            {
                                                if (on.id == c.targetNodeId)
                                                {
                                                    for (const auto& dp : on.directParams)
                                                    {
                                                        if (dp.key == pKey) { connDest = dp.label; break; }
                                                    }
                                                    if (connDest.isEmpty())
                                                    {
                                                        for (const auto& b : on.innerBlocks)
                                                        {
                                                            for (const auto& ip : b.params)
                                                            {
                                                                if (ip.key == pKey) { connDest = ip.label; break; }
                                                            }
                                                        }
                                                    }
                                                    if (connDest.isEmpty()) connDest = on.label;
                                                    break;
                                                }
                                            }
                                            break;
                                        }
                                    }

                                    if (connDest.isNotEmpty())
                                    {
                                        g.setFont(juce::Font(8.0f, juce::Font::bold));
                                        g.setColour(itemCol.withAlpha(0.95f));
                                        const float tagX = row.getX() + 95.0f;
                                        const float tagW = gaugeX - tagX - 2.0f;
                                        if (tagW > 16.0f)
                                            g.drawText("-> " + connDest,
                                                       static_cast<int>(tagX), static_cast<int>(row.getY()), static_cast<int>(tagW), static_cast<int>(row.getHeight()),
                                                       juce::Justification::centredRight, true);
                                    }

                                    juce::Rectangle<float> gaugeTrack(gaugeX, gaugeY, gaugeW, gaugeH);
                                    g.setColour(juce::Colour(0xFF14171E));
                                    g.fillRoundedRectangle(gaugeTrack, 3.0f);
                                    g.setColour(juce::Colours::white.withAlpha(0.12f));
                                    g.drawRoundedRectangle(gaugeTrack, 3.0f, 0.7f);

                                    if (micVal >= 0.0f && itemActive)
                                    {
                                        const float clampedVal = juce::jlimit(0.0f, 1.0f, micVal);
                                        juce::Rectangle<float> fillBar(gaugeTrack.getX(), gaugeTrack.getY(), gaugeTrack.getWidth() * clampedVal, gaugeTrack.getHeight());
                                        g.setColour(itemCol.withAlpha(0.85f));
                                        g.fillRoundedRectangle(fillBar, 3.0f);

                                        const int pct = juce::roundToInt(clampedVal * 100.0f);
                                        g.setFont(juce::Font(8.5f, juce::Font::bold));
                                        g.setColour(MiMOLookAndFeel::fountainInk);
                                        g.drawText(juce::String(pct) + "%", static_cast<int>(pctX), static_cast<int>(row.getY()), static_cast<int>(pctW), static_cast<int>(row.getHeight()), juce::Justification::centredRight);
                                    }
                                    else
                                    {
                                        g.setFont(juce::Font(8.0f, juce::Font::plain));
                                        g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.5f));
                                        g.drawText(itemActive ? "--" : "off", static_cast<int>(pctX), static_cast<int>(row.getY()), static_cast<int>(pctW), static_cast<int>(row.getHeight()), juce::Justification::centredRight);
                                    }
                                }

                                curY += 26.0f;
                            }
                        }
                    }
                    curY += 4.0f;
                }
            }
            else
            {
                float my = box.getY() + 138.0f;
                for (const auto& m : node.modOutputs)
                {
                    juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 25.0f);
                    g.setColour(m.colour.withAlpha(0.22f));
                    g.fillRoundedRectangle(row, 4.0f);
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.drawRoundedRectangle(row, 4.0f, 1.0f);

                    g.setFont(juce::Font(11.0f, juce::Font::bold));
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.drawText(m.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 110, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);
                    g.setFont(juce::Font(9.5f, juce::Font::plain));
                    g.setColour(MiMOLookAndFeel::inkMuted);
                    g.drawText("(tira verso manopola)", static_cast<int>(row.getX() + 105), static_cast<int>(row.getY()), static_cast<int>(row.getWidth() - 110), static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                    my += 30.0f;
                }
            }
        }
        else
        {
            // Modulo Batteria Piezo
            juce::Rectangle<float> instBox(box.getX() + 12.0f, box.getY() + 46.0f, box.getWidth() - 24.0f, 42.0f);
            MiMOLookAndFeel::drawSketchBox(g, instBox, seed + 2, node.hasInstrument ? node.crayonColour : juce::Colours::transparentBlack,
                                           0.35f, juce::Colours::transparentBlack, 1.3f, !node.hasInstrument);
            g.setColour(MiMOLookAndFeel::fountainInk);
            if (node.hasInstrument)
            {
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                g.drawText(node.instrumentName + " (" + node.instrumentEngine + ")",
                           instBox.reduced(8, 4).toNearestInt(), juce::Justification::centred);
            }
            else
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(11.5f, juce::Font::italic));
                g.drawText("Trascina qui uno Strumento ->", instBox.toNearestInt(), juce::Justification::centred);
            }

            if (!node.outputGroups.empty())
            {
                float curY = box.getY() + 96.0f;
                for (size_t gIdx = 0; gIdx < node.outputGroups.size(); ++gIdx)
                {
                    const auto& grp = node.outputGroups[gIdx];
                    juce::Rectangle<float> hdrBox(box.getX() + 10.0f, curY, box.getWidth() - 20.0f, 24.0f);

                    int activeCablesInGrp = 0;
                    for (const auto& item : grp.items)
                        for (const auto& c : cables)
                            if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                activeCablesInGrp++;

                    g.setColour(juce::Colour(0xFF1E2129));
                    g.fillRoundedRectangle(hdrBox, 4.0f);
                    g.setColour(activeCablesInGrp > 0 ? MiMOLookAndFeel::crayonOchre : (isDarkMode ? juce::Colours::white.withAlpha(0.18f) : juce::Colours::black.withAlpha(0.15f)));
                    g.drawRoundedRectangle(hdrBox, 4.0f, 1.0f);

                    // Freccia toggle [▼] / [▶]
                    g.setColour(MiMOLookAndFeel::crayonOchre);
                    g.setFont(juce::Font(10.0f, juce::Font::bold));
                    g.drawText(grp.isExpanded ? juce::String::charToString(0x25BC) : juce::String::charToString(0x25B6),
                               static_cast<int>(hdrBox.getX() + 4), static_cast<int>(hdrBox.getY()), 14, static_cast<int>(hdrBox.getHeight()), juce::Justification::centred);

                    // Titolo gruppo
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.setFont(juce::Font(10.5f, juce::Font::bold));
                    g.drawText(grp.name, static_cast<int>(hdrBox.getX() + 20), static_cast<int>(hdrBox.getY()),
                               static_cast<int>(hdrBox.getWidth() - 75), static_cast<int>(hdrBox.getHeight()), juce::Justification::centredLeft);

                    // Badge conteggio
                    g.setColour(MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(9.0f, juce::Font::plain));
                    g.drawText("(" + juce::String(grp.items.size()) + ")",
                               static_cast<int>(hdrBox.getRight() - 50), static_cast<int>(hdrBox.getY()), 22, static_cast<int>(hdrBox.getHeight()), juce::Justification::centredRight);

                    if (!grp.isExpanded && activeCablesInGrp > 0)
                    {
                        g.setColour(MiMOLookAndFeel::crayonOchre);
                        g.setFont(juce::Font(8.5f, juce::Font::bold));
                        g.drawText(juce::String(activeCablesInGrp) + " cavo",
                                   static_cast<int>(hdrBox.getRight() - 28), static_cast<int>(hdrBox.getY()), 26, static_cast<int>(hdrBox.getHeight()), juce::Justification::centred);
                    }

                    curY += 26.0f;

                    if (grp.isExpanded)
                    {
                        for (const auto& item : grp.items)
                        {
                            juce::Rectangle<float> row(box.getX() + 10.0f, curY, box.getWidth() - 20.0f, 26.0f);
                            const juce::Colour itemCol = item.isTrigger ? juce::Colour(0xFFFF7A18) : item.colour;

                            g.setColour(itemCol.withAlpha(0.20f));
                            g.fillRoundedRectangle(row, 4.0f);
                            g.setColour(itemCol.withAlpha(0.90f));
                            g.drawRoundedRectangle(row, 4.0f, 1.0f);

                            g.setFont(juce::Font(10.0f, juce::Font::bold));
                            g.setColour(MiMOLookAndFeel::fountainInk);
                            g.drawText(item.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 110, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                            if (item.isTrigger)
                            {
                                // Tasto Test Trigger
                                juce::Rectangle<float> testBtn(row.getRight() - 55.0f, row.getY() + 3.0f, 38.0f, 20.0f);
                                MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, MiMOLookAndFeel::crayonOchre, 0.40f);
                                g.setColour(MiMOLookAndFeel::fountainInk);
                                g.setFont(juce::Font(9.0f, juce::Font::bold));
                                g.drawText("TEST", testBtn.toNearestInt(), juce::Justification::centred);
                            }
                            else
                            {
                                g.setFont(juce::Font(8.5f, juce::Font::plain));
                                g.setColour(MiMOLookAndFeel::inkMuted);
                                g.drawText(item.hint, static_cast<int>(row.getX() + 115), static_cast<int>(row.getY()), static_cast<int>(row.getWidth() - 118), static_cast<int>(row.getHeight()), juce::Justification::centredLeft);
                            }

                            curY += 30.0f;
                        }
                    }
                    curY += 4.0f;
                }
            }
            else
            {
                // Fallback classico se outputGroups è vuoto
                juce::Rectangle<float> trigRow(box.getX() + 12.0f, box.getY() + 96.0f, box.getWidth() - 24.0f, 26.0f);
                g.setColour(juce::Colour(0xFFFF7A18).withAlpha(0.18f));
                g.fillRoundedRectangle(trigRow, 4.0f);
                g.setColour(juce::Colour(0xFFFF7A18));
                g.drawRoundedRectangle(trigRow, 4.0f, 1.4f);

                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText("Impulso Colpo (Trigger)", static_cast<int>(trigRow.getX() + 8), static_cast<int>(trigRow.getY()), 140, static_cast<int>(trigRow.getHeight()), juce::Justification::centredLeft);

                juce::Rectangle<float> testBtn(trigRow.getRight() - 65.0f, trigRow.getY() + 3.0f, 40.0f, 20.0f);
                MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, MiMOLookAndFeel::crayonOchre, 0.40f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(9.5f, juce::Font::bold));
                g.drawText("TEST", testBtn.toNearestInt(), juce::Justification::centred);

                float my = box.getY() + 128.0f;
                for (const auto& m : node.modOutputs)
                {
                    juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 26.0f);
                    g.setColour(m.colour.withAlpha(0.20f));
                    g.fillRoundedRectangle(row, 4.0f);
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.drawRoundedRectangle(row, 4.0f, 1.2f);

                    g.setFont(juce::Font(11.0f, juce::Font::bold));
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.drawText(m.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 90, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);
                    g.setFont(juce::Font(9.5f, juce::Font::plain));
                    g.setColour(MiMOLookAndFeel::inkMuted);
                    g.drawText("(tira cavo verso manopola)", static_cast<int>(row.getX() + 90), static_cast<int>(row.getY()), static_cast<int>(row.getWidth() - 95), static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                    my += 32.0f;
                }
            }
        }

        // Tendina selettore dispositivo per moduli integrati
        if (node.isIntegratedMic || node.isIntegratedCamera || node.id == "hw-mic" || node.id == "hw-webcam" || node.id == "hw-eye")
        {
            const auto devRow = getIntegratedDeviceRowBounds(box);
            const bool isMicNode = (node.isIntegratedMic || node.id == "hw-mic");
            const bool isOpen = isMicNode ? micDropdownOpen : camDropdownOpen;
            const juce::String activeCamLabel = useSmartphoneEye ? juce::String("Smartphone (QR AI)") : getCameraDeviceLabel();
            const juce::String devLabel = isMicNode ? ("Mic: " + getMicDeviceLabel()) : ("Cam: " + activeCamLabel);

            MiMOLookAndFeel::drawSketchBox(g, devRow, seed + 303,
                                           isOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg,
                                           isOpen ? 0.45f : 0.75f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText(devLabel, static_cast<int>(devRow.getX() + 6.0f), static_cast<int>(devRow.getY()),
                       static_cast<int>(devRow.getWidth() - 24.0f), static_cast<int>(devRow.getHeight()),
                       juce::Justification::centredLeft);

            juce::Path arrow;
            const float ax = devRow.getRight() - 11.0f;
            const float ay = devRow.getCentreY();
            if (isOpen)
                arrow.addTriangle(ax - 4.0f, ay + 2.5f, ax + 4.0f, ay + 2.5f, ax, ay - 2.5f);
            else
                arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 2.5f);
            g.fillPath(arrow);
        }
    }
    else if (node.type == NodeType::NestedEffect)
    {
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

        MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.52f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

        if (node.isVerticalFlow)
        {
            juce::Rectangle<float> topPlug(box.getX() + 18.0f, box.getY() - 4.0f, 22.0f, 8.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillRoundedRectangle(topPlug, 2.0f);

            juce::Rectangle<float> botPlug(box.getX() + 18.0f, box.getBottom() - 4.0f, 22.0f, 8.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillRoundedRectangle(botPlug, 2.0f);
        }
        else
        {
            // Etichette jacks chiare e pulite
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawText("IN", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 25), 24, 14, juce::Justification::centredLeft);
            g.drawText("OUT", static_cast<int>(box.getRight() - 32), static_cast<int>(box.getY() + 25), 24, 14, juce::Justification::centredRight);
        }

        // Close button [X]
        juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
        MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

        // Pulsante Power / Bypass con LED luminoso
        const auto pwrBtn = CanvasGraphHelper::getBypassButtonBounds(box);
        MiMOLookAndFeel::drawSketchBox(g, pwrBtn, seed + 101, node.isBypassed ? MiMOLookAndFeel::paperBg : MiMOLookAndFeel::crayonSage, node.isBypassed ? 0.35f : 0.80f);
        g.setColour(node.isBypassed ? MiMOLookAndFeel::inkMuted.withAlpha(0.4f) : juce::Colour(0xFF55E678));
        g.fillEllipse(pwrBtn.getCentreX() - 3.5f, pwrBtn.getCentreY() - 3.5f, 7.0f, 7.0f);

        // Mini VU-Meter real-time
        const auto vuBox = CanvasGraphHelper::getVuMeterBounds(box);
        g.setColour(isDarkMode ? juce::Colour(0xFF222630) : juce::Colour(0xFFE4E8EE));
        g.fillRoundedRectangle(vuBox, 2.0f);
        const float peakVal = juce::jlimit(0.0f, 1.0f, node.peakLevel);
        if (peakVal > 0.005f)
        {
            const float w = vuBox.getWidth() * peakVal;
            juce::Rectangle<float> fill(vuBox.getX(), vuBox.getY(), w, vuBox.getHeight());
            const juce::Colour peakCol = peakVal > 0.85f ? MiMOLookAndFeel::crayonCherry : (peakVal > 0.6f ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::crayonSage);
            g.setColour(peakCol);
            g.fillRoundedRectangle(fill, 2.0f);
        }

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(node.label, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 10), 160, 20, juce::Justification::centredLeft);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.5f, juce::Font::plain));
        g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 28), 190, 16, juce::Justification::centredLeft);

        juce::Rectangle<float> expBadge(box.getRight() - 92.0f, box.getY() + 10.0f, 62.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, expBadge, seed + 3, MiMOLookAndFeel::crayonOchre, 0.4f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Espandi", expBadge.toNearestInt(), juce::Justification::centred);

        // Disegna le manopole
        for (const auto& kh : collectCanvasKnobs())
        {
            if (kh.nodeId != node.id)
                continue;

            for (const auto& b : node.innerBlocks)
            {
                if (b.id != kh.blockId)
                    continue;
                for (const auto& p : b.params)
                {
                    if (p.key != kh.paramKey)
                        continue;

                    bool isModulated = false;
                    juce::Colour modCol = MiMOLookAndFeel::crayonOchre;
                    const juce::String targetPin = "param:" + b.id + "." + p.key;
                    for (const auto& c : cables)
                    {
                        if (c.targetNodeId == node.id && c.targetPinId == targetPin)
                        {
                            isModulated = true;
                            modCol = c.colour;
                            break;
                        }
                    }

                    MiMOLookAndFeel::drawCrayonKnob(
                        g, kh.worldBounds, p.getNormalized(), node.crayonColour,
                        p.label, p.getFormattedValue(),
                        (node.id + p.key).hashCode(), isModulated, modCol
                    );
                }
            }
        }
    }
    else if (node.type == NodeType::InstrumentNode)
    {
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

        MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.52f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

        // Close button [X]
        juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
        MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

        // Pulsante Power / Mute con LED luminoso
        const auto pwrBtn = CanvasGraphHelper::getBypassButtonBounds(box);
        MiMOLookAndFeel::drawSketchBox(g, pwrBtn, seed + 101, node.isBypassed ? MiMOLookAndFeel::paperBg : MiMOLookAndFeel::crayonSage, node.isBypassed ? 0.35f : 0.80f);
        g.setColour(node.isBypassed ? MiMOLookAndFeel::inkMuted.withAlpha(0.4f) : juce::Colour(0xFF55E678));
        g.fillEllipse(pwrBtn.getCentreX() - 3.5f, pwrBtn.getCentreY() - 3.5f, 7.0f, 7.0f);

        // Mini VU-Meter real-time
        const auto vuBox = CanvasGraphHelper::getVuMeterBounds(box);
        g.setColour(isDarkMode ? juce::Colour(0xFF222630) : juce::Colour(0xFFE4E8EE));
        g.fillRoundedRectangle(vuBox, 2.0f);
        const float peakVal = juce::jlimit(0.0f, 1.0f, node.peakLevel);
        if (peakVal > 0.005f)
        {
            const float w = vuBox.getWidth() * peakVal;
            juce::Rectangle<float> fill(vuBox.getX(), vuBox.getY(), w, vuBox.getHeight());
            const juce::Colour peakCol = peakVal > 0.85f ? MiMOLookAndFeel::crayonCherry : (peakVal > 0.6f ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::crayonSage);
            g.setColour(peakCol);
            g.fillRoundedRectangle(fill, 2.0f);
        }

        const bool isLfo = (node.id.containsIgnoreCase("lfo") || node.id.containsIgnoreCase("mod_lfo"));
        const bool isFollower = (node.id.containsIgnoreCase("follower") || node.id.containsIgnoreCase("env"));
        const bool isScale = (node.id.containsIgnoreCase("scale") || node.id.containsIgnoreCase("quantiz"));
        const bool isLooper = (node.id.containsIgnoreCase("looper") || node.id.containsIgnoreCase("tape_loop"));
        const bool isUtilityNode = isLfo || isFollower || isScale || isLooper || node.id.startsWithIgnoreCase("ut-") || node.id.startsWithIgnoreCase("mod-");

        // Badge pillola pastello
        juce::Rectangle<float> badge(box.getX() + 12.0f, box.getY() + 8.0f, 75.0f, 18.0f);
        g.setColour(node.crayonColour.withAlpha(0.22f));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(node.crayonColour);
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(isLfo ? "LFO CV" : (isUtilityNode ? "UTILITY" : "STRUMENTO"), badge.toNearestInt(), juce::Justification::centred);

        // Nome strumento e categoria
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(14.5f, juce::Font::bold));
        g.drawText(node.label, static_cast<int>(box.getX() + 94), static_cast<int>(box.getY() + 8), static_cast<int>(box.getWidth() - 175), 18, juce::Justification::centredLeft);

        const bool isPerc = (node.id.containsIgnoreCase("drum") || node.id.containsIgnoreCase("percuss"));
        if (isPerc)
        {
            // Pulsante Mode Toggle [⚙ ARRANGIA] o [⚙ SEMPLICE]
            const juce::Rectangle<float> modeBtn(box.getRight() - 145.0f, box.getY() + 8.0f, 90.0f, 18.0f);
            MiMOLookAndFeel::drawSketchBox(g, modeBtn, seed + 102, node.isDrumAdvanced ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::paperBg, node.isDrumAdvanced ? 0.65f : 0.35f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(node.isDrumAdvanced ? "⚙ ARRANGIA" : "⚙ SEMPLICE", modeBtn.toNearestInt(), juce::Justification::centred);

            if (node.isDrumAdvanced)
            {
                // Barra Selettore Preset
                static const char* presetNames[] = { "808 Studio", "Acoustic Warm", "Electro Tight", "Lofi Dust" };
                const juce::Rectangle<float> presetBar(box.getX() + 52.0f, box.getY() + 32.0f, box.getWidth() - 100.0f, 18.0f);
                MiMOLookAndFeel::drawSketchBox(g, presetBar, seed + 103, MiMOLookAndFeel::crayonSage, 0.35f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(9.5f, juce::Font::bold));
                g.drawText("Preset: " + juce::String(presetNames[node.drumPresetIdx % 4]) + " ▾", presetBar.toNearestInt(), juce::Justification::centred);

                // 4 Tab Voci: Cassa | Rullante | Hat | Tom
                static const char* voiceNames[] = { "Cassa", "Rull.", "Hat", "Tom" };
                const float tabW = (box.getWidth() - 100.0f) / 4.0f;
                for (int v = 0; v < 4; ++v)
                {
                    const juce::Rectangle<float> tabBox(box.getX() + 52.0f + static_cast<float>(v) * tabW, box.getY() + 54.0f, tabW - 2.0f, 20.0f);
                    const bool isSel = (node.drumEditVoice == v);
                    MiMOLookAndFeel::drawSketchBox(g, tabBox, seed + 105 + v, isSel ? node.crayonColour : MiMOLookAndFeel::paperBg, isSel ? 0.70f : 0.25f);
                    g.setColour(isSel ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(9.0f, isSel ? juce::Font::bold : juce::Font::plain));
                    g.drawText(voiceNames[v], tabBox.toNearestInt(), juce::Justification::centred);
                }
            }
            else
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(11.0f, juce::Font::plain));
                g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 29), static_cast<int>(box.getWidth() - 50), 14, juce::Justification::centredLeft);

                // 4 Pulsanti Quick-Test / Audition
                static const char* voiceNames[] = { "Cassa", "Rullante", "Hi-Hat", "Tom" };
                const float btnW = (box.getWidth() - 96.0f) / 4.0f;
                for (int v = 0; v < 4; ++v)
                {
                    const juce::Rectangle<float> btnBox(box.getX() + 50.0f + static_cast<float>(v) * btnW, box.getY() + 48.0f, btnW - 3.0f, 22.0f);
                    MiMOLookAndFeel::drawSketchBox(g, btnBox, seed + 105 + v, MiMOLookAndFeel::crayonOchre, 0.35f);
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.setFont(juce::Font(9.5f, juce::Font::bold));
                    g.drawText(voiceNames[v], btnBox.toNearestInt(), juce::Justification::centred);
                }
            }

            // Etichette jacks percussioni dedicate lungo il bordo sinistro
            g.setFont(juce::Font(8.0f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::crayonCherry);
            g.drawText("CASSA", static_cast<int>(box.getX() + 6), static_cast<int>(box.getY() + 44), 38, 12, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::crayonPeach);
            g.drawText("RULL", static_cast<int>(box.getX() + 6), static_cast<int>(box.getY() + 66), 38, 12, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::crayonCyan);
            g.drawText("HAT", static_cast<int>(box.getX() + 6), static_cast<int>(box.getY() + 88), 38, 12, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::crayonMint);
            g.drawText("TOM", static_cast<int>(box.getX() + 6), static_cast<int>(box.getY() + 110), 38, 12, juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xFFFF7A18));
            g.drawText("ALL", static_cast<int>(box.getX() + 6), static_cast<int>(box.getY() + 132), 38, 12, juce::Justification::centredLeft);
        }
        else if (isLfo)
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 30), static_cast<int>(box.getWidth() - 50), 15, juce::Justification::centredLeft);

            // Jack CV Out (Triangolo a destra, y + 36)
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::crayonPeach);
            g.drawText("CV", static_cast<int>(box.getRight() - 36), static_cast<int>(box.getY() + 29), 28, 14, juce::Justification::centredRight);

            // Jack Clock Trigger Out (Rombo a destra, y + 68)
            g.setColour(juce::Colour(0xFFFF7A18));
            g.drawText("TRIG", static_cast<int>(box.getRight() - 40), static_cast<int>(box.getY() + 61), 32, 14, juce::Justification::centredRight);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 30), static_cast<int>(box.getWidth() - 50), 15, juce::Justification::centredLeft);

            // Etichette jacks standard
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawText("IN", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xFFFF7A18));
            g.drawText("TRIG", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 52), 32, 14, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText("OUT", static_cast<int>(box.getRight() - 32), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredRight);
        }

        // Manopole dello strumento
        for (const auto& kh : collectCanvasKnobs())
        {
            if (kh.nodeId != node.id)
                continue;

            for (const auto& p : node.directParams)
            {
                if (p.key != kh.paramKey)
                    continue;

                bool isModulated = false;
                juce::Colour modCol = MiMOLookAndFeel::crayonOchre;
                const juce::String targetPin = "param:" + kh.blockId + "." + p.key;
                for (const auto& c : cables)
                {
                    if (c.targetNodeId == node.id && c.targetPinId == targetPin)
                    {
                        isModulated = true;
                        modCol = c.colour;
                        break;
                    }
                }

                MiMOLookAndFeel::drawCrayonKnob(
                    g, kh.worldBounds, p.getNormalized(), node.crayonColour,
                    p.label, p.getFormattedValue(),
                    (node.id + p.key).hashCode(), isModulated, modCol
                );
            }
        }
    }
    else if (node.type == NodeType::MasterOut)
    {
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

        MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.45f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.drawText("IN", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredLeft);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(14.5f, juce::Font::bold));
        g.drawText(node.label, static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 10), 140, 18, juce::Justification::centred);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText(node.subtitle, static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 27), 140, 15, juce::Justification::centred);

        if (!node.directParams.empty())
        {
            const auto& p = node.directParams[0];
            juce::Rectangle<float> kb(box.getX() + 25.0f, box.getY() + 52.0f, box.getWidth() - 50.0f, 88.0f);
            MiMOLookAndFeel::drawCrayonKnob(g, kb, p.getNormalized(), node.crayonColour,
                                            p.label, p.getFormattedValue(), seed + 9);
        }
    }
}

void MainComponent::paintCanvasDropdowns(juce::Graphics& g)
{
    if (!camDropdownOpen && !micDropdownOpen)
        return;

    for (const auto& node : nodes)
    {
        if (node.type != NodeType::HardwareModule)
            continue;

        const bool isMicNode = (node.isIntegratedMic || node.id == "hw-mic");
        const bool isCamNode = (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye");
        if (isMicNode && !micDropdownOpen)
            continue;
        if (isCamNode && !camDropdownOpen)
            continue;
        if (!isMicNode && !isCamNode)
            continue;

        const juce::Rectangle<float> box(node.pos.x, node.pos.y, node.size.x, node.size.y);
        const auto devRow = getIntegratedDeviceRowBounds(box);
        const auto options = isMicNode ? getMicDropdownOptions() : getCameraDropdownOptions();
        if (options.isEmpty())
            continue;

        const float itemH = 22.0f;
        const float menuH = static_cast<float>(options.size()) * itemH + 6.0f;
        const juce::Rectangle<float> menuBox(devRow.getX(), devRow.getBottom() + 2.0f, devRow.getWidth(), menuH);

        // Ombra morbida sotto la tendina
        g.setColour(juce::Colours::black.withAlpha(isDarkMode ? 0.55f : 0.22f));
        g.fillRoundedRectangle(menuBox.translated(0.0f, 3.0f), 5.0f);

        // Sfondo opaco carta coerente con Giorno/Notte
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(menuBox, 5.0f);
        MiMOLookAndFeel::drawSketchBox(g, menuBox, node.id.hashCode() + 505,
                                       juce::Colours::transparentBlack, 0.0f,
                                       MiMOLookAndFeel::fountainInk, 1.6f, false, MiMOLookAndFeel::paperCard);

        const int activeIdx = isMicNode ? std::max(0, micDeviceIndex.load()) : std::max(0, cameraDeviceIndex.load());

        for (int i = 0; i < options.size(); ++i)
        {
            const juce::Rectangle<float> itemRect(menuBox.getX() + 3.0f,
                                                  menuBox.getY() + 3.0f + static_cast<float>(i) * itemH,
                                                  menuBox.getWidth() - 6.0f,
                                                  itemH);

            bool isSelected = false;
            if (isMicNode)
            {
                isSelected = (i == activeIdx);
            }
            else
            {
                const auto items = getVideoSourceItems();
                if (i < items.size())
                {
                    isSelected = items[i].isSmartphone
                        ? useSmartphoneEye
                        : (!useSmartphoneEye && items[i].pcDeviceIdx == activeIdx);
                }
            }

            if (isSelected)
            {
                g.setColour((isMicNode ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::crayonCyan).withAlpha(0.45f));
                g.fillRoundedRectangle(itemRect, 3.5f);
            }

            // Separatore leggero tra le voci
            if (i > 0)
            {
                g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.20f));
                g.drawLine(itemRect.getX() + 4.0f, itemRect.getY(), itemRect.getRight() - 4.0f, itemRect.getY(), 0.8f);
            }

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.5f, isSelected ? juce::Font::bold : juce::Font::plain));
            const juce::String prefix = isSelected ? "* " : "   ";
            g.drawText(prefix + options[i], itemRect.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }
}

void MainComponent::paintRightSidebar(juce::Graphics& g)
{
    // 1. Quadratino con freccina a sinistra (cambia verso quando apriamo o chiudiamo la zona)
    const auto toggleRect = getSidebarToggleBounds();
    MiMOLookAndFeel::drawSketchBox(g, toggleRect, 301,
                                   sidebarOpen ? MiMOLookAndFeel::paperCard : MiMOLookAndFeel::crayonOchre,
                                   sidebarOpen ? 0.35f : 0.65f, juce::Colours::transparentBlack, 1.6f);

    {
        const float cx = toggleRect.getCentreX();
        const float cy = toggleRect.getCentreY();
        juce::Path p;
        if (sidebarOpen)
        {
            // Freccina verso destra (chiude la zona verso il margine)
            p.addTriangle(cx - 3.5f, cy - 5.5f,
                          cx - 3.5f, cy + 5.5f,
                          cx + 4.5f, cy);
        }
        else
        {
            // Freccina verso sinistra (apre la zona verso l'interno)
            p.addTriangle(cx + 3.5f, cy - 5.5f,
                          cx + 3.5f, cy + 5.5f,
                          cx - 4.5f, cy);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(p);
    }

    if (!sidebarOpen)
        return;

    // =========================================================================
    // 2. PULSANTE FOTOCAMERA POLAROID
    // (Agganciato a sinistra subito a fianco del quadratino con la freccia)
    // =========================================================================
    const auto camBtn = getCameraButtonBounds();
    MiMOLookAndFeel::drawSketchBox(g, camBtn, 302, MiMOLookAndFeel::crayonOchre, 0.60f, juce::Colours::transparentBlack, 1.8f);
    const juce::Point<float> c = camBtn.getCentre();
    MiMOLookAndFeel::drawCameraIcon(g, { c.x - 14.0f, c.y - 11.0f, 28.0f, 22.0f });

    // =========================================================================
    // 3. PULSANTE INGRANAGGIO IMPOSTAZIONI (A destra)
    // =========================================================================
    const auto gearBtn = getSettingsButtonBounds();
    MiMOLookAndFeel::drawSketchBox(g, gearBtn, 304,
                                   settingsOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                   settingsOpen ? 0.60f : 0.35f, juce::Colours::transparentBlack, 1.6f);

    // Disegna l'icona dell'ingranaggio in grafica vettoriale
    {
        const float gx = gearBtn.getCentreX();
        const float gy = gearBtn.getCentreY();
        const float rOut = 9.0f;
        const float rIn  = 6.2f;
        const float rHole = 3.0f;
        juce::Path gear;
        const int teeth = 6;
        for (int i = 0; i < teeth; ++i)
        {
            const float a0 = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(teeth);
            const float a1 = a0 + 0.22f;
            const float a2 = a0 + 0.44f;
            const float a3 = a0 + 0.66f;

            if (i == 0)
                gear.startNewSubPath(gx + rIn * std::cos(a0), gy + rIn * std::sin(a0));
            else
                gear.lineTo(gx + rIn * std::cos(a0), gy + rIn * std::sin(a0));

            gear.lineTo(gx + rOut * std::cos(a1), gy + rOut * std::sin(a1));
            gear.lineTo(gx + rOut * std::cos(a2), gy + rOut * std::sin(a2));
            gear.lineTo(gx + rIn * std::cos(a3), gy + rIn * std::sin(a3));
        }
        gear.closeSubPath();
        gear.addEllipse(gx - rHole, gy - rHole, rHole * 2.0f, rHole * 2.0f);
        gear.setUsingNonZeroWinding(false);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(gear);
    }

    // =========================================================================
    // AREA SCROLLABILE VERTICALE (SEZIONI 0, 1, 2, 3)
    // =========================================================================
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(getSidebarViewportBounds().toNearestInt());

    // =========================================================================
    // 0. SEZIONE MODULI INTEGRATI (PC / Laptop)
    // =========================================================================
    const auto integHeader = getIntegratedHeaderBounds();
    MiMOLookAndFeel::drawSketchBox(g, integHeader, 310,
                                   integratedSectionOpen ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::paperCard,
                                   integratedSectionOpen ? 0.45f : 0.25f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("0. MODULI INTEGRATI (PC)", static_cast<int>(integHeader.getX() + 14), static_cast<int>(integHeader.getY()),
               200, static_cast<int>(integHeader.getHeight()), juce::Justification::centredLeft);

    // Freccia apertura/chiusura
    {
        const float ax = integHeader.getRight() - 20.0f;
        const float ay = integHeader.getCentreY();
        juce::Path arrow;
        if (integratedSectionOpen)
        {
            arrow.addTriangle(ax - 5.0f, ay - 3.0f,
                              ax + 5.0f, ay - 3.0f,
                              ax,        ay + 4.0f);
        }
        else
        {
            arrow.addTriangle(ax - 3.0f, ay - 5.0f,
                              ax - 3.0f, ay + 5.0f,
                              ax + 4.0f, ay);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(arrow);
    }

    if (integratedSectionOpen)
    {
        const auto integBody = getIntegratedBodyBounds();
        MiMOLookAndFeel::drawSketchBox(g, integBody, 311, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

        const float cardX = integBody.getX() + 8.0f;
        const float cardW = integBody.getWidth() - 16.0f;

        // ---------------------------------------------------------------------
        // CARD 1: BATTERIA (PIEZO ROCKPI P1)
        // ---------------------------------------------------------------------
        const bool drumInCanvas = isDrumInCanvas();
        const float drumY = integBody.getY() + 6.0f;
        const juce::Rectangle<float> drumCard(cardX, drumY, cardW, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, drumCard, 312,
                                       drumInCanvas ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                       drumInCanvas ? 0.35f : 0.15f, juce::Colours::transparentBlack, 1.4f);

        // Titolo e Icona Batteria
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("[P1] Modulo Batteria", static_cast<int>(cardX + 10), static_cast<int>(drumY + 5),
                   static_cast<int>(cardW - 88), 16, juce::Justification::centredLeft);

        // Bottone Attivazione / Metti nel Canvas
        const auto drumSw = getDrumSwitchBounds();
        MiMOLookAndFeel::drawSketchBox(g, drumSw, 313,
                                       drumInCanvas ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonOchre,
                                       drumInCanvas ? 0.70f : 0.40f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.drawText(drumInCanvas ? "NEL CANVAS" : "+ NEL CANVAS", drumSw.toNearestInt(), juce::Justification::centred);

        // Subtitle di stato hardware
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        if (drumInCanvas)
        {
            g.setColour(MiMOLookAndFeel::crayonSage);
            g.fillEllipse(cardX + 10.0f, drumY + 26.5f, 6.5f, 6.5f);
            g.drawText("Sensori Piezo - I2C Ch1", static_cast<int>(cardX + 20), static_cast<int>(drumY + 23), static_cast<int>(cardW - 30), 14, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawEllipse(cardX + 10.0f, drumY + 26.5f, 6.5f, 6.5f, 1.2f);
            g.drawText("Sensori Piezo (Disattivo)", static_cast<int>(cardX + 20), static_cast<int>(drumY + 23), static_cast<int>(cardW - 30), 14, juce::Justification::centredLeft);
        }

        // ---------------------------------------------------------------------
        // CARD 2: MICROFONO INTEGRATO PC (TRIGGER BATTERIA)
        // ---------------------------------------------------------------------
        const bool micInCanvas = isMicInCanvas();
        const float micY = integBody.getY() + 54.0f;
        const juce::Rectangle<float> micCard(cardX, micY, cardW, 44.0f);
        MiMOLookAndFeel::drawSketchBox(g, micCard, 314,
                                       micInCanvas ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::paperCard,
                                       micInCanvas ? 0.35f : 0.15f, juce::Colours::transparentBlack, 1.4f);

        // Titolo Microfono PC
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("[MIC] Microfono (Batteria)", static_cast<int>(cardX + 10), static_cast<int>(micY + 5),
                   static_cast<int>(cardW - 88), 16, juce::Justification::centredLeft);

        // Bottone Attivazione / Metti nel Canvas
        const auto micSw = getMicSwitchBounds();
        MiMOLookAndFeel::drawSketchBox(g, micSw, 315,
                                       micInCanvas ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonOchre,
                                       micInCanvas ? 0.70f : 0.40f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.drawText(micInCanvas ? "NEL CANVAS" : "+ NEL CANVAS", micSw.toNearestInt(), juce::Justification::centred);

        // Subtitle di stato hardware
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        if (micInCanvas)
        {
            g.setColour(MiMOLookAndFeel::crayonOchre);
            g.fillEllipse(cardX + 10.0f, micY + 26.5f, 6.5f, 6.5f);
            g.drawText("Laptop Mic: " + getMicDeviceLabel(), static_cast<int>(cardX + 20), static_cast<int>(micY + 23), static_cast<int>(cardW - 30), 14, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawEllipse(cardX + 10.0f, micY + 26.5f, 6.5f, 6.5f, 1.2f);
            g.drawText("Mic Spento (Metti nel Canvas)", static_cast<int>(cardX + 20), static_cast<int>(micY + 23), static_cast<int>(cardW - 30), 14, juce::Justification::centredLeft);
        }

        // ---------------------------------------------------------------------
        // CARD 3: CAMERA (WEBCAM PC / MODULO OCCHIO)
        // ---------------------------------------------------------------------
        const bool cameraInCanvas = isCameraInCanvas();
        const float camY = integBody.getY() + 102.0f;
        const float camH = cameraSettingsOpen ? 186.0f : 44.0f;
        const juce::Rectangle<float> camCard(cardX, camY, cardW, camH);
        MiMOLookAndFeel::drawSketchBox(g, camCard, 316,
                                       cameraInCanvas ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                       cameraInCanvas ? 0.35f : 0.15f, juce::Colours::transparentBlack, 1.4f);

        // Titolo e Icona Webcam / MiMO Eye
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("[P2] Modulo Occhio", static_cast<int>(cardX + 10), static_cast<int>(camY + 5),
                   static_cast<int>(cardW - 88), 16, juce::Justification::centredLeft);

        // Bottone Attivazione / Metti nel Canvas
        const auto camSw = getCameraSwitchBounds();
        MiMOLookAndFeel::drawSketchBox(g, camSw, 317,
                                       cameraInCanvas ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonCyan,
                                       cameraInCanvas ? 0.70f : 0.40f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.drawText(cameraInCanvas ? "NEL CANVAS" : "+ NEL CANVAS", camSw.toNearestInt(), juce::Justification::centred);

        // Subtitle di stato hardware
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        if (cameraInCanvas)
        {
            g.setColour(MiMOLookAndFeel::crayonCyan);
            g.fillEllipse(cardX + 10.0f, camY + 27.5f, 6.5f, 6.5f);
            const juce::String eyeDesc = (eyeServer && eyeServer->isClientConnected())
                ? eyeServer->getClientTypeDescription()
                : getCameraStatusText();
            g.drawText(eyeDesc, static_cast<int>(cardX + 20), static_cast<int>(camY + 24), static_cast<int>(cardW - 110), 14, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawEllipse(cardX + 10.0f, camY + 27.5f, 6.5f, 6.5f, 1.2f);
            g.drawText("Webcam Spenta (Metti nel Canvas)", static_cast<int>(cardX + 20), static_cast<int>(camY + 24), static_cast<int>(cardW - 110), 14, juce::Justification::centredLeft);
        }

        // Pulsante "QR & AI"
        const auto setBtn = getCameraSettingsButtonBounds();
        MiMOLookAndFeel::drawSketchBox(g, setBtn, 318,
                                       eyeDrawerOpen ? MiMOLookAndFeel::crayonLilac : MiMOLookAndFeel::paperCard,
                                       eyeDrawerOpen ? 0.55f : 0.35f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("QR & AI", setBtn.toNearestInt(), juce::Justification::centred);

        // Sottopannello Settaggi Avanzati (quando aperto)
        if (cameraSettingsOpen)
        {
            // Separatore orizzontale tratteggiato a penna
            g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.35f));
            const float dashLengths[2] = { 4.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(cardX + 10.0f, camY + 42.0f, cardX + cardW - 10.0f, camY + 42.0f),
                             dashLengths, 2, 1.0f);

            // Modalità di Tracciamento
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("Modo:", static_cast<int>(cardX + 10), static_cast<int>(camY + 46), 50, 18, juce::Justification::centredLeft);

            const char* modeNames[3] = { "Movimento", "Luce", "Presenza" };
            for (int m = 0; m < 3; ++m)
            {
                const auto pill = getCameraModePillBounds(m);
                const bool isSel = (static_cast<int>(cameraTrackingMode) == m);
                MiMOLookAndFeel::drawSketchBox(g, pill, 350 + m,
                                               isSel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                               isSel ? 0.60f : 0.25f, juce::Colours::transparentBlack, 1.1f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(isSel ? 9.5f : 9.0f, isSel ? juce::Font::bold : juce::Font::plain));
                g.drawText(modeNames[m], pill.toNearestInt(), juce::Justification::centred);
            }

            // Checkbox Specchia Video (Mirror)
            const auto mirBox = getCameraMirrorCheckboxBounds();
            const juce::Rectangle<float> checkSquare(mirBox.getX(), mirBox.getY() + 1.0f, 14.0f, 14.0f);
            MiMOLookAndFeel::drawSketchBox(g, checkSquare, 360,
                                           cameraMirror ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                           cameraMirror ? 0.50f : 0.25f, juce::Colours::transparentBlack, 1.2f);
            if (cameraMirror)
            {
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText("X", checkSquare.toNearestInt(), juce::Justification::centred);
            }
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText("Specchia orizzontalmente (Mirror)",
                       static_cast<int>(mirBox.getX() + 20.0f), static_cast<int>(mirBox.getY()),
                       static_cast<int>(mirBox.getWidth() - 20.0f), static_cast<int>(mirBox.getHeight()),
                       juce::Justification::centredLeft);

            // Mini-Anteprima Naïf Webcam (FRAME REALE DELLA WEBCAM)
            const juce::Rectangle<float> monitor(cardX + 10.0f, camY + 112.0f, cardW - 20.0f, 66.0f);
            MiMOLookAndFeel::drawSketchBox(g, monitor, 370,
                                           cameraInCanvas ? MiMOLookAndFeel::paperBg : MiMOLookAndFeel::paperCard,
                                           0.85f, juce::Colours::transparentBlack, 1.3f);

            if (cameraInCanvas)
            {
                juce::Image camFrame;
                {
                    juce::ScopedLock sl(cameraLock);
                    if (latestWebcamImage.isValid())
                        camFrame = latestWebcamImage.createCopy();
                }

                if (camFrame.isValid())
                {
                    g.saveState();
                    g.reduceClipRegion(monitor.reduced(2.0f).toNearestInt());
                    g.drawImage(camFrame, monitor.reduced(2.0f), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
                    g.setColour(MiMOLookAndFeel::crayonCyan.withAlpha(0.18f));
                    g.fillRect(monitor.reduced(2.0f));
                    g.restoreState();

                    // Disegna reticolo di tracciamento reale
                    const float cx = monitor.getCentreX() + realCameraCentroidX.load() * (monitor.getWidth() * 0.38f);
                    const float cy = monitor.getCentreY() + realCameraCentroidY.load() * (monitor.getHeight() * 0.35f);

                    g.setColour(MiMOLookAndFeel::crayonCyan);
                    g.drawEllipse(cx - 8.0f, cy - 8.0f, 16.0f, 16.0f, 1.3f);
                    g.drawLine(cx - 12.0f, cy, cx + 12.0f, cy, 1.0f);
                    g.drawLine(cx, cy - 12.0f, cx, cy + 12.0f, 1.0f);
                }
                else
                {
                    g.setColour(MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(10.0f, juce::Font::italic));
                    g.drawText("Avvio webcam...", monitor.toNearestInt(), juce::Justification::centred);
                }

                // Watermark e FPS
                g.setColour(MiMOLookAndFeel::crayonSage);
                g.fillEllipse(monitor.getX() + 8.0f, monitor.getY() + 7.0f, 6.0f, 6.0f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(9.0f, juce::Font::bold));
                g.drawText("LIVE - 30 FPS", static_cast<int>(monitor.getX() + 18), static_cast<int>(monitor.getY() + 4), 100, 12, juce::Justification::centredLeft);

                juce::String trModeStr = cameraTrackingMode == CameraTrackingMode::Motion ? "MOVIMENTO" : (cameraTrackingMode == CameraTrackingMode::Light ? "LUCE" : "POSIZIONE XY");
                g.drawText(trModeStr, static_cast<int>(monitor.getRight() - 90), static_cast<int>(monitor.getY() + 4), 84, 12, juce::Justification::centredRight);
            }
            else
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.0f, juce::Font::italic));
                g.drawText("Webcam spenta\n(Metti nel Canvas per avviare)", monitor.toNearestInt(), juce::Justification::centred);
            }
        }
    }

    // =========================================================================
    // 2. ZONA STRUMENTI
    // =========================================================================
    const auto instHeader = getInstrumentsHeaderBounds();
    MiMOLookAndFeel::drawSketchBox(g, instHeader, 320,
                                   instrumentsSectionOpen ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                   instrumentsSectionOpen ? 0.50f : 0.25f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("1. STRUMENTI (" + juce::String(instrumentCatalog.size()) + ")", static_cast<int>(instHeader.getX() + 14), static_cast<int>(instHeader.getY()),
               200, static_cast<int>(instHeader.getHeight()), juce::Justification::centredLeft);

    // Freccia giu (se aperto) o a destra (se chiuso)
    {
        const float ax = instHeader.getRight() - 20.0f;
        const float ay = instHeader.getCentreY();
        juce::Path arrow;
        if (instrumentsSectionOpen)
        {
            arrow.addTriangle(ax - 5.0f, ay - 3.0f,
                              ax + 5.0f, ay - 3.0f,
                              ax,        ay + 4.0f);
        }
        else
        {
            arrow.addTriangle(ax - 3.0f, ay - 5.0f,
                              ax - 3.0f, ay + 5.0f,
                              ax + 4.0f, ay);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(arrow);
    }

    if (instrumentsSectionOpen)
    {
        const auto instBody = getInstrumentsBodyBounds();
        MiMOLookAndFeel::drawSketchBox(g, instBody, 322, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

        const float x = instBody.getX() + 8.0f;
        const float w = instBody.getWidth() - 16.0f;
        for (size_t i = 0; i < instrumentCatalog.size(); ++i)
        {
            const auto& item = instrumentCatalog[i];
            const float y = instBody.getY() + 6.0f + static_cast<float>(i) * 40.0f;
            juce::Rectangle<float> row(x, y, w, 35.0f);
            MiMOLookAndFeel::drawSketchBox(g, row, 400 + static_cast<int>(i), item.colour, 0.45f, juce::Colours::transparentBlack, 1.4f);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.5f, juce::Font::bold));
            g.drawText(item.name, static_cast<int>(x + 10), static_cast<int>(y + 3), static_cast<int>(w - 65), 16, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(item.category + " - " + item.hint, static_cast<int>(x + 10), static_cast<int>(y + 18), static_cast<int>(w - 65), 14, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("[" + item.engine + "]", static_cast<int>(row.getRight() - 56), static_cast<int>(y + 8), 48, 18, juce::Justification::centredRight);
        }
    }

    // =========================================================================
    // 3. ZONA EFFETTI NESTED
    // =========================================================================
    const auto fxHeader = getEffectsHeaderBounds();
    MiMOLookAndFeel::drawSketchBox(g, fxHeader, 330,
                                   effectsSectionOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                   effectsSectionOpen ? 0.50f : 0.25f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("2. EFFETTI NESTED (" + juce::String(effectCatalog.size()) + ")", static_cast<int>(fxHeader.getX() + 10), static_cast<int>(fxHeader.getY()),
               200, static_cast<int>(fxHeader.getHeight()), juce::Justification::centredLeft);

    // Freccia giu (se aperto) o a destra (se chiuso)
    {
        const float ax = fxHeader.getRight() - 20.0f;
        const float ay = fxHeader.getCentreY();
        juce::Path arrow;
        if (effectsSectionOpen)
        {
            arrow.addTriangle(ax - 5.0f, ay - 3.0f,
                              ax + 5.0f, ay - 3.0f,
                              ax,        ay + 4.0f);
        }
        else
        {
            arrow.addTriangle(ax - 3.0f, ay - 5.0f,
                              ax - 3.0f, ay + 5.0f,
                              ax + 4.0f, ay);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(arrow);
    }

    if (effectsSectionOpen)
    {
        const auto fxBody = getEffectsBodyBounds();
        MiMOLookAndFeel::drawSketchBox(g, fxBody, 332, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

        const float x = fxBody.getX() + 8.0f;
        const float w = fxBody.getWidth() - 16.0f;
        for (size_t i = 0; i < effectCatalog.size(); ++i)
        {
            const auto& fx = effectCatalog[i];
            const float y = fxBody.getY() + 6.0f + static_cast<float>(i) * 42.0f;
            juce::Rectangle<float> row(x, y, w, 37.0f);
            MiMOLookAndFeel::drawSketchBox(g, row, 500 + static_cast<int>(i), fx.colour, 0.52f, juce::Colours::transparentBlack, 1.4f);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.5f, juce::Font::bold));
            g.drawText(fx.name, static_cast<int>(x + 10), static_cast<int>(y + 3), static_cast<int>(w - 70), 16, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(fx.category + " - " + fx.hint, static_cast<int>(x + 10), static_cast<int>(y + 19), static_cast<int>(w - 70), 14, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("+ Cavo", static_cast<int>(row.getRight() - 56), static_cast<int>(y + 9), 48, 18, juce::Justification::centredRight);
        }
    }

    // =========================================================================
    // 4. ZONA UTILITY & CONTROLLO
    // =========================================================================
    const auto utHeader = getUtilitiesHeaderBounds();
    MiMOLookAndFeel::drawSketchBox(g, utHeader, 340,
                                   utilitiesSectionOpen ? MiMOLookAndFeel::crayonMint : MiMOLookAndFeel::paperCard,
                                   utilitiesSectionOpen ? 0.50f : 0.25f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("3. UTILITY & CONTROLLO (" + juce::String(utilityCatalog.size()) + ")",
               static_cast<int>(utHeader.getX() + 10), static_cast<int>(utHeader.getY()),
               220, static_cast<int>(utHeader.getHeight()), juce::Justification::centredLeft);

    // Freccia giu (se aperto) o a destra (se chiuso)
    {
        const float ax = utHeader.getRight() - 20.0f;
        const float ay = utHeader.getCentreY();
        juce::Path arrow;
        if (utilitiesSectionOpen)
        {
            arrow.addTriangle(ax - 5.0f, ay - 3.0f,
                              ax + 5.0f, ay - 3.0f,
                              ax,        ay + 4.0f);
        }
        else
        {
            arrow.addTriangle(ax - 3.0f, ay - 5.0f,
                              ax - 3.0f, ay + 5.0f,
                              ax + 4.0f, ay);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(arrow);
    }

    if (utilitiesSectionOpen)
    {
        const auto utBody = getUtilitiesBodyBounds();
        MiMOLookAndFeel::drawSketchBox(g, utBody, 342, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

        const float x = utBody.getX() + 8.0f;
        const float w = utBody.getWidth() - 16.0f;
        for (size_t i = 0; i < utilityCatalog.size(); ++i)
        {
            const auto& ut = utilityCatalog[i];
            const float y = utBody.getY() + 6.0f + static_cast<float>(i) * 40.0f;
            juce::Rectangle<float> row(x, y, w, 35.0f);
            MiMOLookAndFeel::drawSketchBox(g, row, 600 + static_cast<int>(i), ut.colour, 0.48f, juce::Colours::transparentBlack, 1.4f);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.5f, juce::Font::bold));
            g.drawText(ut.name, static_cast<int>(x + 10), static_cast<int>(y + 3), static_cast<int>(w - 65), 16, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(ut.category + " - " + ut.hint, static_cast<int>(x + 10), static_cast<int>(y + 18), static_cast<int>(w - 65), 14, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("[" + ut.engine + "]", static_cast<int>(row.getRight() - 56), static_cast<int>(y + 8), 48, 18, juce::Justification::centredRight);
        }
    }
    } // Fine blocco con ScopedSaveState e Scissoring Viewport

    // =========================================================================
    // INDICATORE BARRA DI SCORRIMENTO VERTICALE PASTELLO
    // =========================================================================
    const float maxScroll = getSidebarMaxScroll();
    if (maxScroll > 0.0f)
    {
        const float trackX = static_cast<float>(getWidth() - 6);
        const float trackY = 56.0f;
        const float trackH = std::max(20.0f, static_cast<float>(getHeight() - 72));

        g.setColour(MiMOLookAndFeel::paperCard.withAlpha(0.40f));
        g.fillRoundedRectangle(trackX, trackY, 4.0f, trackH, 2.0f);

        const float visibleRatio = juce::jlimit(0.12f, 0.90f, trackH / (trackH + maxScroll));
        const float thumbH = std::max(24.0f, trackH * visibleRatio);
        const float scrollNorm = juce::jlimit(0.0f, 1.0f, sidebarScrollY / maxScroll);
        const float thumbY = trackY + (trackH - thumbH) * scrollNorm;

        g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.75f));
        g.fillRoundedRectangle(trackX, thumbY, 4.0f, thumbH, 2.0f);
        g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(0.60f));
        g.drawRoundedRectangle(trackX, thumbY, 4.0f, thumbH, 2.0f, 1.0f);
    }

    // 4. Popup Impostazioni (aperto cliccando sull'icona dell'ingranaggio)
    if (settingsOpen)
    {
        paintSettingsPanel(g);
    }
}

void MainComponent::paintSettingsPanel(juce::Graphics& g)
{
    const auto panel = getSettingsPanelBounds();
    MiMOLookAndFeel::drawSketchBox(g, panel, 850, MiMOLookAndFeel::crayonCyan, 0.20f,
                                   MiMOLookAndFeel::crayonCyan, 2.0f, false, MiMOLookAndFeel::paperBg);

    // 1. Header Impostazioni
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.5f, juce::Font::bold));
    g.drawText("IMPOSTAZIONI HUB", static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 10), 180, 20, juce::Justification::centredLeft);

    // Tasto X chiudi
    juce::Rectangle<float> closeBtn(panel.getRight() - 28.0f, panel.getY() + 8.0f, 20.0f, 20.0f);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // 2. Pulsante Modalita Giorno / Notte
    juce::Rectangle<float> themeBtn(panel.getX() + 14.0f, panel.getY() + 34.0f, panel.getWidth() - 28.0f, 32.0f);
    MiMOLookAndFeel::drawSketchBox(g, themeBtn, 855,
                                   isDarkMode ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::crayonOchre, 0.55f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(isDarkMode ? "Modalita Notte [N] (Attiva)" : "Modalita Giorno [N] (Attiva)",
               themeBtn.toNearestInt(), juce::Justification::centred);

    // 3. Sezione Volume Interno Applicazione
    const float curVol = appMasterVolume.load();
    const int volPercent = static_cast<int>(std::round(curVol * 100.0f));
    const float volDb = juce::Decibels::gainToDecibels(curVol, -48.0f);
    juce::String dbStr = (volDb >= 0.05f ? "+" : "") + juce::String(volDb, 1) + " dB";

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawText("VOLUME INTERNO APP", static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 74), 140, 20, juce::Justification::centredLeft);

    juce::Colour volAccent = (curVol > 1.35f) ? MiMOLookAndFeel::crayonCherry
                           : (curVol > 1.05f) ? MiMOLookAndFeel::crayonOchre
                           : MiMOLookAndFeel::crayonCyan;

    g.setColour(volAccent);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(juce::String(volPercent) + "% (" + dbStr + ")",
               static_cast<int>(panel.getRight() - 140), static_cast<int>(panel.getY() + 74), 126, 20, juce::Justification::centredRight);

    // 3a. Slider Track Volume (da 0% a 200%)
    const auto volTrack = getSettingsVolumeTrackBounds();
    g.setColour(isDarkMode ? juce::Colour(0xFF181B20) : juce::Colour(0xFFDED8CE));
    g.fillRoundedRectangle(volTrack, 5.0f);
    g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(0.35f));
    g.drawRoundedRectangle(volTrack, 5.0f, 1.2f);

    // Tacca al centro (100% / Unity Gain)
    const float midX = volTrack.getX() + volTrack.getWidth() * 0.5f;
    g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.55f));
    g.drawVerticalLine(static_cast<int>(midX), volTrack.getY() + 2.0f, volTrack.getBottom() - 2.0f);

    // Riempimento della barra
    const float normVol = juce::jlimit(0.0f, 1.0f, curVol / 2.0f);
    const float fillW = volTrack.getWidth() * normVol;
    if (fillW > 2.0f)
    {
        juce::Rectangle<float> fillRect(volTrack.getX(), volTrack.getY(), fillW, volTrack.getHeight());
        g.setColour(volAccent.withAlpha(0.78f));
        g.fillRoundedRectangle(fillRect, 5.0f);
    }

    // Pomello cursore
    const float thumbX = volTrack.getX() + fillW;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.fillEllipse(thumbX - 6.0f, volTrack.getCentreY() - 7.0f, 12.0f, 14.0f);
    g.setColour(volAccent);
    g.fillEllipse(thumbX - 4.0f, volTrack.getCentreY() - 5.0f, 8.0f, 10.0f);

    // 3b. Pulsanti Rapidi Volume: [ -10% ], [ 100% ], [ +50% Boost ], [ +10% ]
    auto drawVolBtn = [&](juce::Rectangle<float> b, const juce::String& text, juce::Colour tint)
    {
        MiMOLookAndFeel::drawSketchBox(g, b, 860 + static_cast<int>(b.getX()), tint, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(text, b.toNearestInt(), juce::Justification::centred);
    };

    drawVolBtn(getSettingsVolDownBtnBounds(), "-10%", MiMOLookAndFeel::crayonOchre);
    drawVolBtn(getSettingsVol100BtnBounds(), "100%", (volPercent == 100) ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonCyan);
    drawVolBtn(getSettingsVol150BtnBounds(), "+50% Boost", (volPercent == 150) ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::crayonPeach);
    drawVolBtn(getSettingsVolUpBtnBounds(), "+10%", MiMOLookAndFeel::crayonSage);

    // 4. Vu-Meter Livello Uscita Live
    const float peak = audioEngine.getMasterPeak();
    const float meterY = panel.getY() + 166.0f;
    juce::Rectangle<float> meterBg(panel.getX() + 14.0f, meterY, panel.getWidth() - 28.0f, 4.0f);
    g.setColour(isDarkMode ? juce::Colour(0xFF22262D) : juce::Colour(0xFFDAD3C8));
    g.fillRoundedRectangle(meterBg, 2.0f);

    const float peakW = meterBg.getWidth() * juce::jlimit(0.0f, 1.0f, peak);
    if (peakW > 1.0f)
    {
        juce::Rectangle<float> meterFg(meterBg.getX(), meterBg.getY(), peakW, 4.0f);
        g.setColour((peak > 0.90f) ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::crayonSage);
        g.fillRoundedRectangle(meterFg, 2.0f);
    }

    // 5. Dettagli Motore Faust DSP + Hardware
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(9.5f, juce::Font::plain));
    g.drawText("Audio Engine: Faust DSP + ASIO (48 kHz Stereo)\nSoft-Limiter Anti-Clipping Udo Zolzer attivo",
               static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 178),
               static_cast<int>(panel.getWidth() - 28), 30, juce::Justification::centredLeft);
}

void MainComponent::paintNestedModal(juce::Graphics& g, const CanvasNode& node)
{
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.58f));

    const auto modal = getLocalBounds().toFloat().reduced(44.0f, 36.0f);
    MiMOLookAndFeel::drawSketchBox(g, modal, 600, node.crayonColour, 0.16f,
                                   juce::Colours::transparentBlack, 2.5f, false, MiMOLookAndFeel::paperBg);

    // Header Modale
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("ARCHITETTURA INTERNA (NODO NESTED FAUST DSP): " + node.label,
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 16), 720, 26, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.5f, juce::Font::plain));
    g.drawText("Clicca su una manopola interna per esporla/nasconderla ([*] Esposto) sul nodo genitore nel Canvas STUDIO, o trascinala in verticale!",
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 42), 820, 20, juce::Justification::centredLeft);

    // Pulsante Chiudi
    juce::Rectangle<float> closeBtn(modal.getRight() - 210.0f, modal.getY() + 18.0f, 186.0f, 38.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, 601, MiMOLookAndFeel::crayonCherry, 0.78f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("<- Torna al Canvas STUDIO", closeBtn.toNearestInt(), juce::Justification::centred);

    // Nodo Group Input a sinistra
    juce::Rectangle<float> groupIn(modal.getX() + 30.0f, modal.getY() + 160.0f, 190.0f, 140.0f);
    MiMOLookAndFeel::drawSketchBox(g, groupIn, 602, MiMOLookAndFeel::crayonOchre, 0.48f, juce::Colours::transparentBlack, 2.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText("INGRESSO NODO", groupIn.reduced(10, 8).removeFromTop(22).toNearestInt(), juce::Justification::centredLeft);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("- Segnale Audio In\n- [*] Esponi Parametro\n  (collegato ai knob [*])", groupIn.reduced(12, 34).toNearestInt(), juce::Justification::topLeft);

    // Nodo Group Output a destra
    juce::Rectangle<float> groupOut(modal.getRight() - 190.0f, modal.getY() + 180.0f, 155.0f, 100.0f);
    MiMOLookAndFeel::drawSketchBox(g, groupOut, 603, MiMOLookAndFeel::crayonSage, 0.48f, juce::Colours::transparentBlack, 2.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText("USCITA NODO", groupOut.reduced(10, 12).removeFromTop(22).toNearestInt(), juce::Justification::centred);
    g.setFont(juce::Font(11.5f, juce::Font::plain));
    g.drawText("Ritorna alla catena STUDIO", groupOut.reduced(10, 38).toNearestInt(), juce::Justification::centred);

    // Cavi interni e Blocchi Faust DSP
    juce::Point<float> prevAudioOut(groupIn.getRight(), groupIn.getY() + 45.0f);
    const juce::Point<float> exposeOrigin(groupIn.getRight(), groupIn.getY() + 95.0f);

    for (size_t bi = 0; bi < node.innerBlocks.size(); ++bi)
    {
        const auto& b = node.innerBlocks[bi];
        const float bw = std::max(210.0f, static_cast<float>(b.params.size()) * 95.0f + 30.0f);
        const juce::Rectangle<float> bRect(modal.getX() + b.pos.x, modal.getY() + b.pos.y, bw, 195.0f);

        if (!b.isLfo)
        {
            const juce::Point<float> audioIn(bRect.getX(), bRect.getY() + 30.0f);
            MiMOLookAndFeel::drawInkCable(g, prevAudioOut, audioIn, {}, 700 + static_cast<int>(bi), boilTick,
                                          false, MiMOLookAndFeel::fountainInk);
            prevAudioOut = { bRect.getRight(), bRect.getY() + 30.0f };
        }

        MiMOLookAndFeel::drawSketchBox(g, bRect, 610 + static_cast<int>(bi), b.colour, 0.48f, juce::Colours::transparentBlack, 2.0f);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(14.5f, juce::Font::bold));
        g.drawText(b.label + " [Faust DSP]", static_cast<int>(bRect.getX() + 12), static_cast<int>(bRect.getY() + 8),
                   static_cast<int>(bw - 24), 20, juce::Justification::centredLeft);

        // Snippet codice Faust
        juce::Rectangle<float> codeBox(bRect.getX() + 10.0f, bRect.getY() + 32.0f, bw - 20.0f, 24.0f);
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillRoundedRectangle(codeBox, 3.0f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText(b.faustCode, codeBox.reduced(6, 0).toNearestInt(), juce::Justification::centredLeft);

        // Manopole interne del blocco
        const float slotW = (bw - 20.0f) / static_cast<float>(std::max(1, static_cast<int>(b.params.size())));
        for (size_t pi = 0; pi < b.params.size(); ++pi)
        {
            const auto& p = b.params[pi];
            juce::Rectangle<float> kb(bRect.getX() + 10.0f + static_cast<float>(pi) * slotW,
                                      bRect.getY() + 64.0f, slotW, 88.0f);

            if (p.isExposed)
            {
                const juce::Point<float> knobPin(kb.getX() + 8.0f, kb.getY() + 26.0f);
                MiMOLookAndFeel::drawInkCable(g, exposeOrigin, knobPin, {},
                                              750 + static_cast<int>(bi * 10 + pi), boilTick,
                                              true, MiMOLookAndFeel::crayonOchre, false, true);
            }

            MiMOLookAndFeel::drawCrayonKnob(g, kb, p.getNormalized(), b.colour,
                                            p.label, p.getFormattedValue(),
                                            (b.id + p.key).hashCode(), p.isExposed, MiMOLookAndFeel::crayonOchre);

            // Badge cliccabile per esporre/nascondere sul nodo genitore
            juce::Rectangle<float> expToggle(kb.getX() + 6.0f, kb.getBottom() + 2.0f, kb.getWidth() - 12.0f, 20.0f);
            MiMOLookAndFeel::drawSketchBox(g, expToggle, 800 + static_cast<int>(pi),
                                           p.isExposed ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           p.isExposed ? 0.75f : 0.0f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText(p.isExposed ? "[*] Esposto" : "+ Esponi", expToggle.toNearestInt(), juce::Justification::centred);
        }
    }

    MiMOLookAndFeel::drawInkCable(g, prevAudioOut, { groupOut.getX(), groupOut.getY() + 45.0f },
                                  {}, 799, boilTick, false, MiMOLookAndFeel::fountainInk);
}

void MainComponent::paintPolaroidOverlay(juce::Graphics& g)
{
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.45f));

    const auto card = getPolaroidCardBounds();
    MiMOLookAndFeel::drawSketchBox(g, card, 910, juce::Colours::transparentBlack, 0.0f,
                                   MiMOLookAndFeel::crayonOchre, 2.4f, false, MiMOLookAndFeel::polaroidWhite);

    // Finestra fotografica superiore della Polaroid
    juce::Rectangle<float> photoArea(card.getX() + 18.0f, card.getY() + 18.0f, card.getWidth() - 36.0f, 220.0f);
    g.setColour(MiMOLookAndFeel::paperBg);
    g.fillRect(photoArea);
    if (pendingPolaroid.previewImage.isValid())
    {
        g.drawImageWithin(pendingPolaroid.previewImage,
                          static_cast<int>(photoArea.getX()), static_cast<int>(photoArea.getY()),
                          static_cast<int>(photoArea.getWidth()), static_cast<int>(photoArea.getHeight()),
                          juce::RectanglePlacement::stretchToFit);
    }
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.drawRoundedRectangle(photoArea, 4.0f, 2.0f);

    // Badge countdown 3s se non ha ancora digitato
    if (!polaroidHasTyped)
    {
        const int secs = std::max(1, (polaroidCountdownTicks + 5) / 6);
        juce::Rectangle<float> timerBadge(photoArea.getRight() - 185.0f, photoArea.getBottom() - 30.0f, 178.0f, 24.0f);
        MiMOLookAndFeel::drawSketchBox(g, timerBadge, 912, MiMOLookAndFeel::crayonOchre, 0.88f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("Auto-salvataggio in " + juce::String(secs) + "s...", timerBadge.toNearestInt(), juce::Justification::centred);
    }

    // Bordo bianco inferiore scrivibile con font stile pennarello
    juce::Rectangle<float> markerZone(card.getX() + 18.0f, photoArea.getBottom() + 12.0f, card.getWidth() - 36.0f, 54.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText(pendingPolaroid.title + " |", markerZone.removeFromTop(32).toNearestInt(), juce::Justification::centred);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.5f, juce::Font::plain));
    g.drawText(polaroidHasTyped
                   ? "Premi INVIO o clicca fuori per salvare nell'Album"
                   : "Digita sulla tastiera per scrivere a pennarello, o trascina sui 5 Slot sotto!",
               static_cast<int>(card.getX() + 12), static_cast<int>(photoArea.getBottom() + 46),
               static_cast<int>(card.getWidth() - 24), 20, juce::Justification::centred);

    // 5 pulsanti rapidi sulla Polaroid per assegnare agli Slot 1..5
    const float btnW = (card.getWidth() - 36.0f - 4.0f * 6.0f) / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        juce::Rectangle<float> sb(card.getX() + 18.0f + static_cast<float>(i) * (btnW + 6.0f),
                                  card.getBottom() - 44.0f, btnW, 28.0f);
        MiMOLookAndFeel::drawSketchBox(g, sb, 920 + i, MiMOLookAndFeel::crayonOchre, 0.45f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText("-> S" + juce::String(i + 1), sb.toNearestInt(), juce::Justification::centred);
    }
}

void MainComponent::paintLiveAlbumView(juce::Graphics& g)
{
    const juce::Colour slotColours[5] = {
        MiMOLookAndFeel::crayonCherry,
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonLilac
    };

    // =========================================================================
    // 1. HEADER LIVE & TELEMETRIA HARDWARE ROCK PI
    // =========================================================================
    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(19.0f, juce::Font::bold));
        g.drawText("WORKSPACE LIVE  //  PERFORMANCE SUL PALCO", 28, 14, 480, 24, juce::Justification::centredLeft);

        // Badge Rock Pi I2C attiva
        juce::Rectangle<float> rockPill(500.0f, 15.0f, 185.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, rockPill, 930, MiMOLookAndFeel::crayonSage, 0.70f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("ROCK PI I2C SINCRONIZZATA", rockPill.toNearestInt(), juce::Justification::centred);

        // Scorciatoie e Guida rapida
        juce::Rectangle<float> helpPill(static_cast<float>(getWidth() - 480), 15.0f, 456.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, helpPill, 931, MiMOLookAndFeel::paperCard, 0.60f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(10.5f, juce::Font::plain));
        g.drawText("Tasti [1..5] = Suona  -  Hold (1s) = Assegna  -  [Spazio] = Looper  -  Doppio Clic = Quick-Edit",
                   helpPill.toNearestInt(), juce::Justification::centred);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(12.5f, juce::Font::plain));
        g.drawText("5 Grandi Slot Scena pilotabili istantaneamente (<1ms audio spillover) dai tasti fisici della scocca o da tastiera.",
                   28, 38, 800, 18, juce::Justification::centredLeft);
    }

    // =========================================================================
    // 2. L'ALBUM (Area Superiore / Centrale: Griglia delle Polaroid)
    // =========================================================================
    juce::Rectangle<float> albumBox(24.0f, 62.0f, static_cast<float>(getWidth() - 48), static_cast<float>(getHeight() - 322));
    MiMOLookAndFeel::drawSketchBox(g, albumBox, 950, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

    // Barra Superiore Album: Contatore e Filtri Motore (Tutti / sfizz / Faust)
    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText("L'ALBUM DELLE POLAROID (" + juce::String(photoAlbum.size()) + ")",
                   static_cast<int>(albumBox.getX() + 16), static_cast<int>(albumBox.getY() + 8), 240, 20, juce::Justification::centredLeft);

        if (selectedAlbumIndex >= 0 && selectedAlbumIndex < static_cast<int>(photoAlbum.size()))
        {
            juce::Rectangle<float> selBadge(albumBox.getX() + 270.0f, albumBox.getY() + 7.0f, 380.0f, 22.0f);
            MiMOLookAndFeel::drawSketchBox(g, selBadge, 952, MiMOLookAndFeel::crayonOchre, 0.85f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("[*] Polaroid Selezionata: tieni premuto un tasto 1-5 per 1s per assegnarla!",
                       selBadge.toNearestInt(), juce::Justification::centred);
        }

        // Filtri Motore: TUTTI / SFIZZ / FAUST
        const char* filters[3] = { "TUTTI", "SFIZZ (Campioni)", "FAUST (Sintesi DSP)" };
        float fx = albumBox.getRight() - 320.0f;
        for (int fi = 0; fi < 3; ++fi)
        {
            const juce::String fName = (fi == 0 ? "ALL" : fi == 1 ? "sfizz" : "faust");
            const bool isSel = (filterEngine == fName);
            juce::Rectangle<float> fb(fx, albumBox.getY() + 7.0f, (fi == 0 ? 56.0f : fi == 1 ? 116.0f : 134.0f), 22.0f);
            MiMOLookAndFeel::drawSketchBox(g, fb, 953 + fi,
                                           isSel ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::paperCard,
                                           isSel ? 0.90f : 0.40f);
            g.setColour(isSel ? MiMOLookAndFeel::paperBg : MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText(filters[fi], fb.toNearestInt(), juce::Justification::centred);
            fx += fb.getWidth() + 6.0f;
        }
    }

    // Griglia Polaroid
    if (photoAlbum.empty())
    {
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(15.5f, juce::Font::italic));
        g.drawText("L'Album e' vuoto. Torna in Home / Studio e clicca sull'icona della fotocamera per scattare una Polaroid!",
                   albumBox.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        const float cardW = 182.0f;
        const float cardH = 208.0f;
        const float gapX = 14.0f;
        const float gapY = 14.0f;
        float cx = albumBox.getX() + 16.0f;
        float cy = albumBox.getY() + 38.0f;

        for (size_t i = 0; i < photoAlbum.size(); ++i)
        {
            const auto& pol = photoAlbum[i];

            // Filtro Engine
            if (filterEngine == "sfizz" && pol.engineType != "sfizz" && pol.engineType != "hybrid")
                continue;
            if (filterEngine == "faust" && pol.engineType != "faust" && pol.engineType != "hybrid")
                continue;

            juce::Rectangle<float> cRect(cx, cy, cardW, cardH);
            const bool isSelected = (selectedAlbumIndex == static_cast<int>(i));

            // Bordo bianco asimmetrico Polaroid (carta fotografica spessa)
            MiMOLookAndFeel::drawSketchBox(g, cRect, 960 + static_cast<int>(i),
                                           isSelected ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           isSelected ? 0.35f : 0.0f,
                                           isSelected ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           isSelected ? 2.6f : 1.8f,
                                           false, MiMOLookAndFeel::polaroidWhite);

            // Foto interna
            juce::Rectangle<float> imgRect(cx + 9.0f, cy + 9.0f, cardW - 18.0f, 118.0f);
            g.setColour(MiMOLookAndFeel::paperBg);
            g.fillRect(imgRect);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawRect(imgRect, 1.2f);

            // Badge Motore Audio in alto a destra nella foto (sfizz / Faust / Hybrid)
            juce::Colour engineCol = pol.engineType == "sfizz" ? MiMOLookAndFeel::crayonPeach
                                   : pol.engineType == "faust" ? MiMOLookAndFeel::crayonCyan
                                   : MiMOLookAndFeel::crayonOchre;
            juce::Rectangle<float> engBadge(imgRect.getRight() - 48.0f, imgRect.getY() + 5.0f, 44.0f, 16.0f);
            MiMOLookAndFeel::drawSketchBox(g, engBadge, 970 + static_cast<int>(i), engineCol, 0.85f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(pol.engineType.toUpperCase(), engBadge.toNearestInt(), juce::Justification::centred);

            // Data/ora scatto
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText("Scatto " + pol.timestamp, static_cast<int>(imgRect.getX() + 6), static_cast<int>(imgRect.getY() + 5),
                       100, 16, juce::Justification::centredLeft);

            // Badge componenti sonori
            float by = imgRect.getY() + 26.0f;
            for (size_t b = 0; b < std::min(static_cast<size_t>(3), pol.badges.size()); ++b)
            {
                juce::Rectangle<float> br(imgRect.getX() + 6.0f, by, imgRect.getWidth() - 12.0f, 16.0f);
                g.setColour(pol.badges[b].second.withAlpha(0.35f));
                g.fillRoundedRectangle(br, 2.5f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.drawRoundedRectangle(br, 2.5f, 0.9f);
                g.setFont(juce::Font(9.5f, juce::Font::plain));
                g.drawText(pol.badges[b].first, br.reduced(3, 0).toNearestInt(), juce::Justification::centredLeft);
                by += 19.0f;
            }

            // BPM in basso a sinistra nella foto
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(juce::String(pol.bpm) + " BPM", static_cast<int>(imgRect.getX() + 6), static_cast<int>(imgRect.getBottom() - 18),
                       80, 16, juce::Justification::centredLeft);

            // Banda inferiore bianca della Polaroid: Titolo a Pennarello
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(13.5f, juce::Font::bold));
            g.drawText(pol.title, static_cast<int>(cx + 6), static_cast<int>(imgRect.getBottom() + 6),
                       static_cast<int>(cardW - 12), 34, juce::Justification::centred, true);

            // Prompt di azione
            g.setFont(juce::Font(9.5f, isSelected ? juce::Font::bold : juce::Font::plain));
            g.setColour(isSelected ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::inkMuted);
            g.drawText(isSelected ? "[*] PRONTA (Tieni premuto 1-5)" : "Trascina su Slot 1-5",
                       static_cast<int>(cx + 6), static_cast<int>(cRect.getBottom() - 18),
                       static_cast<int>(cardW - 12), 14, juce::Justification::centred);

            cx += cardW + gapX;
            if (cx + cardW > albumBox.getRight() - 16.0f)
            {
                cx = albumBox.getX() + 16.0f;
                cy += cardH + gapY;
            }
        }
    }

    // =========================================================================
    // 3. LA DOCK DELLE 5 SCENE HARDWARE (AREA INFERIORE)
    // =========================================================================
    {
        // Titolo dock
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("DOCK DELLE 5 SCENE HARDWARE (Pulsanti Fisici 1-5 della Rock Pi)",
                   28, static_cast<int>(getHeight() - 272), 520, 20, juce::Justification::centredLeft);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.5f, juce::Font::plain));
        g.drawText("Scena Attiva: S" + juce::String(activeSceneSlot + 1) + " (" + (sceneSlots[activeSceneSlot].isValid ? sceneSlots[activeSceneSlot].title : "Slot Libero") + ")",
                   static_cast<int>(getWidth() - 480), static_cast<int>(getHeight() - 272), 320, 20, juce::Justification::centredRight);

        // I 5 Grandi Slot
        for (int i = 0; i < 5; ++i)
        {
            auto r = getSlotBounds(i);
            const bool isActive = (activeSceneSlot == i);
            const bool isOccupied = sceneSlots[i].isValid;
            const bool isHover = (hoveredSlotIndex == i);

            // Snap elastic spring scale
            if (snappedSlotIndex == i && snapAnimScale > 1.001f)
            {
                const float scaleDiff = (snapAnimScale - 1.0f) * 12.0f;
                r = r.expanded(scaleDiff);
            }
            else if (isHover)
            {
                r = r.expanded(3.5f);
            }

            // Dimming per performance: gli slot non attivi si attenuano per non distrarre
            const float slotAlpha = isActive ? 1.0f : 0.40f;
            g.setOpacity(slotAlpha);

            float fillOpacity = 0.08f;
            if (isActive) fillOpacity = 0.88f;
            else if (isHover) fillOpacity = 0.35f;
            else if (isOccupied) fillOpacity = 0.22f;

            MiMOLookAndFeel::drawSketchBox(
                g, r, 1000 + i,
                slotColours[i], fillOpacity,
                isActive ? slotColours[i] : juce::Colours::transparentBlack,
                isActive ? 2.8f : 1.8f,
                !isOccupied && !isActive,
                MiMOLookAndFeel::paperCard
            );

            // Numero 1-5 in Filigrana Gigante
            g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(isActive ? 0.12f : 0.05f));
            g.setFont(juce::Font(88.0f, juce::Font::bold));
            g.drawText(juce::String(i + 1), r.toNearestInt(), juce::Justification::centred);

            // Header Slot: Badge Pulsante Hardware Rock Pi + LED di stato
            juce::Rectangle<float> pBadge(r.getX() + 8.0f, r.getY() + 8.0f, 22.0f, 22.0f);
            g.setColour(slotColours[i]);
            g.fillEllipse(pBadge);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawEllipse(pBadge, 1.4f);
            g.setFont(juce::Font(11.5f, juce::Font::bold));
            g.drawText(juce::String(i + 1), pBadge.toNearestInt(), juce::Justification::centred);

            g.setFont(juce::Font(11.5f, juce::Font::bold));
            g.drawText("PULSANTE [" + juce::String(i + 1) + "]",
                       static_cast<int>(pBadge.getRight() + 6), static_cast<int>(r.getY() + 9),
                       100, 18, juce::Justification::centredLeft);

            // LED
            juce::Rectangle<float> led(r.getRight() - 20.0f, r.getY() + 11.0f, 12.0f, 12.0f);
            if (isActive)
            {
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.fillEllipse(led);
            }
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawEllipse(led, 1.4f);

            // Corpo Slot
            if (isOccupied)
            {
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(16.0f, juce::Font::bold));
                g.drawText(sceneSlots[i].title,
                           static_cast<int>(r.getX() + 10), static_cast<int>(r.getY() + 36),
                           static_cast<int>(r.getWidth() - 20), 40, juce::Justification::topLeft, true);

                // Badge Motore
                juce::Colour engineCol = sceneSlots[i].engineType == "sfizz" ? MiMOLookAndFeel::crayonPeach
                                       : sceneSlots[i].engineType == "faust" ? MiMOLookAndFeel::crayonCyan
                                       : MiMOLookAndFeel::crayonOchre;
                juce::Rectangle<float> eb(r.getX() + 10.0f, r.getY() + 80.0f, 44.0f, 16.0f);
                MiMOLookAndFeel::drawSketchBox(g, eb, 1200 + i, engineCol, 0.75f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(9.0f, juce::Font::bold));
                g.drawText(sceneSlots[i].engineType.toUpperCase(), eb.toNearestInt(), juce::Justification::centred);

                g.setFont(juce::Font(10.5f, juce::Font::bold));
                g.drawText(juce::String(sceneSlots[i].bpm) + " BPM",
                           static_cast<int>(eb.getRight() + 6), static_cast<int>(r.getY() + 80),
                           70, 16, juce::Justification::centredLeft);

                // Badges componenti
                float by = r.getY() + 102.0f;
                for (size_t b = 0; b < std::min(static_cast<size_t>(2), sceneSlots[i].badges.size()); ++b)
                {
                    juce::Rectangle<float> br(r.getX() + 10.0f, by, r.getWidth() - 20.0f, 17.0f);
                    g.setColour(sceneSlots[i].badges[b].second.withAlpha(0.40f));
                    g.fillRoundedRectangle(br, 2.5f);
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.drawRoundedRectangle(br, 2.5f, 0.9f);
                    g.setFont(juce::Font(9.5f, juce::Font::plain));
                    g.drawText(sceneSlots[i].badges[b].first, br.reduced(3, 0).toNearestInt(), juce::Justification::centredLeft);
                    by += 20.0f;
                }
            }
            else
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                g.drawText("SLOT LIBERO", static_cast<int>(r.getX() + 10), static_cast<int>(r.getY() + 64),
                           static_cast<int>(r.getWidth() - 20), 20, juce::Justification::centred);
                g.setFont(juce::Font(11.0f, juce::Font::plain));
                g.drawText("Trascina Polaroid qui", static_cast<int>(r.getX() + 10), static_cast<int>(r.getY() + 86),
                           static_cast<int>(r.getWidth() - 20), 16, juce::Justification::centred);
            }

            // Footer Slot
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            if (isActive)
            {
                g.drawText("[IN ESECUZIONE]", static_cast<int>(r.getX() + 10), static_cast<int>(r.getBottom() - 24),
                           static_cast<int>(r.getWidth() - 20), 16, juce::Justification::centredLeft);
            }
            else
            {
                g.drawText("Tasto [" + juce::String(i + 1) + "]", static_cast<int>(r.getX() + 10), static_cast<int>(r.getBottom() - 24),
                           70, 16, juce::Justification::centredLeft);
            }

            if (isOccupied)
            {
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.0f, juce::Font::italic));
                g.drawText("Doppio clic: Quick-Edit", static_cast<int>(r.getX() + 10), static_cast<int>(r.getBottom() - 24),
                           static_cast<int>(r.getWidth() - 20), 16, juce::Justification::centredRight);
            }

            // Barra di progresso Assegnazione Rapida (Hold 1s)
            if (holdSlotIndex == i && holdProgress > 0.0f)
            {
                auto bar = r.removeFromBottom(5.0f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.fillRect(bar.withWidth(bar.getWidth() * holdProgress));
            }

            g.setOpacity(1.0f);
        }

        // =====================================================================
        // 4. 1-BUTTON LOOPER MINIMALE (Accanto ai 5 Slot)
        // =====================================================================
        const auto looperBox = getLooperBounds();
        MiMOLookAndFeel::drawSketchBox(g, looperBox, 1300, MiMOLookAndFeel::paperCard, 0.55f,
                                       juce::Colours::transparentBlack, 1.8f);

        const float lcx = looperBox.getCentreX();
        const float lcy = looperBox.getY() + 68.0f;
        const float lrad = 40.0f;

        juce::Colour loopFill = MiMOLookAndFeel::paperCard;
        juce::String loopTitle = "PRONTO";
        if (looperState == LooperState::Recording)
        {
            loopFill = MiMOLookAndFeel::crayonCherry;
            loopTitle = "[REC]";
        }
        else if (looperState == LooperState::Playing)
        {
            loopFill = MiMOLookAndFeel::crayonSage;
            loopTitle = "[PLAY]";
        }
        else if (looperState == LooperState::Overdubbing)
        {
            loopFill = MiMOLookAndFeel::crayonOchre;
            loopTitle = "[+DUB]";
        }

        // Cerchio pastello di riempimento in tempo reale
        g.setColour(loopFill.withAlpha(looperState == LooperState::Idle ? 0.15f : 0.88f));
        g.fillEllipse(lcx - lrad, lcy - lrad, lrad * 2.0f, lrad * 2.0f);

        // Contorno a mano
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.drawEllipse(lcx - lrad, lcy - lrad, lrad * 2.0f, lrad * 2.0f, looperState != LooperState::Idle ? 2.4f : 1.8f);

        // Sweep rotativo del loop in tempo reale (60 FPS)
        if (looperState != LooperState::Idle && looperSweepProgress > 0.01f)
        {
            juce::Path sweep;
            sweep.addCentredArc(lcx, lcy, lrad, lrad, 0.0f, 0.0f, looperSweepProgress * juce::MathConstants<float>::twoPi, true);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.strokePath(sweep, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Simbolo iconico centrale grande e chiaro (NO testo minuscolo)
        if (looperState == LooperState::Recording)
        {
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillEllipse(lcx - 11.0f, lcy - 11.0f, 22.0f, 22.0f);
        }
        else if (looperState == LooperState::Playing)
        {
            juce::Path tri;
            tri.addTriangle(lcx - 9.0f, lcy - 13.0f,
                            lcx + 13.0f, lcy,
                            lcx - 9.0f, lcy + 13.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillPath(tri);
        }
        else if (looperState == LooperState::Overdubbing)
        {
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawLine(lcx - 13.0f, lcy, lcx + 13.0f, lcy, 3.8f);
            g.drawLine(lcx, lcy - 13.0f, lcx, lcy + 13.0f, 3.8f);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(0.6f));
            g.drawEllipse(lcx - 11.0f, lcy - 11.0f, 22.0f, 22.0f, 2.0f);
        }

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(loopTitle,
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 10.0f),
                   static_cast<int>(looperBox.getWidth()), 18, juce::Justification::centred);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        g.drawText("[Spazio] o Clic",
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 28.0f),
                   static_cast<int>(looperBox.getWidth()), 16, juce::Justification::centred);
        g.drawText("Dx: Stop",
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 44.0f),
                   static_cast<int>(looperBox.getWidth()), 16, juce::Justification::centred);
    }
}

void MainComponent::paintCommunityView(juce::Graphics& g)
{
    juce::Rectangle<float> box(48.0f, 86.0f, static_cast<float>(getWidth() - 96), static_cast<float>(getHeight() - 215));
    MiMOLookAndFeel::drawSketchBox(g, box, 990, MiMOLookAndFeel::crayonCyan, 0.18f, juce::Colours::transparentBlack, 2.2f);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("COMMUNITY MARKETPLACE & LIBRERIA PACCHETTI .MIMO",
               static_cast<int>(box.getX() + 24), static_cast<int>(box.getY() + 18), 640, 26, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.5f, juce::Font::plain));
    g.drawText("Pacchetti .mimo validati via manifest.json (motori sfizz campionati e sintesi/effetti Faust DSP).",
               static_cast<int>(box.getX() + 24), static_cast<int>(box.getY() + 44), 760, 20, juce::Justification::centredLeft);

    float y = box.getY() + 80.0f;
    for (size_t i = 0; i < instrumentCatalog.size(); ++i)
    {
        const auto& item = instrumentCatalog[i];
        juce::Rectangle<float> row(box.getX() + 24.0f, y, box.getWidth() - 48.0f, 48.0f);
        MiMOLookAndFeel::drawSketchBox(g, row, 995 + static_cast<int>(i), item.colour, 0.45f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(item.name + "  [" + item.engine + "]  -  " + item.hint,
                   row.reduced(16, 0).toNearestInt(), juce::Justification::centredLeft);
        y += 56.0f;
    }
}

void MainComponent::paintBottomDockAndSlots(juce::Graphics& g)
{
    const juce::Colour slotColours[5] = {
        MiMOLookAndFeel::crayonCherry,
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonLilac
    };

    // 1. I 5 Slot Scena Hardware in basso (compaiono nello Studio solo quando si scatta o si trascina una Polaroid; in Live sono disegnati a tutta altezza nella Dock Scene)
    if (activeTab != ActiveTab::Live)
    {
        const bool showSlots = hasPendingPolaroid || (dragMode == DragMode::DragPolaroid);
        if (showSlots)
        {
            for (int i = 0; i < 5; ++i)
            {
                const auto r = getSlotBounds(i);
                const bool assigned = sceneSlots[i].isValid;
                const bool isHover = (hoveredSlotIndex == i);

                MiMOLookAndFeel::drawSketchBox(
                    g, r, 1000 + i,
                    assigned ? slotColours[i] : juce::Colours::transparentBlack,
                    assigned ? 0.65f : 0.0f,
                    isHover ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                    (activeSceneSlot == i && assigned) ? 2.4f : 1.7f
                );

                g.setColour(slotColours[i]);
                g.fillEllipse(r.getX() + 10.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.drawEllipse(r.getX() + 10.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f, 1.4f);

                g.setFont(juce::Font(12.5f, juce::Font::bold));
                g.drawText(assigned ? ("S" + juce::String(i + 1) + ": " + sceneSlots[i].title)
                                    : ("SLOT SCENA " + juce::String(i + 1)),
                           static_cast<int>(r.getX() + 32), static_cast<int>(r.getY() + 6),
                           static_cast<int>(r.getWidth() - 38), 18, juce::Justification::centredLeft);

                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.5f, juce::Font::plain));
                g.drawText(assigned ? "Assegna qui" : "Trascina Polaroid qui",
                           static_cast<int>(r.getX() + 32), static_cast<int>(r.getY() + 24),
                           static_cast<int>(r.getWidth() - 38), 16, juce::Justification::centredLeft);
            }
        }
    }

    // 2. Dock Principale in Basso al Centro (Home/Studio, Live, Community)
    const float dockW = 450.0f;
    const juce::Rectangle<float> dockRect((static_cast<float>(getWidth()) - dockW) * 0.5f,
                                          static_cast<float>(getHeight() - 52), dockW, 42.0f);
    MiMOLookAndFeel::drawSketchBox(g, dockRect, 1010, juce::Colours::transparentBlack, 0.0f,
                                   juce::Colours::transparentBlack, 2.1f);

    const char* tabLabels[3] = { "Home / Studio", "Live (Album)", "Community" };
    const juce::Colour tabCols[3] = {
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonCherry,
        MiMOLookAndFeel::crayonCyan
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto tb = getDockTabBounds(i);
        const bool isActive = (static_cast<int>(activeTab) == i);
        if (isActive)
            MiMOLookAndFeel::drawSketchBox(g, tb, 1020 + i, tabCols[i], 0.72f, juce::Colours::transparentBlack, 1.6f);

        juce::Rectangle<float> iconBox(tb.getX() + 8.0f, tb.getY() + 5.0f, 24.0f, 24.0f);
        if (i == 0)
            MiMOLookAndFeel::drawStudioIcon(g, iconBox, tabCols[i]);
        else if (i == 1)
            MiMOLookAndFeel::drawLiveIcon(g, iconBox, tabCols[i]);
        else
            MiMOLookAndFeel::drawCommunityIcon(g, iconBox, tabCols[i]);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, isActive ? juce::Font::bold : juce::Font::plain));
        g.drawText(tabLabels[i], static_cast<int>(tb.getX() + 34), static_cast<int>(tb.getY()),
                   static_cast<int>(tb.getWidth() - 38), static_cast<int>(tb.getHeight()),
                   juce::Justification::centredLeft);
    }
}

void MainComponent::resized()
{
}

// =============================================================================
// Gestione Interazioni Mouse & Tastiera
// =============================================================================
void MainComponent::mouseMove(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    currentMousePos = pt;

    if (activeTab == ActiveTab::Studio && expandedNodeId.isEmpty() && !hasPendingPolaroid)
    {
        const auto worldPt = screenToWorld(pt);
        bool foundPin = false;

        const PinHit* bestHoverPin = nullptr;
        float bestHoverDist = 22.0f;
        for (const auto& pin : collectAllPins())
        {
            const float dist = pin.worldPos.getDistanceFrom(worldPt);
            if (dist <= bestHoverDist)
            {
                bestHoverDist = dist;
                bestHoverPin = &pin;
            }
        }

        if (bestHoverPin != nullptr)
        {
            foundPin = true;
            if (hoveredPinNodeId != bestHoverPin->nodeId || hoveredPinId != bestHoverPin->pinId)
            {
                hoveredPinNodeId = bestHoverPin->nodeId;
                hoveredPinId = bestHoverPin->pinId;
                hoveredPinPos = bestHoverPin->worldPos;
                hasHoveredPin = true;
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                repaint();
            }
        }

        if (hasHoveredPin && !foundPin)
        {
            hasHoveredPin = false;
            hoveredPinNodeId.clear();
            hoveredPinId.clear();
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }

        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto pt = e.position;
    currentMousePos = pt;
    dragMode = DragMode::None;

    // 0. Se la finestra modale MiMO Eye AI Controller e' aperta
    if (eyeDrawerOpen)
    {
        const auto mb = getEyeModalBounds();
        if (!mb.contains(pt) || getEyeModalCloseBounds().contains(pt))
        {
            eyeDrawerOpen = false;
            repaint();
            return;
        }

        for (int t = 0; t < 2; ++t)
        {
            if (getEyeModalTabBounds(t).contains(pt))
            {
                eyeDrawerTab = t;
                repaint();
                return;
            }
        }

        // Controlli Impostazioni Dettaglio Mano sul PC (attivi in entrambi i Tab!)
        for (int hf = 0; hf < 3; ++hf)
        {
            if (getEyeModalHandFilterBounds(hf).contains(pt))
            {
                eyeHandFilter = hf;
                syncEyeNodeOutputsWithDetailSettings();
                repaint();
                return;
            }
        }

        for (int pd = 0; pd < 3; ++pd)
        {
            if (getEyeModalPhalanxBounds(pd).contains(pt))
            {
                eyePhalanxDetail = pd;
                syncEyeNodeOutputsWithDetailSettings();
                repaint();
                return;
            }
        }

        for (int gIdx = 0; gIdx < 4; ++gIdx)
        {
            if (getEyeModalGestureBounds(gIdx).contains(pt))
            {
                if (gIdx == 0) eyeGesturePos3D = !eyeGesturePos3D;
                else if (gIdx == 1) eyeGestureFistTilt = !eyeGestureFistTilt;
                else if (gIdx == 2) eyeGestureFingerFlex = !eyeGestureFingerFlex;
                else if (gIdx == 3) eyeGesturePinchTrig = !eyeGesturePinchTrig;

                if (gIdx == 1 || gIdx == 2 || gIdx == 3)
                {
                    if (eyePhalanxDetail == 0)
                        eyePhalanxDetail = 2; // Sblocca automaticamente le falangi se si attiva una gesture delle dita
                }

                syncEyeNodeOutputsWithDetailSettings();
                repaint();
                return;
            }
        }

        for (int p = 0; p < 4; ++p)
        {
            if (getEyeModalPinchTestBounds(p).contains(pt))
            {
                if (eyeServer != nullptr)
                    eyeServer->injectTestPinch(p, 0.9f);
                repaint();
                return;
            }
        }

        if (eyeDrawerTab == 0)
        {
            // Tab 0: Sorgenti Video (PC & Smartphone)
            const auto items = getVideoSourceItems();
            const int maxToShow = std::min(5, static_cast<int>(items.size()));
            for (int i = 0; i < maxToShow; ++i)
            {
                if (getEyeModalCameraOptionBounds(i).contains(pt))
                {
                    const auto& it = items[i];
                    selectVideoSource(it.isSmartphone, it.pcDeviceIdx, it.label);
                    return;
                }
            }

            if (getEyeModalViewToggleBounds().contains(pt))
            {
                eyeShowVideo = !eyeShowVideo;
                repaint();
                return;
            }
        }
        else
        {
            // Tab 1: Connessione Smartphone (QR Code)
            if (getEyeModalUrlButtonBounds().contains(pt))
            {
                if (eyeServer != nullptr)
                    juce::URL(eyeServer->getEyeUrl()).launchInDefaultBrowser();
                return;
            }

            if (getEyeModalActivatePhoneButtonBounds().contains(pt))
            {
                selectVideoSource(true);
                eyeDrawerTab = 0; // Passa direttamente al monitor per vedere la fotocamera
                return;
            }
        }

        return; // Blocca tutti i click verso il canvas sottostante quando la modale è aperta
    }

    // 1. Se c'è una Polaroid appena scattata in sovrimpressione
    if (hasPendingPolaroid)
    {
        const auto card = getPolaroidCardBounds();
        if (!card.contains(pt))
        {
            commitPolaroidToAlbum(pendingPolaroid);
            return;
        }

        // Controlla se ha cliccato uno dei 5 pulsanti rapidi "-> S1..S5"
        const float btnW = (card.getWidth() - 36.0f - 4.0f * 6.0f) / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            juce::Rectangle<float> sb(card.getX() + 18.0f + static_cast<float>(i) * (btnW + 6.0f),
                                      card.getBottom() - 44.0f, btnW, 28.0f);
            if (sb.contains(pt))
            {
                assignPolaroidToSlot(i, pendingPolaroid);
                return;
            }
        }

        // Inizia il trascinamento della Polaroid verso i 5 slot
        polaroidHasTyped = true;
        draggedPolaroid = pendingPolaroid;
        dragMode = DragMode::DragPolaroid;
        return;
    }

    // 2. Se l'editor Fullscreen di un Nodo Nested è aperto
    if (expandedNodeId.isNotEmpty())
    {
        const auto modal = getLocalBounds().toFloat().reduced(44.0f, 36.0f);
        juce::Rectangle<float> closeBtn(modal.getRight() - 210.0f, modal.getY() + 18.0f, 186.0f, 38.0f);
        if (closeBtn.contains(pt) || !modal.contains(pt))
        {
            expandedNodeId.clear();
            repaint();
            return;
        }

        for (auto& n : nodes)
        {
            if (n.id != expandedNodeId)
                continue;

            for (auto& b : n.innerBlocks)
            {
                const float bw = std::max(210.0f, static_cast<float>(b.params.size()) * 95.0f + 30.0f);
                const juce::Rectangle<float> bRect(modal.getX() + b.pos.x, modal.getY() + b.pos.y, bw, 195.0f);
                const float slotW = (bw - 20.0f) / static_cast<float>(std::max(1, static_cast<int>(b.params.size())));

                for (size_t pi = 0; pi < b.params.size(); ++pi)
                {
                    auto& p = b.params[pi];
                    juce::Rectangle<float> kb(bRect.getX() + 10.0f + static_cast<float>(pi) * slotW,
                                              bRect.getY() + 64.0f, slotW, 88.0f);
                    juce::Rectangle<float> expToggle(kb.getX() + 6.0f, kb.getBottom() + 2.0f, kb.getWidth() - 12.0f, 20.0f);

                    if (expToggle.contains(pt))
                    {
                        p.isExposed = !p.isExposed;
                        repaint();
                        return;
                    }
                    if (kb.contains(pt))
                    {
                        dragMode = DragMode::TurnInnerKnob;
                        draggedNodeId = n.id;
                        draggedBlockId = b.id;
                        draggedParamKey = p.key;
                        dragStartMouse = pt;
                        dragStartKnobNorm = p.getNormalized();
                        return;
                    }
                }

                if (bRect.contains(pt))
                {
                    dragMode = DragMode::MoveInnerBlock;
                    draggedNodeId = n.id;
                    draggedBlockId = b.id;
                    dragStartMouse = pt;
                    dragStartOffset = b.pos;
                    return;
                }
            }
        }
        return;
    }

    // 3. Controllo Click sul Dock di Navigazione Inferiore
    for (int i = 0; i < 3; ++i)
    {
        if (getDockTabBounds(i).contains(pt))
        {
            if (editingSlotIndex >= 0 && i == 1)
            {
                sceneSlots[editingSlotIndex].snapshot = { nodes, cables };
                editingSlotIndex = -1;
            }
            activeTab = static_cast<ActiveTab>(i);
            repaint();
            return;
        }
    }

    // 4. Controllo Click sui 5 Slot Scena in basso (attivi solo quando compare la Polaroid)
    const bool showSlots = hasPendingPolaroid || (dragMode == DragMode::DragPolaroid);
    if (showSlots)
    {
        for (int i = 0; i < 5; ++i)
        {
            if (getSlotBounds(i).contains(pt))
            {
                activeSceneSlot = i;
                if (hasPendingPolaroid)
                {
                    assignPolaroidToSlot(i, pendingPolaroid);
                    hasPendingPolaroid = false;
                }
                else if (sceneSlots[i].isValid)
                {
                    nodes = sceneSlots[i].snapshot.nodes;
                    cables = sceneSlots[i].snapshot.cables;
                }
                repaint();
                return;
            }
        }
    }

    // Eventuale pulsante Quick-Edit "✓ TORNA AL LIVE"
    if (editingSlotIndex >= 0)
    {
        juce::Rectangle<float> saveBtn(static_cast<float>(getWidth()) * 0.5f - 195.0f, 16.0f, 390.0f, 44.0f);
        if (saveBtn.contains(pt))
        {
            sceneSlots[editingSlotIndex].snapshot = { nodes, cables };
            editingSlotIndex = -1;
            activeTab = ActiveTab::Live;
            repaint();
            return;
        }
    }

    // 5. Interazioni specifiche della Vista LIVE (Album + 5 Slot + 1-Button Looper)
    if (activeTab == ActiveTab::Live)
    {
        // A. Clic sul 1-Button Looper
        if (getLooperBounds().contains(pt))
        {
            if (e.mods.isRightButtonDown())
                clearLooper();
            else
                toggleLooper();
            return;
        }

        // B. Clic su uno dei 5 Slot Scena
        for (int i = 0; i < 5; ++i)
        {
            if (getSlotBounds(i).contains(pt))
            {
                if (selectedAlbumIndex >= 0 && selectedAlbumIndex < static_cast<int>(photoAlbum.size()))
                {
                    assignPolaroidToSlot(i, photoAlbum[static_cast<size_t>(selectedAlbumIndex)]);
                    selectedAlbumIndex = -1;
                }
                else
                {
                    triggerSceneSlot(i);
                    holdSlotIndex = i;
                    holdProgress = 0.0f;
                }
                return;
            }
        }

        // C. Filtri Motore (TUTTI / SFIZZ / FAUST)
        juce::Rectangle<float> albumBox(24.0f, 62.0f, static_cast<float>(getWidth() - 48), static_cast<float>(getHeight() - 322));
        float fx = albumBox.getRight() - 320.0f;
        for (int fi = 0; fi < 3; ++fi)
        {
            const juce::String fName = (fi == 0 ? "ALL" : fi == 1 ? "sfizz" : "faust");
            juce::Rectangle<float> fb(fx, albumBox.getY() + 7.0f, (fi == 0 ? 56.0f : fi == 1 ? 116.0f : 134.0f), 22.0f);
            if (fb.contains(pt))
            {
                filterEngine = fName;
                selectedAlbumIndex = -1;
                repaint();
                return;
            }
            fx += fb.getWidth() + 6.0f;
        }

        // D. Selezione / Drag Polaroid nell'Album
        const float cardW = 182.0f;
        const float cardH = 208.0f;
        const float gapX = 14.0f;
        const float gapY = 14.0f;
        float cx = albumBox.getX() + 16.0f;
        float cy = albumBox.getY() + 38.0f;

        for (size_t i = 0; i < photoAlbum.size(); ++i)
        {
            const auto& pol = photoAlbum[i];
            if (filterEngine == "sfizz" && pol.engineType != "sfizz" && pol.engineType != "hybrid") continue;
            if (filterEngine == "faust" && pol.engineType != "faust" && pol.engineType != "hybrid") continue;

            juce::Rectangle<float> cRect(cx, cy, cardW, cardH);
            if (cRect.contains(pt))
            {
                selectedAlbumIndex = (selectedAlbumIndex == static_cast<int>(i) ? -1 : static_cast<int>(i));
                draggedPolaroid = pol;
                dragMode = DragMode::DragPolaroid;
                repaint();
                return;
            }

            cx += cardW + gapX;
            if (cx + cardW > albumBox.getRight() - 16.0f)
            {
                cx = albumBox.getX() + 16.0f;
                cy += cardH + gapY;
            }
        }
        return;
    }

    if (activeTab != ActiveTab::Studio)
        return;

    // 6. Controllo Sidebar Destra (Toggle Quadratino, Fotocamera, Ingranaggio, Strumenti, Effetti)
    if (getSidebarToggleBounds().contains(pt))
    {
        sidebarOpen = !sidebarOpen;
        if (!sidebarOpen)
            settingsOpen = false;
        repaint();
        return;
    }

    if (sidebarOpen)
    {
        // 6a. Se il pannello impostazioni e' aperto
        if (settingsOpen)
        {
            const auto panel = getSettingsPanelBounds();
            if (panel.contains(pt))
            {
                juce::Rectangle<float> closeBtn(panel.getRight() - 28.0f, panel.getY() + 8.0f, 20.0f, 20.0f);
                if (closeBtn.contains(pt))
                {
                    settingsOpen = false;
                    saveSettings();
                    repaint();
                    return;
                }

                juce::Rectangle<float> themeBtn(panel.getX() + 14.0f, panel.getY() + 34.0f, panel.getWidth() - 28.0f, 32.0f);
                if (themeBtn.contains(pt))
                {
                    toggleDarkMode();
                    saveSettings();
                    return;
                }

                // Clic o avvio trascinamento sulla barra volume
                const auto volTrack = getSettingsVolumeTrackBounds();
                if (volTrack.expanded(2.0f, 6.0f).contains(pt))
                {
                    dragMode = DragMode::DragSettingsVolume;
                    const float norm = juce::jlimit(0.0f, 1.0f, (pt.x - volTrack.getX()) / volTrack.getWidth());
                    setAppMasterVolume(norm * 2.0f);
                    return;
                }

                // Pulsanti Rapidi Preset Volume
                if (getSettingsVolDownBtnBounds().contains(pt))
                {
                    setAppMasterVolume(appMasterVolume.load() - 0.10f);
                    saveSettings();
                    return;
                }
                if (getSettingsVol100BtnBounds().contains(pt))
                {
                    setAppMasterVolume(1.0f);
                    saveSettings();
                    return;
                }
                if (getSettingsVol150BtnBounds().contains(pt))
                {
                    setAppMasterVolume(1.5f);
                    saveSettings();
                    return;
                }
                if (getSettingsVolUpBtnBounds().contains(pt))
                {
                    setAppMasterVolume(appMasterVolume.load() + 0.10f);
                    saveSettings();
                    return;
                }

                return;
            }
            else if (!getSettingsButtonBounds().contains(pt))
            {
                settingsOpen = false;
                saveSettings();
                repaint();
            }
        }

        // 6b. Pulsante Ingranaggio Impostazioni
        if (getSettingsButtonBounds().contains(pt))
        {
            settingsOpen = !settingsOpen;
            repaint();
            return;
        }

        // 6c. Pulsante Fotocamera Polaroid (Scatto rapido)
        if (getCameraButtonBounds().contains(pt))
        {
            triggerSnapPolaroid();
            return;
        }

        // 6d-0. Zona MODULI INTEGRATI (Click sull'header o sui controlli)
        if (getIntegratedHeaderBounds().contains(pt))
        {
            integratedSectionOpen = !integratedSectionOpen;
            sidebarScrollY = juce::jlimit(0.0f, getSidebarMaxScroll(), sidebarScrollY);
            repaint();
            return;
        }

        if (integratedSectionOpen && getIntegratedBodyBounds().contains(pt))
        {
            // Bottone Metti / Rimuovi Batteria Hardware dal Canvas
            if (getDrumSwitchBounds().contains(pt))
            {
                toggleDrumInCanvas();
                return;
            }

            // Bottone Metti / Rimuovi Microfono dal Canvas
            if (getMicSwitchBounds().contains(pt))
            {
                toggleMicInCanvas();
                return;
            }

            // Bottone Metti / Rimuovi Webcam dal Canvas
            if (getCameraSwitchBounds().contains(pt))
            {
                toggleCameraInCanvas();
                return;
            }

            // Bottone Settaggi Fotocamera / QR Code MiMO Eye
            if (getCameraSettingsButtonBounds().contains(pt))
            {
                eyeDrawerOpen = !eyeDrawerOpen;
                eyeDrawerTab = 0;
                if (eyeDrawerOpen && !useSmartphoneEye)
                {
                    isCameraRecording = true;
                    if (cameraWorker != nullptr)
                    {
                        cameraWorker->setWanted(true);
                        cameraWorker->requestReopen();
                    }
                }
                repaint();
                return;
            }

            // Settaggi Avanzati Fotocamera
            if (cameraSettingsOpen)
            {
                for (int m = 0; m < 3; ++m)
                {
                    if (getCameraModePillBounds(m).contains(pt))
                    {
                        cameraTrackingMode = static_cast<CameraTrackingMode>(m);
                        repaint();
                        return;
                    }
                }

                if (getCameraMirrorCheckboxBounds().contains(pt))
                {
                    cameraMirror = !cameraMirror;
                    repaint();
                    return;
                }
            }

            const auto integBody = getIntegratedBodyBounds();
            const float cardX = integBody.getX() + 8.0f;
            const float cardW = integBody.getWidth() - 16.0f;

            // Clic altrove nella card della Batteria Hardware -> Trascina Modulo Batteria nel Canvas!
            const juce::Rectangle<float> drumCard(cardX, integBody.getY() + 6.0f, cardW, 44.0f);
            if (drumCard.contains(pt))
            {
                draggedCatalogItem = { "hw-drum", "Hardware P1", "Modulo Batteria", "Sensori Piezo", "Trigger piezo I2C", MiMOLookAndFeel::crayonSage, false, false, {} };
                dragMode = DragMode::DragSidebarItem;
                repaint();
                return;
            }

            // Clic altrove nella card del Microfono -> Trascina Microfono nel Canvas!
            const juce::Rectangle<float> micCard(cardX, integBody.getY() + 54.0f, cardW, 44.0f);
            if (micCard.contains(pt))
            {
                draggedCatalogItem = { "integrated-mic", "Integrato", "Microfono PC (Batteria)", "Laptop", "Trigger audio da mic", MiMOLookAndFeel::crayonOchre, false, true, {} };
                dragMode = DragMode::DragSidebarItem;
                repaint();
                return;
            }

            // Clic altrove nella card della Webcam -> Trascina Webcam nel Canvas!
            const juce::Rectangle<float> camCard(cardX, integBody.getY() + 102.0f, cardW, cameraSettingsOpen ? 186.0f : 44.0f);
            if (camCard.contains(pt))
            {
                draggedCatalogItem = { "integrated-webcam", "Integrato", "Webcam PC (Occhio)", "Laptop", "Sensore ottico webcam", MiMOLookAndFeel::crayonCyan, false, true, {} };
                dragMode = DragMode::DragSidebarItem;
                repaint();
                return;
            }

            return; // Cliccato dentro il box moduli integrati (non propagare al canvas)
        }

        // 6d. Zona Strumenti (Click sull'header o sulla freccia apre/chiude)
        if (getInstrumentsHeaderBounds().contains(pt))
        {
            instrumentsSectionOpen = !instrumentsSectionOpen;
            repaint();
            return;
        }

        if (instrumentsSectionOpen && getInstrumentsBodyBounds().contains(pt))
        {
            const auto instBody = getInstrumentsBodyBounds();
            const float x = instBody.getX() + 8.0f;
            const float w = instBody.getWidth() - 16.0f;
            for (size_t i = 0; i < instrumentCatalog.size(); ++i)
            {
                juce::Rectangle<float> row(x, instBody.getY() + 6.0f + static_cast<float>(i) * 40.0f, w, 35.0f);
                if (row.contains(pt))
                {
                    draggedCatalogItem = instrumentCatalog[i];
                    dragMode = DragMode::DragSidebarItem;
                    return;
                }
            }
            return;
        }

        // 6c. Zona Effetti Nested
        if (getEffectsHeaderBounds().contains(pt))
        {
            effectsSectionOpen = !effectsSectionOpen;
            repaint();
            return;
        }

        if (effectsSectionOpen && getEffectsBodyBounds().contains(pt))
        {
            const auto fxBody = getEffectsBodyBounds();
            const float x = fxBody.getX() + 8.0f;
            const float w = fxBody.getWidth() - 16.0f;
            for (size_t i = 0; i < effectCatalog.size(); ++i)
            {
                juce::Rectangle<float> row(x, fxBody.getY() + 6.0f + static_cast<float>(i) * 42.0f, w, 37.0f);
                if (row.contains(pt))
                {
                    draggedCatalogItem = effectCatalog[i];
                    dragMode = DragMode::DragSidebarItem;
                    return;
                }
            }
            return;
        }

        // 6d. Zona Utility & Controllo
        if (getUtilitiesHeaderBounds().contains(pt))
        {
            utilitiesSectionOpen = !utilitiesSectionOpen;
            repaint();
            return;
        }

        if (utilitiesSectionOpen && getUtilitiesBodyBounds().contains(pt))
        {
            const auto utBody = getUtilitiesBodyBounds();
            const float x = utBody.getX() + 8.0f;
            const float w = utBody.getWidth() - 16.0f;
            for (size_t i = 0; i < utilityCatalog.size(); ++i)
            {
                juce::Rectangle<float> row(x, utBody.getY() + 6.0f + static_cast<float>(i) * 40.0f, w, 35.0f);
                if (row.contains(pt))
                {
                    draggedCatalogItem = utilityCatalog[i];
                    dragMode = DragMode::DragSidebarItem;
                    return;
                }
            }
            return;
        }

        // Se cliccato nello spazio vuoto della sidebar, non propagare al canvas sottostante
        if (pt.x >= static_cast<float>(getWidth() - 308))
            return;
    }

    // 7. Interazioni nel Canvas Infinito STUDIO
    const auto worldPt = screenToWorld(pt);
    panVelocity = { 0.0f, 0.0f };

    // Tasto centrale del mouse (Middle Button) o Barra Spaziatrice -> Pan immediato ovunque!
    if (e.mods.isMiddleButtonDown() || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
    {
        selectedNodeId.clear();
    syncAudioRoutes();
        dragMode = DragMode::PanCanvas;
        dragStartMouse = pt;
        dragStartOffset = canvasOffset;
        repaint();
        return;
    }

    // 7-dropdown. Gestione click sulle tendine aperte di selezione Webcam / Microfono nel Canvas
    if (camDropdownOpen || micDropdownOpen)
    {
        for (const auto& n : nodes)
        {
            if (n.type != NodeType::HardwareModule)
                continue;

            const bool isMicNode = (n.isIntegratedMic || n.id == "hw-mic");
            const bool isCamNode = (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye");
            if (isMicNode && !micDropdownOpen)
                continue;
            if (isCamNode && !camDropdownOpen)
                continue;
            if (!isMicNode && !isCamNode)
                continue;

            const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);
            const auto devRow = getIntegratedDeviceRowBounds(box);
            const auto options = isMicNode ? getMicDropdownOptions() : getCameraDropdownOptions();
            const float itemH = 22.0f;
            const float menuH = static_cast<float>(options.size()) * itemH + 6.0f;
            const juce::Rectangle<float> menuBox(devRow.getX(), devRow.getBottom() + 2.0f, devRow.getWidth(), menuH);

            if (menuBox.contains(worldPt))
            {
                const int clickedIdx = juce::jlimit(0, options.size() - 1,
                    static_cast<int>((worldPt.y - (menuBox.getY() + 3.0f)) / itemH));

                if (isMicNode)
                {
                    micDeviceIndex.store(clickedIdx);
                    {
                        const juce::ScopedLock sl(micInfoLock);
                        if (clickedIdx < micDeviceList.size())
                            micDeviceName = micDeviceList[clickedIdx];
                    }
                    if (micWorker != nullptr && isMicRecording)
                        micWorker->requestReopen();
                    micDropdownOpen = false;
                }
                else
                {
                    const auto items = getVideoSourceItems();
                    if (clickedIdx >= 0 && clickedIdx < items.size())
                    {
                        const auto& it = items[clickedIdx];
                        selectVideoSource(it.isSmartphone, it.pcDeviceIdx, it.label);
                        if (it.isSmartphone)
                        {
                            eyeDrawerOpen = true;
                            eyeDrawerTab = 1; // Tab QR Code per verificare la connessione o attivarla
                        }
                    }
                    camDropdownOpen = false;
                }
                repaint();
                return;
            }
        }

        // Click fuori dalla tendina: se non e' sul pulsante devRow stesso, chiudila
        bool clickedOnDevRow = false;
        for (const auto& n : nodes)
        {
            if (n.type == NodeType::HardwareModule && (n.isIntegratedMic || n.isIntegratedCamera || n.id == "hw-mic" || n.id == "hw-webcam" || n.id == "hw-eye"))
            {
                const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);
                if (getIntegratedDeviceRowBounds(box).contains(worldPt))
                    clickedOnDevRow = true;
            }
        }
        if (!clickedOnDevRow)
        {
            camDropdownOpen = false;
            micDropdownOpen = false;
            repaint();
        }
    }

    // 7-pre0. Click sul pulsante [QR], [VID], [X] di rimozione, tendina dispositivo o sugli slider dei moduli nel Canvas
    for (const auto& n : nodes)
    {
        if (n.type == NodeType::HardwareModule)
        {
            const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);

            // Click sul pulsante [QR] configurazione / QR Code di MiMO Eye (anche su hw-eye standard)
            if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
            {
                const auto cfgBtn = getEyeCardConfigButtonBounds(box);
                if (cfgBtn.contains(worldPt))
                {
                    eyeDrawerOpen = true;
                    eyeDrawerTab = 0;
                    repaint();
                    return;
                }

                // Click sul pulsante rapido [VID] / [SKEL]
                const auto vidBtn = getEyeCardViewToggleButtonBounds(box);
                if (vidBtn.contains(worldPt))
                {
                    eyeShowVideo = !eyeShowVideo;
                    repaint();
                    return;
                }
            }

            if (n.type == NodeType::HardwareModule)
            {
                const juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 7.0f, 18.0f, 18.0f);
                if (closeBtn.contains(worldPt))
                {
                    if (n.id == "hw-drum")
                        toggleDrumInCanvas();
                    else if (n.isIntegratedMic || n.id == "hw-mic")
                        toggleMicInCanvas();
                    else if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
                        toggleCameraInCanvas();
                    return;
                }

                // Click sulla tendina selettore dispositivo in fondo al nodo integrato
                const auto devRow = getIntegratedDeviceRowBounds(box);
                if (devRow.contains(worldPt))
                {
                    if (n.isIntegratedMic || n.id == "hw-mic")
                    {
                        micDropdownOpen = !micDropdownOpen;
                        camDropdownOpen = false;
                    }
                    else if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
                    {
                        camDropdownOpen = !camDropdownOpen;
                        micDropdownOpen = false;
                    }
                    repaint();
                    return;
                }

                if (n.isIntegratedMic || n.id == "hw-mic")
                {
                    // Click sulla barra VU Meter -> spara un colpo di test immediato sul sintetizzatore!
                    juce::Rectangle<float> vuBox(n.pos.x + 12.0f, n.pos.y + 48.0f, n.size.x - 24.0f, 20.0f);
                    if (vuBox.contains(worldPt))
                    {
                        synthHitVelocity.store(0.85f);
                        synthTriggerPending.store(true);
                        realMicVelocity.store(0.85f);
                        realMicTriggered.store(true);
                        repaint();
                        return;
                    }

                    juce::Rectangle<float> slTrack(n.pos.x + 12.0f, n.pos.y + 74.0f, n.size.x - 24.0f, 18.0f);
                    if (slTrack.expanded(4.0f, 6.0f).contains(worldPt))
                    {
                        dragMode = DragMode::DragCanvasMicSensitivity;
                        draggedNodeId = n.id;
                        micSensitivity = juce::jlimit(0.02f, 1.0f, (worldPt.x - slTrack.getX()) / slTrack.getWidth());
                        repaint();
                        return;
                    }
                }
            }
        }
    }

    // 7-pre. Click sul pulsante [X] di disconnessione sulle righe di modulazione dei moduli hardware
    for (const auto& n : nodes)
    {
        if (n.type == NodeType::HardwareModule)
        {
            float my = (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye") ? n.pos.y + 172.0f : n.pos.y + 106.0f;
            for (const auto& m : n.modOutputs)
            {
                juce::Rectangle<float> row(n.pos.x + 12.0f, my, n.size.x - 22.0f, 24.0f);
                juce::Rectangle<float> delBtn(row.getRight() - 22.0f, row.getY() + 3.0f, 18.0f, 18.0f);
                if (delBtn.contains(worldPt))
                {
                    const juce::String myPin = "mod:" + m.key;
                    cables.erase(std::remove_if(cables.begin(), cables.end(),
                        [&](const CanvasCable& c) { return c.sourceNodeId == n.id && c.sourcePinId == myPin; }),
                        cables.end());
                    repaint();
                    return;
                }
                my += 28.0f;
            }
        }
    }

    // 7a. Click su un Pin per collegare, staccare o selezionare un Cavo
    const PinHit* bestClickedPin = nullptr;
    float bestPinDist = 15.0f;
    const auto allPins = collectAllPins();
    for (const auto& pin : allPins)
    {
        const float d = pin.worldPos.getDistanceFrom(worldPt);
        if (d <= bestPinDist)
        {
            bestPinDist = d;
            bestClickedPin = &pin;
        }
    }

    if (bestClickedPin != nullptr)
    {
        const auto& pin = *bestClickedPin;
        // Click destro sul pin: scollega e cancella subito tutti i cavi attaccati
        if (e.mods.isRightButtonDown())
        {
            cables.erase(std::remove_if(cables.begin(), cables.end(),
                [&](const CanvasCable& c) {
                    return (c.sourceNodeId == pin.nodeId && c.sourcePinId == pin.pinId) ||
                           (c.targetNodeId == pin.nodeId && c.targetPinId == pin.pinId);
                }), cables.end());
            selectedPinNodeId.clear();
            selectedPinId.clear();
            syncAudioRoutes();
            repaint();
            return;
        }

        // Seleziona questo pin per eventuale cancellazione rapida con [CANC]
        selectedPinNodeId = pin.nodeId;
        selectedPinId = pin.pinId;
        selectedCableId.clear();

        // Controlla se questo pin ha già un cavo collegato:
        // Regola: gli OUTPUT possono avere cavi multipli (fan-out illimitato) e creano SEMPRE un nuovo cavo.
        // Solo dagli INPUT dei moduli stacchiamo il cavo preesistente per poterlo riposizionare o cancellare nel vuoto!
        int foundCableIdx = -1;
        if (!pin.isOutput)
        {
            for (int ci = static_cast<int>(cables.size()) - 1; ci >= 0; --ci)
            {
                const auto& c = cables[static_cast<size_t>(ci)];
                if (c.targetNodeId == pin.nodeId && c.targetPinId == pin.pinId)
                {
                    foundCableIdx = ci;
                    break;
                }
            }
        }

        if (foundCableIdx >= 0)
        {
            // Stacca il cavo dall'input e permetti di trascinarlo per ricollegarlo altrove o cancellarlo nel vuoto
            unhookedOriginalCable = cables[static_cast<size_t>(foundCableIdx)];
            isUnhookingExistingCable = true;
            unhookDragStartWorld = pin.worldPos;

            cableSourceNodeId = unhookedOriginalCable.sourceNodeId;
            cableSourcePinId = unhookedOriginalCable.sourcePinId;
            cableSourceIsOutput = true;

            cableSourceIsMod = unhookedOriginalCable.isModulation;
            cableSourceIsTrigger = unhookedOriginalCable.isTrigger || (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond);
            cableSourceShape = pin.shapeType;
            cableSourceColour = unhookedOriginalCable.colour;
            cableStartWorld = getPinWorldPos(cableSourceNodeId, cableSourcePinId);
            cableCurrentWorld = worldPt;
            cableFreehandWorld.clear();
            dragMode = DragMode::DrawCable;

            cables.erase(cables.begin() + foundCableIdx);
            syncAudioRoutes();
            repaint();
            return;
        }
        else
        {
            // Inizia il disegno di un nuovo cavo
            isUnhookingExistingCable = false;
            dragMode = DragMode::DrawCable;
            cableSourceNodeId = pin.nodeId;
            cableSourcePinId = pin.pinId;
            cableSourceIsOutput = pin.isOutput;
            cableSourceIsMod = pin.isMod;
            cableSourceIsTrigger = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            cableSourceShape = pin.shapeType;
            cableSourceColour = pin.colour;
            cableStartWorld = pin.worldPos;
            cableCurrentWorld = worldPt;
            cableFreehandWorld.clear();
            cableFreehandWorld.push_back(pin.worldPos);
            repaint();
            return;
        }
    }

    // 7b. Click su un cavo: selezionalo con tasto sinistro o eliminalo con tasto destro
    {
        juce::String nearCableId = findAudioCableNear(worldPt, 18.0f);
        if (nearCableId.isNotEmpty())
        {
            if (e.mods.isRightButtonDown())
            {
                cables.erase(std::remove_if(cables.begin(), cables.end(),
                    [&](const CanvasCable& c) { return c.id == nearCableId; }),
                    cables.end());
                selectedCableId.clear();
                syncAudioRoutes();
                repaint();
                return;
            }
            else
            {
                selectedCableId = nearCableId;
                selectedNodeId.clear();
                selectedPinNodeId.clear();
                selectedPinId.clear();
                repaint();
                return;
            }
        }
    }

    // 7b. Click su una Manopola Pastello per ruotarla
    for (const auto& kh : collectCanvasKnobs())
    {
        if (kh.worldBounds.contains(worldPt))
        {
            for (const auto& n : nodes)
            {
                if (n.id != kh.nodeId)
                    continue;
                if (n.type == NodeType::NestedEffect)
                {
                    for (const auto& b : n.innerBlocks)
                        if (b.id == kh.blockId)
                            for (const auto& p : b.params)
                                if (p.key == kh.paramKey)
                                {
                                    dragMode = DragMode::TurnKnob;
                                    draggedNodeId = n.id;
                                    draggedBlockId = b.id;
                                    draggedParamKey = p.key;
                                    dragStartMouse = pt;
                                    dragStartKnobNorm = p.getNormalized();
                                    return;
                                }
                }
                else if (n.type == NodeType::InstrumentNode)
                {
                    for (const auto& p : n.directParams)
                    {
                        if (p.key == kh.paramKey)
                        {
                            dragMode = DragMode::TurnKnob;
                            draggedNodeId = n.id;
                            draggedBlockId = "direct";
                            draggedParamKey = p.key;
                            dragStartMouse = pt;
                            dragStartKnobNorm = p.getNormalized();
                            return;
                        }
                    }
                }
                else if (n.type == NodeType::MasterOut && !n.directParams.empty())
                {
                    dragMode = DragMode::TurnKnob;
                    draggedNodeId = n.id;
                    draggedBlockId = "direct";
                    draggedParamKey = n.directParams[0].key;
                    dragStartMouse = pt;
                    dragStartKnobNorm = n.directParams[0].getNormalized();
                    return;
                }
            }
        }
    }

    // 7c. Click su un Nodo per selezionarlo, chiuderlo [X], testarlo o spostarlo
    for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i)
    {
        const auto& n = nodes[static_cast<size_t>(i)];
        const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);
        if (box.contains(worldPt))
        {
            // Pulsante [X] di chiusura / rimozione del modulo dal canvas
            if (n.type != NodeType::RockPiCentral && n.type != NodeType::MasterOut)
            {
                juce::Rectangle<float> closeBtn(box.getRight() - 26.0f, box.getY() + 7.0f, 20.0f, 20.0f);
                if (closeBtn.contains(worldPt))
                {
                    deleteNode(n.id);
                    return;
                }

                // Pulsante Power / Bypass
                if (n.type == NodeType::InstrumentNode || n.type == NodeType::NestedEffect)
                {
                    const auto pwrBtn = CanvasGraphHelper::getBypassButtonBounds(box);
                    if (pwrBtn.contains(worldPt))
                    {
                        nodes[static_cast<size_t>(i)].isBypassed = !nodes[static_cast<size_t>(i)].isBypassed;
                        audioEngine.setNodeBypass(n.id, nodes[static_cast<size_t>(i)].isBypassed);
                        repaint();
                        return;
                    }
                }
            }

            // Click su Header a tendina (Accordion) di un Modulo Hardware
            if (n.type == NodeType::HardwareModule && !n.outputGroups.empty())
            {
                const bool isEye = (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye");
                const bool isMic = (n.isIntegratedMic || n.id == "hw-mic" || n.id == "hw-drum");
                float curY = isEye ? (box.getY() + 168.0f) : (isMic ? (box.getY() + 104.0f) : (box.getY() + 96.0f));

                for (size_t gIdx = 0; gIdx < n.outputGroups.size(); ++gIdx)
                {
                    const auto& grp = n.outputGroups[gIdx];
                    if (!grp.parentGroup.isEmpty())
                        continue;

                    juce::Rectangle<float> hdrBox(box.getX() + 8.0f, curY, box.getWidth() - 16.0f, 24.0f);
                    const juce::Rectangle<float> switchBox(hdrBox.getX() + 4.0f, hdrBox.getCentreY() - 8.0f, 32.0f, 16.0f);

                    if (switchBox.contains(worldPt))
                    {
                        nodes[static_cast<size_t>(i)].outputGroups[gIdx].isEnabled =
                            !nodes[static_cast<size_t>(i)].outputGroups[gIdx].isEnabled;
                        repaint();
                        return;
                    }
                    else if (hdrBox.contains(worldPt))
                    {
                        nodes[static_cast<size_t>(i)].outputGroups[gIdx].isExpanded =
                            !nodes[static_cast<size_t>(i)].outputGroups[gIdx].isExpanded;

                        nodes[static_cast<size_t>(i)].size.y = getNodeCalculatedHeight(nodes[static_cast<size_t>(i)]);
                        repaint();
                        return;
                    }

                    curY += 26.0f;

                    if (grp.items.empty()) // Ha sottogruppi (es. Mano DX / Mano SX)
                    {
                        if (grp.isExpanded)
                        {
                            for (size_t sIdx = 0; sIdx < n.outputGroups.size(); ++sIdx)
                            {
                                const auto& sub = n.outputGroups[sIdx];
                                if (sub.parentGroup != grp.id)
                                    continue;

                                juce::Rectangle<float> subHdrBox(box.getX() + 14.0f, curY, box.getWidth() - 28.0f, 22.0f);
                                const juce::Rectangle<float> subSwitchBox(subHdrBox.getX() + 3.0f, subHdrBox.getCentreY() - 7.0f, 28.0f, 14.0f);

                                if (subSwitchBox.contains(worldPt))
                                {
                                    nodes[static_cast<size_t>(i)].outputGroups[sIdx].isEnabled =
                                        !nodes[static_cast<size_t>(i)].outputGroups[sIdx].isEnabled;
                                    repaint();
                                    return;
                                }
                                else if (subHdrBox.contains(worldPt))
                                {
                                    nodes[static_cast<size_t>(i)].outputGroups[sIdx].isExpanded =
                                        !nodes[static_cast<size_t>(i)].outputGroups[sIdx].isExpanded;

                                    nodes[static_cast<size_t>(i)].size.y = getNodeCalculatedHeight(nodes[static_cast<size_t>(i)]);
                                    repaint();
                                    return;
                                }

                                curY += 24.0f;

                                if (sub.isExpanded)
                                {
                                    for (const auto& item : sub.items)
                                    {
                                        if (item.isTrigger)
                                        {
                                            juce::Rectangle<float> row(box.getX() + 18.0f, curY, box.getWidth() - 36.0f, 24.0f);
                                            juce::Rectangle<float> testBtn(row.getRight() - 40.0f, row.getY() + 2.5f, 32.0f, 19.0f);
                                            const float soundBtnX = row.getX() + 76.0f;
                                            const float soundBtnW = testBtn.getX() - soundBtnX - 4.0f;
                                            juce::Rectangle<float> soundBtn(soundBtnX, row.getY() + 2.5f, soundBtnW, 19.0f);

                                            const bool isSec = (sub.parentGroup == "eye_sx" || sub.id.contains("sx") || grp.id == "eye_sx");
                                            int subVoice = 0;
                                            if (item.pinId.endsWith("_mid")) subVoice = 1;
                                            else if (item.pinId.endsWith("_rng")) subVoice = 2;
                                            else if (item.pinId.endsWith("_pky")) subVoice = 3;
                                            const int globalFingerIdx = isSec ? (subVoice + 4) : subVoice;

                                            if (soundBtn.contains(worldPt))
                                            {
                                                if (!sub.isEnabled || !grp.isEnabled)
                                                    return;

                                                juce::PopupMenu m;
                                                m.addSectionHeader("SUONO / STRUMENTO: " + item.label + (isSec ? " (SX)" : " (DX)"));

                                                const int currentSndId = audioEngine.getEyeFingerSound(globalFingerIdx);

                                                juce::PopupMenu subDrums;
                                                subDrums.addItem(1, "Cassa (Kick)", true, currentSndId == 0);
                                                subDrums.addItem(2, "Rullante (Snare)", true, currentSndId == 1);
                                                subDrums.addItem(3, "Charleston (Hi-Hat)", true, currentSndId == 2);
                                                subDrums.addItem(4, "Percussione / Tom", true, currentSndId == 3);
                                                subDrums.addItem(14, "Latta Giocattolo", true, currentSndId == 13);
                                                m.addSubMenu("Batteria & Percussioni", subDrums);

                                                juce::PopupMenu subMelodic;
                                                subMelodic.addItem(5, "Piano Feltro (Do3)", true, currentSndId == 4);
                                                subMelodic.addItem(6, "Piano Feltro (Re3)", true, currentSndId == 5);
                                                subMelodic.addItem(7, "Piano Feltro (Mi3)", true, currentSndId == 6);
                                                subMelodic.addItem(8, "Piano Feltro (Sol3)", true, currentSndId == 7);
                                                subMelodic.addItem(9, "Piano Feltro (La3)", true, currentSndId == 8);
                                                subMelodic.addItem(10, "Piano Feltro (Do4)", true, currentSndId == 9);
                                                subMelodic.addItem(15, "Carillon a Manovella", true, currentSndId == 14);
                                                m.addSubMenu("Piano & Melodici SFZ", subMelodic);

                                                juce::PopupMenu subSynth;
                                                subSynth.addItem(11, "Synth Lead (Do4)", true, currentSndId == 10);
                                                subSynth.addItem(12, "Pad Gessetto (Mi4)", true, currentSndId == 11);
                                                subSynth.addItem(13, "808 Sub Bass", true, currentSndId == 12);
                                                m.addSubMenu("Sintesi Faust 3D & Bassi", subSynth);

                                                std::vector<int> canvasInstIndices;
                                                for (size_t ci = 0; ci < nodes.size(); ++ci)
                                                    if (nodes[ci].type == NodeType::InstrumentNode || nodes[ci].hasInstrument)
                                                        canvasInstIndices.push_back(static_cast<int>(ci));

                                                if (!canvasInstIndices.empty())
                                                {
                                                    m.addSeparator();
                                                    juce::PopupMenu subCanvas;
                                                    for (size_t k = 0; k < canvasInstIndices.size(); ++k)
                                                    {
                                                        const auto& cNode = nodes[static_cast<size_t>(canvasInstIndices[k])];
                                                        subCanvas.addItem(100 + static_cast<int>(k), "Collega a " + cNode.label);
                                                    }
                                                    m.addSubMenu("Collega a Strumento Canvas", subCanvas);
                                                }

                                                bool hasCable = false;
                                                for (const auto& c : cables)
                                                {
                                                    if (c.sourceNodeId == n.id && c.sourcePinId == item.pinId)
                                                    {
                                                        hasCable = true;
                                                        break;
                                                    }
                                                }
                                                if (hasCable)
                                                {
                                                    m.addSeparator();
                                                    m.addItem(999, "Scollega Cavo da Strumento Canvas");
                                                }

                                                m.showMenuAsync(juce::PopupMenu::Options(),
                                                    [this, nodeId = n.id, pinId = item.pinId, globalFingerIdx, subVoice, isSec, canvasInstIndices](int res)
                                                    {
                                                        if (res <= 0) return;
                                                        if (res >= 1 && res <= 20)
                                                        {
                                                            const int chosenSoundId = (res == 14 ? 13 : (res == 15 ? 14 : (res - 1)));
                                                            audioEngine.setEyeFingerSound(globalFingerIdx, chosenSoundId);
                                                            audioEngine.triggerSoundById(chosenSoundId, 1.0f);
                                                            lastTriggerSourceCode.store(3);
                                                            lastTriggerFingerIdx.store(subVoice);
                                                            lastTriggerIsSecondary.store(isSec);
                                                            triggerPulseAlpha.store(1.0f);
                                                        }
                                                        else if (res >= 100 && res < 999)
                                                        {
                                                            const size_t k = static_cast<size_t>(res - 100);
                                                            if (k < canvasInstIndices.size())
                                                            {
                                                                const auto& targetNode = nodes[static_cast<size_t>(canvasInstIndices[k])];
                                                                cables.erase(std::remove_if(cables.begin(), cables.end(),
                                                                    [&](const CanvasCable& cc) {
                                                                        return (cc.sourceNodeId == nodeId && cc.sourcePinId == pinId);
                                                                    }), cables.end());
                                                                cables.push_back({
                                                                    "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                                                                    nodeId, pinId,
                                                                    targetNode.id, "trigger:in",
                                                                    false, juce::Colour(0xFFFF7A18), {}
                                                                });
                                                                syncAudioRoutes();
                                                                audioEngine.triggerFromSource(nodeId, subVoice, 1.0f, isSec);
                                                                lastTriggerSourceCode.store(3);
                                                                lastTriggerFingerIdx.store(subVoice);
                                                                lastTriggerIsSecondary.store(isSec);
                                                                triggerPulseAlpha.store(1.0f);
                                                            }
                                                        }
                                                        else if (res == 999)
                                                        {
                                                            cables.erase(std::remove_if(cables.begin(), cables.end(),
                                                                [&](const CanvasCable& cc) {
                                                                    return (cc.sourceNodeId == nodeId && cc.sourcePinId == pinId);
                                                                }), cables.end());
                                                            syncAudioRoutes();
                                                        }
                                                        repaint();
                                                    });
                                                return;
                                            }

                                            if (testBtn.contains(worldPt))
                                            {
                                                if (!sub.isEnabled || !grp.isEnabled)
                                                    return; // Disabilitato, niente suono!

                                                audioEngine.triggerFromSource(n.id, subVoice, 1.0f, isSec);
                                                lastTriggerSourceCode.store(3);
                                                lastTriggerFingerIdx.store(subVoice);
                                                lastTriggerIsSecondary.store(isSec);
                                                triggerPulseAlpha.store(1.0f);
                                                repaint();
                                                return;
                                            }
                                        }
                                        curY += 26.0f;
                                    }
                                }
                            }
                        }
                    }
                    else // Gruppo lineare diretto (es. Modulo Batteria)
                    {
                        if (grp.isExpanded)
                        {
                            for (const auto& item : grp.items)
                            {
                                if (item.isTrigger)
                                {
                                    juce::Rectangle<float> row(box.getX() + 10.0f, curY, box.getWidth() - 20.0f, 24.0f);
                                    juce::Rectangle<float> testBtn(row.getRight() - 55.0f, row.getY() + 3.0f, 38.0f, 20.0f);
                                    if (testBtn.contains(worldPt))
                                    {
                                        if (!grp.isEnabled)
                                            return;

                                        audioEngine.triggerFromSource(n.id, 0, 1.0f, false);
                                        lastTriggerSourceCode.store(1);
                                        triggerPulseAlpha.store(1.0f);
                                        repaint();
                                        return;
                                    }
                                }
                                curY += 26.0f;
                            }
                        }
                    }
                    curY += 4.0f;
                }
            }
            // Tasto [TEST] Trigger sul Modulo Batteria (fallback classico)
            if (n.id == "hw-drum" || n.id == "hw-mic" || n.isIntegratedMic)
            {
                const float trigRowY = n.isIntegratedMic ? (box.getY() + 102.0f) : (box.getY() + 96.0f);
                juce::Rectangle<float> trigRow(box.getX() + 12.0f, trigRowY, box.getWidth() - 24.0f, 26.0f);
                juce::Rectangle<float> testBtn(trigRow.getRight() - 65.0f, trigRow.getY() + 3.0f, 40.0f, 20.0f);
                if (testBtn.contains(worldPt))
                {
                    audioEngine.triggerFromSource(n.id, 0, 1.0f);
                    lastTriggerSourceCode.store(n.id == "hw-mic" ? 2 : (n.id == "hw-drum" ? 1 : 3));
                    triggerPulseAlpha.store(1.0f);
                    repaint();
                    return;
                }
            }

            if (n.type == NodeType::NestedEffect)
            {
                juce::Rectangle<float> expBadge(box.getRight() - 92.0f, box.getY() + 10.0f, 62.0f, 22.0f);
                if (expBadge.contains(worldPt))
                {
                    expandedNodeId = n.id;
                    repaint();
                    return;
                }
                selectedNodeId = n.id;
                selectedCableId.clear();
                selectedPinNodeId.clear();
                selectedPinId.clear();
                repaint();
            }
            else if (n.type == NodeType::InstrumentNode)
            {
                selectedNodeId = n.id;
                selectedCableId.clear();
                selectedPinNodeId.clear();
                selectedPinId.clear();

                const bool isPerc = (n.id.containsIgnoreCase("drum") || n.id.containsIgnoreCase("percuss"));
                if (isPerc)
                {
                    // 1. Pulsante Mode Toggle [⚙ ARRANGIA]
                    const juce::Rectangle<float> modeBtn(box.getRight() - 145.0f, box.getY() + 8.0f, 90.0f, 18.0f);
                    if (modeBtn.contains(worldPt))
                    {
                        nodes[static_cast<size_t>(i)].isDrumAdvanced = !nodes[static_cast<size_t>(i)].isDrumAdvanced;
                        nodes[static_cast<size_t>(i)].size.y = nodes[static_cast<size_t>(i)].isDrumAdvanced ? 245.0f : 185.0f;
                        if (nodes[static_cast<size_t>(i)].isDrumAdvanced)
                        {
                            nodes[static_cast<size_t>(i)].directParams = {
                                { "kick_pitch", "Kick Pitch", 35.0f, 120.0f, 44.0f, 44.0f, " Hz", true },
                                { "kick_punch", "Punch",      0.2f, 2.0f, 0.85f, 0.85f, "",     true },
                                { "kick_decay", "Decay",      0.05f, 1.4f, 0.85f, 0.85f, " s",  true },
                                { "drive",      "Drive",      0.0f, 100.0f, 20.0f, 20.0f, " %", true }
                            };
                        }
                        else
                        {
                            nodes[static_cast<size_t>(i)].directParams = {
                                { "vol",   "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",     true },
                                { "pitch", "Pitch", -12.0f, 12.0f, 0.0f, 0.0f, " st", true },
                                { "decay", "Decay",  0.25f, 2.5f, 1.0f, 1.0f, " x",  true }
                            };
                        }
                        syncAudioRoutes();
                        repaint();
                        return;
                    }

                    if (nodes[static_cast<size_t>(i)].isDrumAdvanced)
                    {
                        // 2. Barra Preset
                        const juce::Rectangle<float> presetBar(box.getX() + 52.0f, box.getY() + 32.0f, box.getWidth() - 100.0f, 18.0f);
                        if (presetBar.contains(worldPt))
                        {
                            nodes[static_cast<size_t>(i)].drumPresetIdx = (nodes[static_cast<size_t>(i)].drumPresetIdx + 1) % 4;
                            audioEngine.setModulation(n.id, "preset", static_cast<float>(nodes[static_cast<size_t>(i)].drumPresetIdx) / 4.0f);
                            audioEngine.triggerInstrument(n.id, 0, 0.95f);
                            lastTriggerSourceCode.store(4);
                            triggerPulseAlpha.store(1.0f);
                            repaint();
                            return;
                        }

                        // 3. Tab Voci
                        const float tabW = (box.getWidth() - 100.0f) / 4.0f;
                        for (int v = 0; v < 4; ++v)
                        {
                            const juce::Rectangle<float> tabBox(box.getX() + 52.0f + static_cast<float>(v) * tabW, box.getY() + 54.0f, tabW - 2.0f, 20.0f);
                            if (tabBox.contains(worldPt))
                            {
                                nodes[static_cast<size_t>(i)].drumEditVoice = v;
                                if (v == 0) // Kick
                                {
                                    nodes[static_cast<size_t>(i)].directParams = {
                                        { "kick_pitch", "Kick Pitch", 35.0f, 120.0f, 44.0f, 44.0f, " Hz", true },
                                        { "kick_punch", "Punch",      0.2f, 2.0f, 0.85f, 0.85f, "",     true },
                                        { "kick_decay", "Decay",      0.05f, 1.4f, 0.85f, 0.85f, " s",  true },
                                        { "drive",      "Drive",      0.0f, 100.0f, 20.0f, 20.0f, " %", true }
                                    };
                                }
                                else if (v == 1) // Snare
                                {
                                    nodes[static_cast<size_t>(i)].directParams = {
                                        { "snare_tone",   "Corpo",  110.0f, 380.0f, 180.0f, 180.0f, " Hz", true },
                                        { "snare_snappy", "Snappy", 0.0f, 1.8f, 1.15f, 1.15f, "",     true },
                                        { "snare_decay",  "Decay",  0.04f, 0.7f, 0.18f, 0.18f, " s",  true },
                                        { "drive",        "Drive",  0.0f, 100.0f, 20.0f, 20.0f, " %", true }
                                    };
                                }
                                else if (v == 2) // Hat
                                {
                                    nodes[static_cast<size_t>(i)].directParams = {
                                        { "hat_cutoff", "Cutoff", 3200.0f, 12200.0f, 8200.0f, 8200.0f, " Hz", true },
                                        { "hat_sizzle", "Sizzle", 0.6f, 3.8f, 1.4f, 1.4f, "",     true },
                                        { "hat_decay",  "Decay",  0.02f, 0.5f, 0.045f, 0.045f, " s", true },
                                        { "vol",        "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",     true }
                                    };
                                }
                                else // Tom
                                {
                                    nodes[static_cast<size_t>(i)].directParams = {
                                        { "tom_pitch", "Pitch",  60.0f, 260.0f, 82.0f, 82.0f, " Hz", true },
                                        { "tom_decay", "Decay",  0.08f, 1.2f, 0.45f, 0.45f, " s",  true },
                                        { "drive",     "Drive",  0.0f, 100.0f, 20.0f, 20.0f, " %", true },
                                        { "vol",       "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",     true }
                                    };
                                }
                                audioEngine.triggerInstrument(n.id, v, 0.95f);
                                lastTriggerSourceCode.store(4);
                                triggerPulseAlpha.store(1.0f);
                                repaint();
                                return;
                            }
                        }
                    }
                    else
                    {
                        // 4. Pulsanti Quick-Test / Audition
                        const float btnW = (box.getWidth() - 96.0f) / 4.0f;
                        for (int v = 0; v < 4; ++v)
                        {
                            const juce::Rectangle<float> btnBox(box.getX() + 50.0f + static_cast<float>(v) * btnW, box.getY() + 48.0f, btnW - 3.0f, 22.0f);
                            if (btnBox.contains(worldPt))
                            {
                                audioEngine.triggerInstrument(n.id, v, 0.95f);
                                lastTriggerSourceCode.store(4);
                                triggerPulseAlpha.store(1.0f);
                                repaint();
                                return;
                            }
                        }
                    }
                }

                // Default trigger per altri strumenti
                audioEngine.triggerInstrument(n.id, 0, 0.9f);
                lastTriggerSourceCode.store(4);
                triggerPulseAlpha.store(1.0f);
                repaint();
            }
            else if (n.type == NodeType::HardwareModule)
            {
                // TUTTI i moduli hardware possono essere selezionati ed eliminati!
                selectedNodeId = n.id;
                selectedCableId.clear();
                selectedPinNodeId.clear();
                selectedPinId.clear();
                repaint();
            }
            else
            {
                selectedNodeId.clear();
                repaint();
            }

            if (n.type != NodeType::RockPiCentral)
            {
                dragMode = DragMode::MoveNode;
                draggedNodeId = n.id;
                dragStartMouse = worldPt;
                dragStartOffset = n.pos;
                return;
            }
        }
    }

    // 7d. Altrimenti Pan libero del Canvas Infinito (deseleziona)
    selectedNodeId.clear();
    dragMode = DragMode::PanCanvas;
    dragStartMouse = pt;
    dragStartOffset = canvasOffset;
    panVelocity = { 0.0f, 0.0f };
    repaint();
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    currentMousePos = pt;

    if (dragMode == DragMode::PanCanvas)
    {
        canvasOffset = dragStartOffset + (pt - dragStartMouse);
        panVelocity = { 0.0f, 0.0f };
        repaint();
    }
    else if (dragMode == DragMode::MoveNode)
    {
        const auto worldPt = screenToWorld(pt);
        for (auto& n : nodes)
        {
            if (n.id == draggedNodeId)
            {
                n.pos = dragStartOffset + (worldPt - dragStartMouse);
                n.targetPos = n.pos;
                n.isAnimatingPos = false;
                break;
            }
        }
        repaint();
    }
    else if (dragMode == DragMode::MoveInnerBlock)
    {
        for (auto& n : nodes)
            if (n.id == draggedNodeId)
                for (auto& b : n.innerBlocks)
                    if (b.id == draggedBlockId)
                        b.pos = dragStartOffset + (pt - dragStartMouse);
        repaint();
    }
    else if (dragMode == DragMode::TurnKnob || dragMode == DragMode::TurnInnerKnob)
    {
        const float dy = dragStartMouse.y - pt.y;
        const float newNorm = juce::jlimit(0.0f, 1.0f, dragStartKnobNorm + dy / 150.0f);
        for (auto& n : nodes)
        {
            if (n.id != draggedNodeId)
                continue;
            if (draggedBlockId == "direct" && !n.directParams.empty())
            {
                for (auto& p : n.directParams)
                {
                    if (p.key == draggedParamKey)
                    {
                        p.setFromNormalized(newNorm);
                        if (n.type == NodeType::MasterOut)
                        {
                            const float g = juce::Decibels::decibelsToGain(p.value);
                            audioEngine.setMasterVolume(g);
                            synthMasterGain.store(g);
                            appMasterVolume.store(g);
                        }
                        else
                            audioEngine.setModulation(n.id, p.key, newNorm);
                        break;
                    }
                }
            }
            else
            {
                for (auto& b : n.innerBlocks)
                {
                    if (b.id == draggedBlockId)
                    {
                        for (auto& p : b.params)
                        {
                            if (p.key == draggedParamKey)
                            {
                                p.setFromNormalized(newNorm);
                                audioEngine.setModulation(n.id, p.key, newNorm);
                                break;
                            }
                        }
                    }
                }
            }
        }
        repaint();
    }
    else if (dragMode == DragMode::DrawCable)
    {
        cableCurrentWorld = screenToWorld(pt);

        // Snap magnetico verso la presa compatibile per tipo segnale
        const PinHit* snapPin = nullptr;
        float snapDist = 28.0f;
        for (const auto& pin : collectAllPins())
        {
            const bool validDir = (pin.isOutput != cableSourceIsOutput);
            const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const bool typeCompatible = cableSourceIsTrigger ? pinIsTrig : (cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

            if (validDir && pin.nodeId != cableSourceNodeId && typeCompatible)
            {
                const float d = pin.worldPos.getDistanceFrom(cableCurrentWorld);
                if (d <= snapDist)
                {
                    snapDist = d;
                    snapPin = &pin;
                }
            }
        }
        if (snapPin != nullptr)
            cableCurrentWorld = snapPin->worldPos;

        if (cableFreehandWorld.empty() || cableFreehandWorld.back().getDistanceFrom(cableCurrentWorld) >= 10.0f)
            cableFreehandWorld.push_back(cableCurrentWorld);
        repaint();
    }
    else if (dragMode == DragMode::DragSidebarItem)
    {
        const auto worldPt = screenToWorld(pt);
        if (!draggedCatalogItem.isEffect)
        {
            hoveredHwNodeId = findHwModuleAt(worldPt);
            hoveredAudioCableId.clear();
        }
        else
        {
            hoveredAudioCableId = findAudioCableNear(worldPt, 45.0f);
            hoveredHwNodeId.clear();
        }
        repaint();
    }
    else if (dragMode == DragMode::DragPolaroid)
    {
        hoveredSlotIndex = -1;
        for (int i = 0; i < 5; ++i)
            if (getSlotBounds(i).contains(pt))
                hoveredSlotIndex = i;
        repaint();
    }
    else if (dragMode == DragMode::DragCanvasMicSensitivity)
    {
        const auto worldPt = screenToWorld(pt);
        for (const auto& n : nodes)
        {
            if (n.id == draggedNodeId)
            {
                juce::Rectangle<float> slTrack(n.pos.x + 12.0f, n.pos.y + 74.0f, n.size.x - 24.0f, 18.0f);
                if (slTrack.getWidth() > 0.0f)
                {
                    micSensitivity = juce::jlimit(0.02f, 1.0f, (worldPt.x - slTrack.getX()) / slTrack.getWidth());
                    repaint();
                }
                break;
            }
        }
    }
    else if (dragMode == DragMode::DragSettingsVolume)
    {
        const auto volTrack = getSettingsVolumeTrackBounds();
        if (volTrack.getWidth() > 0.0f)
        {
            const float norm = juce::jlimit(0.0f, 1.0f, (pt.x - volTrack.getX()) / volTrack.getWidth());
            setAppMasterVolume(norm * 2.0f);
        }
    }

}

void MainComponent::mouseUp(const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const auto worldPt = screenToWorld(pt);

    if (dragMode == DragMode::DrawCable)
    {
        bool connected = false;
        const PinHit* bestPin = nullptr;
        float bestDist = 32.0f;
        const auto allPins = collectAllPins();
        for (const auto& pin : allPins)
        {
            const bool validDir = (pin.isOutput != cableSourceIsOutput);
            const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const bool typeCompatible = cableSourceIsTrigger ? pinIsTrig : (cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

            if (validDir && pin.nodeId != cableSourceNodeId && typeCompatible)
            {
                const float dist = pin.worldPos.getDistanceFrom(worldPt);
                if (dist <= bestDist)
                {
                    bestDist = dist;
                    bestPin = &pin;
                }
            }
        }

        if (bestPin != nullptr)
        {
            const auto& pin = *bestPin;
            const auto& srcNode = cableSourceIsOutput ? cableSourceNodeId : pin.nodeId;
            const auto& srcPin  = cableSourceIsOutput ? cableSourcePinId  : pin.pinId;
            const auto& dstNode = cableSourceIsOutput ? pin.nodeId        : cableSourceNodeId;
            const auto& dstPin  = cableSourceIsOutput ? pin.pinId         : cableSourcePinId;
            const bool isMod = cableSourceIsMod || pin.isMod;
            const bool isTrig = cableSourceIsTrigger || (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const juce::Colour cableCol = isTrig ? juce::Colour(0xFFFF7A18) : (isMod ? (cableSourceIsOutput ? cableSourceColour : pin.colour) : MiMOLookAndFeel::fountainInk);

            // Regola: l'input (dstNode, dstPin) riceve al massimo un solo cavo.
            // Se c'era già un cavo su questo ingresso, viene sostituito.
            // L'output (srcNode, srcPin) può invece avere cavi multipli (fan-out illimitato verso più moduli)!
            cables.erase(std::remove_if(cables.begin(), cables.end(),
                [&](const CanvasCable& c) {
                    // Cancella qualsiasi cavo preesistente collegato all'ingresso di destinazione
                    if (c.targetNodeId == dstNode && c.targetPinId == dstPin)
                        return true;
                    // Prevenzione cavi identici duplicati tra la stessa coppia di pin
                    if (c.sourceNodeId == srcNode && c.sourcePinId == srcPin &&
                        c.targetNodeId == dstNode && c.targetPinId == dstPin)
                        return true;
                    // Prevenzione loop inversi
                    if (c.sourceNodeId == dstNode && c.sourcePinId == dstPin &&
                        c.targetNodeId == srcNode && c.targetPinId == srcPin)
                        return true;
                    return false;
                }),
                cables.end());

            cables.push_back({
                "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                srcNode, srcPin,
                dstNode, dstPin,
                isMod, cableCol, {},
                isTrig
            });

            connected = true;
            syncAudioRoutes();
        }

        // Se non è stato collegato a nessun pin ed era un cavo preesistente staccato:
        // "prendendo quel nodo e lasciandolo nel nulla si stacchi" -> resta cancellato!
        isUnhookingExistingCable = false;
        cableFreehandWorld.clear();
        dragMode = DragMode::None;
        repaint();
    }
    else if (dragMode == DragMode::MoveNode)
    {
        // "scollegandolo e riponendolo nuovamente nella lista dei componenti"
        const float sidebarThreshold = static_cast<float>(getWidth()) - (sidebarOpen ? 280.0f : 50.0f);
        if (pt.x >= sidebarThreshold)
        {
            deleteNode(draggedNodeId);
            dragMode = DragMode::None;
            draggedNodeId.clear();
            repaint();
            return;
        }

        // Solo se il modulo hardware NON ha cavi collegati, verifica se agganciarlo automaticamente a MiMO
        for (const auto& n : nodes)
        {
            if (n.id == draggedNodeId && n.type == NodeType::HardwareModule)
            {
                bool hasExistingCable = false;
                for (const auto& c : cables)
                {
                    if (c.sourceNodeId == n.id && c.sourcePinId == "audio:out")
                    {
                        hasExistingCable = true;
                        break;
                    }
                }

                if (!hasExistingCable)
                {
                    for (const auto& rPi : nodes)
                    {
                        if (rPi.id == "rockpi")
                        {
                            static constexpr float keyXFractions[5] = { 0.1490f, 0.2958f, 0.4426f, 0.5895f, 0.7349f };
                            for (int i = 0; i < 5; ++i)
                            {
                                const float targetX = rPi.pos.x + rPi.size.x * keyXFractions[i];
                                const float targetY = rPi.pos.y + rPi.size.y + 4.0f;
                                const auto plugPos = juce::Point<float>(n.pos.x + 29.0f, n.pos.y);

                                if (std::abs(plugPos.x - targetX) <= 50.0f && std::abs(plugPos.y - targetY) <= 120.0f)
                                {
                                    const juce::String targetPin = "audio:p" + juce::String(i + 1);
                                    bool slotOccupied = false;
                                    for (const auto& c : cables)
                                    {
                                        if (c.targetNodeId == "rockpi" && c.targetPinId == targetPin)
                                        {
                                            slotOccupied = true;
                                            break;
                                        }
                                    }

                                    if (!slotOccupied)
                                    {
                                        cables.push_back({
                                            "c-" + juce::String(juce::Time::getMillisecondCounter()),
                                            n.id, "audio:out",
                                            "rockpi", targetPin,
                                            false, MiMOLookAndFeel::fountainInk, {}
                                        });
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
        dragMode = DragMode::None;
        draggedNodeId.clear();
        repaint();
    }
    else if (dragMode == DragMode::DragSidebarItem)
    {
        if (!draggedCatalogItem.isEffect)
        {
            if (draggedCatalogItem.id == "hw-drum")
            {
                if (!isDrumInCanvas())
                    toggleDrumInCanvas();

                for (auto& n : nodes)
                {
                    if (n.id == "hw-drum")
                    {
                        n.pos = worldPt - n.size * 0.5f;
                        break;
                    }
                }
            }
            else if (draggedCatalogItem.id == "integrated-mic")
            {
                if (!isMicInCanvas())
                    toggleMicInCanvas();

                for (auto& n : nodes)
                {
                    if (n.isIntegratedMic || n.id == "hw-mic")
                    {
                        n.pos = worldPt - n.size * 0.5f;
                        break;
                    }
                }
            }
            else if (draggedCatalogItem.id == "integrated-webcam" || draggedCatalogItem.id == "integrated-cam")
            {
                if (!isCameraInCanvas())
                    toggleCameraInCanvas();

                for (auto& n : nodes)
                {
                    if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
                    {
                        n.pos = worldPt - n.size * 0.5f;
                        break;
                    }
                }
            }
            else
            {
                const juce::String targetHwId = findHwModuleAt(worldPt);
                if (targetHwId.isNotEmpty())
                {
                    for (auto& n : nodes)
                    {
                        if (n.id == targetHwId)
                        {
                            n.hasInstrument = true;
                            n.instrumentName = draggedCatalogItem.name;
                            n.instrumentEngine = draggedCatalogItem.engine;
                            n.crayonColour = draggedCatalogItem.colour;
                            break;
                        }
                    }
                    syncAudioRoutes();
                }
                else
                {
                    auto instNode = createInstrumentNodeFromCatalog(draggedCatalogItem, worldPt - juce::Point<float>{ 115.0f, 60.0f }, fxCounter++);
                    nodes.push_back(instNode);

                    for (const auto& n : nodes)
                    {
                        if (n.type == NodeType::MasterOut)
                        {
                            cables.push_back({
                                "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                                instNode.id, "audio:out",
                                n.id, "audio:in",
                                false, MiMOLookAndFeel::fountainInk, {}
                            });
                            break;
                        }
                    }
                    syncAudioRoutes();
                }
            }
        }
        else
        {
            juce::String cableId = findAudioCableNear(worldPt, 45.0f);
            if (cableId.isNotEmpty())
            {
                insertEffectOnCable(draggedCatalogItem, cableId, worldPt);
                syncAudioRoutes();
            }
            else
            {
                auto fxNode = createFxNodeFromCatalog(draggedCatalogItem, worldPt - juce::Point<float>{ 130.0f, 80.0f }, fxCounter++, false);
                nodes.push_back(fxNode);
                syncAudioRoutes();
            }
        }
        hoveredHwNodeId.clear();
        hoveredAudioCableId.clear();
        dragMode = DragMode::None;
        repaint();
    }
    else if (dragMode == DragMode::DragPolaroid)
    {
        for (int i = 0; i < 5; ++i)
        {
            if (getSlotBounds(i).contains(pt))
            {
                assignPolaroidToSlot(i, pendingPolaroid);
                break;
            }
        }
        hasPendingPolaroid = false;
        hoveredSlotIndex = -1;
        dragMode = DragMode::None;
        repaint();
    }
    else
    {
        if (dragMode == DragMode::DragSettingsVolume)
            saveSettings();

        dragMode = DragMode::None;
        repaint();
    }
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto pt = e.position;

    // 1. Doppio clic su uno dei 5 Slot Scena: riapre temporaneamente l'editor Canvas di quella Polaroid (Quick-Edit)!
    for (int i = 0; i < 5; ++i)
    {
        if (getSlotBounds(i).contains(pt) && sceneSlots[i].isValid)
        {
            nodes = sceneSlots[i].snapshot.nodes;
            cables = sceneSlots[i].snapshot.cables;
            editingSlotIndex = i;
            activeSceneSlot = i;
            activeTab = ActiveTab::Studio;
            repaint();
            return;
        }
    }

    if (activeTab != ActiveTab::Studio || expandedNodeId.isNotEmpty())
        return;

    // 2. Doppio clic su un Nodo nel Canvas STUDIO
    const auto worldPt = screenToWorld(pt);
    for (const auto& n : nodes)
    {
        if (juce::Rectangle<float>(n.pos.x, n.pos.y, n.size.x, n.size.y).contains(worldPt))
        {
            if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
            {
                eyeDrawerOpen = true;
                eyeDrawerTab = 0;
                repaint();
                return;
            }
            if (n.type == NodeType::NestedEffect)
            {
                expandedNodeId = n.id;
                repaint();
                return;
            }
        }
    }
}

void MainComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // 0. Scorrimento verticale della Sidebar Destra (se il puntatore si trova sopra di essa)
    if (sidebarOpen && e.position.x >= static_cast<float>(getWidth() - 308))
    {
        const float maxScroll = getSidebarMaxScroll();
        if (maxScroll > 0.0f)
        {
            const float wheelDelta = (std::abs(wheel.deltaY) < 0.35f) ? (wheel.deltaY * 120.0f) : (wheel.deltaY * 45.0f);
            sidebarScrollY = juce::jlimit(0.0f, maxScroll, sidebarScrollY - wheelDelta);
            repaint();
            return;
        }
    }

    if (activeTab != ActiveTab::Studio || expandedNodeId.isNotEmpty() || hasPendingPolaroid)
        return;

    // 1. Zoom centrato sul cursore se l'utente tiene premuto Ctrl / Cmd / Alt
    if (e.mods.isCtrlDown() || e.mods.isCommandDown() || e.mods.isAltDown())
    {
        panVelocity = { 0.0f, 0.0f };
        const auto worldBefore = screenToWorld(e.position);
        const float factor = (wheel.deltaY > 0.0f) ? 1.08f : 0.92f;
        canvasZoom = juce::jlimit(0.45f, 2.2f, canvasZoom * factor);
        canvasOffset = e.position - worldBefore * canvasZoom;
        repaint();
        return;
    }

    // 2. Pan a due dita sul touchpad (o rotellina del mouse)
    float dx = wheel.deltaX;
    float dy = wheel.deltaY;

    // Supporto per Shift + rotellina mouse fisica -> scorrimento orizzontale
    if (dx == 0.0f && e.mods.isShiftDown())
    {
        dx = dy;
        dy = 0.0f;
    }

    // Limita spike anomali da driver
    dx = juce::jlimit(-1.5f, 1.5f, dx);
    dy = juce::jlimit(-1.5f, 1.5f, dy);

    if (std::abs(dx) <= 0.00001f && std::abs(dy) <= 0.00001f)
        return;

    // Distinzione intelligente: Touchpad ad alta frequenza vs Rotellina a scatti (Mouse fisico)
    if (std::abs(dx) < 0.35f && std::abs(dy) < 0.35f)
    {
        // TOUCHPAD / TRACKPAD:
        // Spostamento DIRETTO a latenza ZERO (la visuale segue le dita istantaneamente)
        const float touchSpeed = 120.0f;
        canvasOffset.x += dx * touchSpeed;
        canvasOffset.y += dy * touchSpeed;

        // Traccia il vettore di velocita cinetica per accompagnare con inerzia fluida al rilascio
        panVelocity.x = panVelocity.x * 0.40f + (dx * touchSpeed) * 0.60f;
        panVelocity.y = panVelocity.y * 0.40f + (dy * touchSpeed) * 0.60f;
        timeSinceLastWheel = 0.0f;
        repaint();
    }
    else
    {
        // ROTELLINA DEL MOUSE FISICA: impulso progressivo fluido
        const float wheelImpulse = 24.0f;
        panVelocity.x += dx * wheelImpulse;
        panVelocity.y += dy * wheelImpulse;
        timeSinceLastWheel = 0.0f;
    }
}

void MainComponent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    if (activeTab != ActiveTab::Studio || expandedNodeId.isNotEmpty() || hasPendingPolaroid)
        return;

    if (scaleFactor <= 0.0f)
        return;

    panVelocity = { 0.0f, 0.0f };
    const auto worldBefore = screenToWorld(e.position);
    canvasZoom = juce::jlimit(0.45f, 2.2f, canvasZoom * scaleFactor);
    canvasOffset = e.position - worldBefore * canvasZoom;
    repaint();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (hasPendingPolaroid)
    {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
        {
            commitPolaroidToAlbum(pendingPolaroid);
            return true;
        }
        if (key == juce::KeyPress::backspaceKey)
        {
            if (!polaroidHasTyped)
            {
                polaroidHasTyped = true;
                pendingPolaroid.title.clear();
            }
            else
            {
                pendingPolaroid.title = pendingPolaroid.title.dropLastCharacters(1);
            }
            repaint();
            return true;
        }

        const juce::juce_wchar ch = key.getTextCharacter();
        if (ch >= 32 && ch < 127)
        {
            if (!polaroidHasTyped)
            {
                polaroidHasTyped = true;
                pendingPolaroid.title.clear();
            }
            if (pendingPolaroid.title.length() < 26)
                pendingPolaroid.title += ch;
            repaint();
            return true;
        }
    }

    // Scorciatoia Modalita Notte ('N' o 'n')
    if (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N')
    {
        toggleDarkMode();
        return true;
    }

    // Tasti Hardware 1..5 (Selezione ed Esecuzione Scene o Assegnazione Rapida)
    const juce::juce_wchar kch = key.getTextCharacter();
    if (kch >= '1' && kch <= '5')
    {
        const int sIdx = kch - '1';
        if (activeTab == ActiveTab::Live)
        {
            if (selectedAlbumIndex >= 0 && selectedAlbumIndex < static_cast<int>(photoAlbum.size()))
            {
                assignPolaroidToSlot(sIdx, photoAlbum[static_cast<size_t>(selectedAlbumIndex)]);
                selectedAlbumIndex = -1;
            }
            else
            {
                triggerSceneSlot(sIdx);
            }
            return true;
        }
        if (activeTab == ActiveTab::Studio && editingSlotIndex == sIdx)
        {
            sceneSlots[editingSlotIndex].snapshot = { nodes, cables };
            editingSlotIndex = -1;
            activeTab = ActiveTab::Live;
            repaint();
            return true;
        }
    }

    // Tasto Spazio: 1-Button Looper (REC / PLAY / OVERDUB)
    if (key == juce::KeyPress::spaceKey && activeTab == ActiveTab::Live)
    {
        toggleLooper();
        return true;
    }

    // Eliminazione nodo, cavo o pin con tasto CANC (Delete o Backspace)
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedCableId.isNotEmpty())
        {
            cables.erase(std::remove_if(cables.begin(), cables.end(),
                [&](const CanvasCable& c) { return c.id == selectedCableId; }),
                cables.end());
            selectedCableId.clear();
            syncAudioRoutes();
            repaint();
            return true;
        }
        if (selectedPinNodeId.isNotEmpty())
        {
            cables.erase(std::remove_if(cables.begin(), cables.end(),
                [&](const CanvasCable& c) {
                    return (c.sourceNodeId == selectedPinNodeId && c.sourcePinId == selectedPinId) ||
                           (c.targetNodeId == selectedPinNodeId && c.targetPinId == selectedPinId);
                }),
                cables.end());
            selectedPinNodeId.clear();
            selectedPinId.clear();
            syncAudioRoutes();
            repaint();
            return true;
        }
        if (selectedNodeId.isNotEmpty())
        {
            deleteNode(selectedNodeId);
            return true;
        }
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (eyeDrawerOpen)
        {
            eyeDrawerOpen = false;
            repaint();
            return true;
        }
        if (editingSlotIndex >= 0)
        {
            sceneSlots[editingSlotIndex].snapshot = { nodes, cables };
            editingSlotIndex = -1;
            activeTab = ActiveTab::Live;
            repaint();
            return true;
        }
        if (selectedAlbumIndex >= 0)
        {
            selectedAlbumIndex = -1;
            repaint();
            return true;
        }
        if (expandedNodeId.isNotEmpty())
        {
            expandedNodeId.clear();
            repaint();
            return true;
        }
        if (selectedNodeId.isNotEmpty() || selectedCableId.isNotEmpty() || selectedPinNodeId.isNotEmpty())
        {
            selectedNodeId.clear();
            selectedCableId.clear();
            selectedPinNodeId.clear();
            selectedPinId.clear();
            syncAudioRoutes();
            repaint();
            return true;
        }
    }

    return false;
}

// =============================================================================
// MiMO Eye: Rendering Coerente Giorno/Notte & Finestra Modale QR Code AI
// =============================================================================

void MainComponent::drawModernFlatEyeCard(juce::Graphics& /*g*/, const CanvasNode& /*node*/)
{
    // Il rendering del nodo nel canvas è ora unificato direttamente in paintCanvasNode
    // per garantire il 100% di coerenza visiva sia in Modalità Giorno che in Modalità Notte.
}

void MainComponent::drawModernFlatEyeModal(juce::Graphics& g)
{
    // Backdrop oscurato coerente con paintNestedModal
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.55f));

    const auto modalBox = getEyeModalBounds();

    // Ombra morbida
    g.setColour(juce::Colours::black.withAlpha(isDarkMode ? 0.55f : 0.25f));
    g.fillRoundedRectangle(modalBox.translated(0.0f, 6.0f), 12.0f);

    // Finestra Modale con lo stesso stile MiMOLookAndFeel (Giorno & Notte)
    g.setColour(MiMOLookAndFeel::paperBg);
    g.fillRoundedRectangle(modalBox, 10.0f);
    MiMOLookAndFeel::drawSketchBox(g, modalBox, 680, MiMOLookAndFeel::crayonCyan, 0.14f,
                                   juce::Colours::transparentBlack, 2.2f, false, MiMOLookAndFeel::paperBg);

    // Icona Occhio Vettoriale nell'Header
    {
        const float eyeCx = modalBox.getX() + 32.0f;
        const float eyeCy = modalBox.getY() + 26.0f;
        juce::Path eyePath;
        eyePath.startNewSubPath(eyeCx - 11.0f, eyeCy);
        eyePath.quadraticTo(eyeCx, eyeCy - 7.5f, eyeCx + 11.0f, eyeCy);
        eyePath.quadraticTo(eyeCx, eyeCy + 7.5f, eyeCx - 11.0f, eyeCy);
        eyePath.closeSubPath();
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.strokePath(eyePath, juce::PathStrokeType(1.8f));
        g.setColour(MiMOLookAndFeel::crayonCyan);
        g.fillEllipse(eyeCx - 4.5f, eyeCy - 4.5f, 9.0f, 9.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillEllipse(eyeCx - 2.0f, eyeCy - 2.0f, 4.0f, 4.0f);
    }

    // Header
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("MiMO Eye  -  Analisi Mano MediaPipe su PC & Impostazioni Dettaglio",
               static_cast<int>(modalBox.getX() + 50), static_cast<int>(modalBox.getY() + 14),
               static_cast<int>(modalBox.getWidth() - 95), 22, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText("Usa lo Smartphone come semplice Videocamera remota oppure la Webcam PC. L'analisi 21-Landmark viene eseguita sul PC!",
               static_cast<int>(modalBox.getX() + 20), static_cast<int>(modalBox.getY() + 38),
               static_cast<int>(modalBox.getWidth() - 60), 16, juce::Justification::centredLeft);

    // Pulsante Chiudi [X]
    const auto closeBox = getEyeModalCloseBounds();
    MiMOLookAndFeel::drawSketchBox(g, closeBox, 681, MiMOLookAndFeel::crayonCherry, 0.70f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("X", closeBox.toNearestInt(), juce::Justification::centred);

    // Tabs / Pills Switcher (Sorgente Video)
    for (int t = 0; t < 2; ++t)
    {
        const auto tabBox = getEyeModalTabBounds(t);
        const bool isSel = (eyeDrawerTab == t);
        MiMOLookAndFeel::drawSketchBox(g, tabBox, 682 + t,
                                       isSel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                       isSel ? 0.65f : 0.35f, juce::Colours::transparentBlack,
                                       isSel ? 1.8f : 1.2f);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, isSel ? juce::Font::bold : juce::Font::plain));
        const juce::String tabText = (t == 0)
            ? "[CAM] Selettore Videocamere (PC & Smartphone)"
            : "[QR] Connetti Smartphone (Wi-Fi)";
        g.drawText(tabText, tabBox.toNearestInt(), juce::Justification::centred);
    }

    const float contentY = modalBox.getY() + 100.0f;
    const float contentH = modalBox.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(modalBox.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, modalBox.getRight() - leftPanel.getRight() - 32.0f, contentH);

    // =====================================================================
    // PANNELLO SINISTRO: SORGENTE VIDEO
    // Tab 0 = Selettore Videocamere (Webcam HP, Smartphone, Camo, ecc.)
    // Tab 1 = Connessione Smartphone via QR Code
    // =====================================================================
    MiMOLookAndFeel::drawSketchBox(g, leftPanel, 685, MiMOLookAndFeel::paperCard, 0.85f,
                                   juce::Colours::transparentBlack, 1.5f, false, MiMOLookAndFeel::paperCard);

    if (eyeDrawerTab == 0)
    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Sorgente Video per Analisi Mani:",
                   static_cast<int>(leftPanel.getX() + 14), static_cast<int>(leftPanel.getY() + 10),
                   static_cast<int>(leftPanel.getWidth() - 28), 18, juce::Justification::centredLeft);

        const auto items = getVideoSourceItems();
        const int curDevIdx = cameraDeviceIndex.load();
        const int maxToShow = std::min(5, static_cast<int>(items.size()));

        for (int i = 0; i < maxToShow; ++i)
        {
            const auto optBox = getEyeModalCameraOptionBounds(i);
            const auto& it = items[i];
            const bool isAct = it.isSmartphone
                ? useSmartphoneEye
                : (!useSmartphoneEye && (curDevIdx == it.pcDeviceIdx || (i == 0 && curDevIdx < 0)));

            MiMOLookAndFeel::drawSketchBox(g, optBox, 701 + i,
                                           isAct ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                           isAct ? 0.75f : 0.40f, juce::Colours::transparentBlack,
                                           isAct ? 1.8f : 1.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, isAct ? juce::Font::bold : juce::Font::plain));
            const juce::String badgeStr = isAct ? " [ATTIVA]" : "";
            g.drawText(it.label + badgeStr, optBox.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        }

        // Pulsante Toggle Vista: Video + Scheletro vs Solo Scheletro
        const auto viewToggleBox = getEyeModalViewToggleBounds();
        MiMOLookAndFeel::drawSketchBox(g, viewToggleBox, 705,
                                       eyeShowVideo ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::crayonOchre,
                                       0.55f, juce::Colours::transparentBlack, 1.4f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        const juce::String vtText = eyeShowVideo
            ? "Vista: Video Reale + Scheletro [ON]"
            : "Vista: Solo Scheletro Vettoriale";
        g.drawText(vtText, viewToggleBox.toNearestInt(), juce::Justification::centred);

        juce::Rectangle<float> guideBox(leftPanel.getX() + 14.0f, viewToggleBox.getBottom() + 10.0f, leftPanel.getWidth() - 28.0f, 170.0f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText("Sorgenti Supportate:\n"
                   "- Webcam PC (HP Wide Vision HD): Predefinita nativa a 0 latenza.\n"
                   "- Smartphone Web App: Connessione Wi-Fi dal tab [QR].\n"
                   "- Camo: Usa lo smartphone con cavo USB a 60 FPS.\n"
                   "- Esclusivita assoluta: nessuna interferenza o sfarfallio tra sorgenti!",
                   guideBox.toNearestInt(), juce::Justification::topLeft);
    }
    else
    {
        // TAB 1: SOLO CONNESSIONE SMARTPHONE VIA QR CODE
        const juce::String urlStr = eyeServer ? eyeServer->getEyeUrl() : "http://localhost:8088/eye";
        const bool isHttps = (eyeServer != nullptr && eyeServer->hasHttpsTunnel());
        const bool isPhoneStreaming = (eyeServer != nullptr && eyeServer->isSmartphoneStreaming());
        const bool isClientConnected = (eyeServer != nullptr && eyeServer->isClientConnected());

        const juce::Rectangle<float> qrBox(leftPanel.getCentreX() - 75.0f, leftPanel.getY() + 10.0f, 150.0f, 150.0f);

        if (isHttps)
        {
            drawQrCode(g, urlStr, qrBox);
        }
        else
        {
            // Box di caricamento tunnel protetto HTTPS
            MiMOLookAndFeel::drawSketchBox(g, qrBox, 686, MiMOLookAndFeel::paperBg, 0.85f, juce::Colours::transparentBlack, 1.4f);
            g.setColour(MiMOLookAndFeel::crayonOchre);
            g.fillEllipse(qrBox.getCentreX() - 14.0f, qrBox.getCentreY() - 32.0f, 28.0f, 28.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("SSL", qrBox.getCentreX() - 14.0f, qrBox.getCentreY() - 32.0f, 28.0f, 28.0f, juce::Justification::centred);

            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("Generazione Tunnel HTTPS...",
                       static_cast<int>(qrBox.getX() + 6), static_cast<int>(qrBox.getCentreY() + 4.0f),
                       static_cast<int>(qrBox.getWidth() - 12), 20, juce::Justification::centred);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.0f, juce::Font::plain));
            g.drawText("Attendi ~2s per il link protetto,\nnecessario per la fotocamera su Android",
                       static_cast<int>(qrBox.getX() + 6), static_cast<int>(qrBox.getCentreY() + 24.0f),
                       static_cast<int>(qrBox.getWidth() - 12), 26, juce::Justification::centred);
        }

        const juce::Rectangle<float> urlPill(leftPanel.getX() + 12.0f, qrBox.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, urlPill, 686, isHttps ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg, isHttps ? 0.45f : 0.90f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(urlStr, urlPill.toNearestInt(), juce::Justification::centred);

        const auto openBtn = getEyeModalUrlButtonBounds();
        MiMOLookAndFeel::drawSketchBox(g, openBtn, 687, MiMOLookAndFeel::crayonCyan, 0.55f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("Apri Link nel Browser", openBtn.toNearestInt(), juce::Justification::centred);

        // Pulsante Attiva Fotocamera Smartphone
        const auto actBtn = getEyeModalActivatePhoneButtonBounds();
        MiMOLookAndFeel::drawSketchBox(g, actBtn, 693,
                                       useSmartphoneEye ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonPeach,
                                       0.70f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        const juce::String actText = useSmartphoneEye
            ? "[FOTOCAMERA SMARTPHONE ATTIVA]"
            : "[USA FOTOCAMERA SMARTPHONE]";
        g.drawText(actText, actBtn.toNearestInt(), juce::Justification::centred);

        // Badge Stato Connessione Telefono
        const juce::Rectangle<float> phoneStatusBox(leftPanel.getX() + 12.0f, actBtn.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 22.0f);
        const juce::Colour stCol = isPhoneStreaming ? MiMOLookAndFeel::crayonSage
                                                    : (isClientConnected ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg);
        MiMOLookAndFeel::drawSketchBox(g, phoneStatusBox, 694, stCol, 0.60f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        juce::String phoneStText;
        if (isPhoneStreaming)
            phoneStText = "[CONNESSO] Videocamera streaming attiva";
        else if (isClientConnected)
            phoneStText = "[COLLEGATO] Telefono pronto - tocca Consenti";
        else
            phoneStText = "[IN ATTESA] Inquadra il QR col telefono...";
        g.drawText(phoneStText, phoneStatusBox.toNearestInt(), juce::Justification::centred);

        juce::Rectangle<float> guideBox(leftPanel.getX() + 12.0f, phoneStatusBox.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 85.0f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText("Istruzioni di Connessione:\n"
                   "1. Inquadra il codice QR con lo smartphone.\n"
                   "2. Tocca 'Consenti & Attiva' per autorizzare la fotocamera.\n"
                   "3. Il PC riceve il video ed esegue l'analisi MediaPipe.",
                   guideBox.toNearestInt(), juce::Justification::topLeft);
    }

    // =====================================================================
    // PANNELLO DESTRO: MONITOR VIDEO + IMPOSTAZIONI DETTAGLIO MANO SU PC
    // =====================================================================
    MiMOLookAndFeel::drawSketchBox(g, rightPanel, 688, MiMOLookAndFeel::paperCard, 0.85f,
                                   juce::Colours::transparentBlack, 1.5f, false, MiMOLookAndFeel::paperCard);

    // 1. Riga Stato Connessione e Rilevamento Mano
    const bool isPhoneStream = (eyeServer != nullptr && eyeServer->isSmartphoneStreaming());
    const bool hasHandNow = latestEyeState.hasHand;
    juce::Rectangle<float> statusRow(rightPanel.getX() + 14.0f, rightPanel.getY() + 10.0f, rightPanel.getWidth() - 28.0f, 22.0f);
    MiMOLookAndFeel::drawSketchBox(g, statusRow, 689,
                                   (hasHandNow || isPhoneStream) ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                   (hasHandNow || isPhoneStream) ? 0.55f : 0.70f);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    juce::String handLabel;
    if (latestEyeState.hasHand && latestEyeState.hasSecondaryHand)
        handLabel = "2 Mani (DX + SX)";
    else if (latestEyeState.hasHand)
        handLabel = (latestEyeState.handedness == "Left") ? "Mano Sinistra" : "Mano Destra";
    else
        handLabel = "Nessuna Mano";

    const bool isMp = (eyeServer != nullptr && eyeServer->isMediaPipeActive());
    const juce::String engineTag = isMp ? " [IA MediaPipe]" : " [Sensore]";
    juce::String stStr;
    if (hasHandNow)
        stStr = "[TRACCIAMENTO ATTIVO" + engineTag + "] " + (useSmartphoneEye ? juce::String("Camera Smartphone") : juce::String("HP Wide Vision HD (PC)")) + "  |  " + handLabel + "  |  " + (eyePhalanxDetail == 0 ? "1 Pt (Palmo)" : eyePhalanxDetail == 1 ? "6 Pt (5 Punte)" : "21 Falangi");
    else if (useSmartphoneEye && isPhoneStream)
        stStr = "[CONNESSA] Videocamera Smartphone attiva - Mostra la mano nell'inquadratura...";
    else if (useSmartphoneEye)
        stStr = "[ATTESA STREAM] Fotocamera Smartphone attiva - Tocca 'Consenti' sul telefono...";
    else
        stStr = "[ATTIVA] HP Wide Vision HD (PC) - Mostra la mano nell'inquadratura...";
    g.drawText(stStr, statusRow.toNearestInt(), juce::Justification::centred);

    // 2. Monitor Live sul PC (Mostra il video senza deformazioni + Scheletro calcolato dal PC)
    const juce::Rectangle<float> skelBox(rightPanel.getX() + 14.0f, statusRow.getBottom() + 8.0f, rightPanel.getWidth() - 28.0f, 152.0f);
    {
        juce::Image liveImg;
        {
            juce::ScopedLock sl(cameraLock);
            if (latestWebcamImage.isValid())
                liveImg = latestWebcamImage.createCopy();
        }

        MiMOLookAndFeel::drawSketchBox(g, skelBox, 690, MiMOLookAndFeel::paperBg, 0.90f, juce::Colours::transparentBlack, 1.2f);
        const auto innerSkelBox = skelBox.reduced(2.0f);

        if (eyeShowVideo && liveImg.isValid())
        {
            const float imgW = static_cast<float>(liveImg.getWidth());
            const float imgH = static_cast<float>(liveImg.getHeight());
            const auto videoBox = juce::RectanglePlacement(juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
                .appliedTo(juce::Rectangle<float>(0.0f, 0.0f, imgW, imgH), innerSkelBox);

            g.saveState();
            g.reduceClipRegion(innerSkelBox.toNearestInt());
            g.setColour(juce::Colour(0xff181a20));
            g.fillRect(innerSkelBox);
            g.drawImage(liveImg, videoBox, juce::RectanglePlacement::centred);
            drawHandSkeleton(g, videoBox, latestEyeState, true);
            g.restoreState();
        }
        else
        {
            g.saveState();
            g.reduceClipRegion(innerSkelBox.toNearestInt());
            drawHandSkeleton(g, innerSkelBox, latestEyeState, false);
            g.restoreState();
        }
    }

    // 3. SEZIONE IMPOSTAZIONI DETTAGLIO ANALISI MANO SUL PC
    const float cfgStartY = skelBox.getBottom() + 8.0f;

    // 3A. Mano da Tracciare (Entrambe / Mano Destra / Mano Sinistra)
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("1. MANO DA TRACCIARE:",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(cfgStartY),
               145, 22, juce::Justification::centredLeft);

    static const char* handFilterLabels[3] = { "Entrambe", "Mano Destra", "Mano Sinistra" };
    for (int hf = 0; hf < 3; ++hf)
    {
        const auto b = getEyeModalHandFilterBounds(hf);
        const bool sel = (eyeHandFilter == hf);
        MiMOLookAndFeel::drawSketchBox(g, b, 710 + hf,
                                       sel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg,
                                       sel ? 0.70f : 0.45f, juce::Colours::transparentBlack,
                                       sel ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText(handFilterLabels[hf], b.toNearestInt(), juce::Justification::centred);
    }

    // 3B. Numero di Falangi / Dettaglio Scheletro (1 Pt Palmo / 6 Pt Punte / 21 Falangi)
    const float phalY = cfgStartY + 28.0f;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("2. DETTAGLIO FALANGI:",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(phalY),
               145, 22, juce::Justification::centredLeft);

    static const char* phalanxLabels[3] = { "1 pt: Solo Palmo", "6 pt: 5 Punte Dita", "21 pt: Tutte Falangi" };
    for (int pd = 0; pd < 3; ++pd)
    {
        const auto b = getEyeModalPhalanxBounds(pd);
        const bool sel = (eyePhalanxDetail == pd);
        MiMOLookAndFeel::drawSketchBox(g, b, 715 + pd,
                                       sel ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::paperBg,
                                       sel ? 0.70f : 0.45f, juce::Colours::transparentBlack,
                                       sel ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText(phalanxLabels[pd], b.toNearestInt(), juce::Justification::centred);
    }

    // 3C. Gesture di Interazione (Genera i Pin sul nodo nel Canvas per Strumenti ed Effetti!)
    const float gestTitleY = phalY + 26.0f;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("3. GESTURE D'INTERAZIONE (Uscite Cavi nel Canvas per Strumenti ed Effetti):",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(gestTitleY),
               static_cast<int>(rightPanel.getWidth() - 28), 16, juce::Justification::centredLeft);

    const bool gestStates[4] = {
        eyeGesturePos3D,
        eyeGestureFistTilt && (eyePhalanxDetail >= 1),
        eyeGestureFingerFlex && (eyePhalanxDetail >= 1),
        eyeGesturePinchTrig && (eyePhalanxDetail >= 1)
    };
    static const char* gestLabels[4] = {
        "[X/Y/Z] Posizione Mano 3D (3 Uscite)",
        "[Pugno/Rot] Pugno + Rotazione (2 Uscite)",
        "[Falangi] 4 Dita Indipendenti (4 Uscite)",
        "[Pinch] Tocca Pollice+Dita -> Suona Note"
    };
    const juce::Colour gestCols[4] = {
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonPeach
    };

    for (int gIdx = 0; gIdx < 4; ++gIdx)
    {
        const auto b = getEyeModalGestureBounds(gIdx);
        const bool on = gestStates[gIdx];
        MiMOLookAndFeel::drawSketchBox(g, b, 720 + gIdx,
                                       on ? gestCols[gIdx] : MiMOLookAndFeel::paperBg,
                                       on ? 0.68f : 0.40f, juce::Colours::transparentBlack,
                                       on ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.2f, on ? juce::Font::bold : juce::Font::plain));
        const juce::String prefix = on ? "[ON] " : "[  ] ";
        g.drawText(prefix + gestLabels[gIdx], b.reduced(4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    // 4. Pulsanti Test Rapido Pinch / Trigger Audio in fondo a destra
    const juce::Colour pCols[4] = {
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonPeach
    };
    static const char* fingerLabels[4] = { "Test Indice", "Test Medio", "Test Anulare", "Test Mignolo" };
    for (int i = 0; i < 4; ++i)
    {
        const auto btn = getEyeModalPinchTestBounds(i);
        const bool isAct = latestEyeState.pinchActive[static_cast<size_t>(i)];
        MiMOLookAndFeel::drawSketchBox(g, btn, 695 + i,
                                       isAct ? pCols[i] : MiMOLookAndFeel::paperBg,
                                       isAct ? 0.80f : 0.50f, juce::Colours::transparentBlack,
                                       isAct ? 1.8f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        const int flexPct = juce::roundToInt(latestEyeState.fingerFlex[static_cast<size_t>(i + 1)] * 100.0f);
        g.drawText(juce::String(fingerLabels[i]) + " (" + juce::String(flexPct) + "%)",
                   btn.toNearestInt(), juce::Justification::centred);
    }
}

void MainComponent::drawHandSkeleton(juce::Graphics& g, juce::Rectangle<float> bounds, const mimo::HandTrackingState& state, bool overVideo)
{
    MiMOEyeDrawer::drawHandSkeleton(g, bounds, state, overVideo,
                                    isHandEnabled(false), isHandEnabled(true),
                                    isHandTriggersEnabled(false), isHandTriggersEnabled(true));
}

void MainComponent::drawQrCode(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds)
{
    MiMOEyeDrawer::drawQrCode(g, text, bounds);
}

} // namespace mimo
