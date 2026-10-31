#pragma once

#include "AudioDSP.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace mimo::audio
{

// ============================================================================
// Struttura di Routing per Nodi e Cavi
// ============================================================================
struct NodeAudioRoute
{
    juce::String sourceNodeId;
    juce::String sourcePinId;
    juce::String targetNodeId;
    juce::String targetPinId;
    bool isModulation = false;
};

// ============================================================================
// Motore Audio Modulare di MiMO Hub (C++20 / JUCE)
// ============================================================================
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine()
    {
        masterGain.store(1.20f);
    }

    ~AudioEngine() override
    {
        stopAudio();
    }

    // Inizializzazione e apertura continua dell'uscita audio principale
    bool startAudio(int sampleRate = 48000, int bufferSize = 256)
    {
        if (isAudioRunning)
            return true;

        if (deviceManager == nullptr)
            deviceManager = std::make_unique<juce::AudioDeviceManager>();

        // Inizializza 0 ingressi (il microfono può usare il proprio canale o integrarsi) e 2 uscite stereo
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = sampleRate;
        setup.bufferSize = bufferSize;

        const auto result = deviceManager->initialise(0, 2, nullptr, true);
        if (result.isNotEmpty())
        {
            // Fallback con impostazioni standard WASAPI/DirectSound
            deviceManager->initialiseWithDefaultDevices(0, 2);
        }

        currentSampleRate = deviceManager->getCurrentAudioDevice() != nullptr 
                            ? deviceManager->getCurrentAudioDevice()->getCurrentSampleRate() 
                            : static_cast<double>(sampleRate);

        resetDSP(currentSampleRate);
        deviceManager->addAudioCallback(this);
        isAudioRunning = true;
        return true;
    }

    void stopAudio()
    {
        if (deviceManager != nullptr)
        {
            deviceManager->removeAudioCallback(this);
            deviceManager->closeAudioDevice();
        }
        isAudioRunning = false;
    }

    void resetDSP(double sampleRate)
    {
        currentSampleRate = std::max(8000.0, sampleRate);
        const std::lock_guard<std::mutex> lock(dspMutex);

        drumKit.reset(currentSampleRate);
        synthVoice.reset(currentSampleRate);
        subVoice.reset(currentSampleRate);

        svfFilterL.reset(currentSampleRate);
        svfFilterR.reset(currentSampleRate);
        tapeDelay.reset(currentSampleRate);
        reverb.reset(currentSampleRate);
    }

    // ========================================================================
    // Gestione Nodi e Connessioni (Aggiornati dal Canvas in modo Thread-Safe)
    // ========================================================================
    void syncGraphRoutes(const std::vector<NodeAudioRoute>& routes)
    {
        const std::lock_guard<std::mutex> lock(routeMutex);
        activeRoutes = routes;
    }

    // Trigger manuale o da modulo hardware (Fan-in)
    void triggerInstrument(const juce::String& targetNodeId, int subVoice, float velocity)
    {
        TriggerEvent ev;
        ev.targetNodeId = targetNodeId;
        ev.subVoice = subVoice;
        ev.velocity = juce::jlimit(0.1f, 1.0f, velocity);

        const std::lock_guard<std::mutex> lock(triggerMutex);
        pendingTriggers.push_back(ev);
    }

    // Trigger generico da sorgente hardware (es. "hw-drum", "hw-eye", "hw-mic")
    void triggerFromSource(const juce::String& sourceId, int subVoice, float velocity, bool isSecondary = false)
    {
        std::vector<juce::String> targetInstruments;
        {
            const std::lock_guard<std::mutex> lock(routeMutex);

            // 1. Cerca prima connessioni trigger dirette esplicite (trigger:out / trigger:*pinch* -> trigger:in)
            for (const auto& r : activeRoutes)
            {
                const bool matchSource = (r.sourceNodeId == sourceId ||
                                         (sourceId.startsWith("hw-") && r.sourceNodeId.startsWith("hw-drum")) ||
                                         (sourceId.startsWith("hw-") && r.sourceNodeId.startsWith("hw-mic")));

                bool pinMatches = false;
                if (r.sourcePinId == "trigger:out")
                {
                    pinMatches = true;
                }
                else if (sourceId == "hw-eye")
                {
                    if (!isSecondary)
                    {
                        if (subVoice == 0 && (r.sourcePinId == "trigger:dx_pinch_idx" || r.sourcePinId == "trigger:pinch_idx" || r.sourcePinId == "trigger:out")) pinMatches = true;
                        else if (subVoice == 1 && (r.sourcePinId == "trigger:dx_pinch_mid" || r.sourcePinId == "trigger:pinch_mid")) pinMatches = true;
                        else if (subVoice == 2 && (r.sourcePinId == "trigger:dx_pinch_rng" || r.sourcePinId == "trigger:pinch_rng")) pinMatches = true;
                        else if (subVoice == 3 && (r.sourcePinId == "trigger:dx_pinch_pky" || r.sourcePinId == "trigger:pinch_pky")) pinMatches = true;
                        else if (r.sourcePinId.startsWith("trigger:dx_") || r.sourcePinId == "trigger:out") pinMatches = true;
                    }
                    else
                    {
                        if (subVoice == 0 && (r.sourcePinId == "trigger:sx_pinch_idx" || r.sourcePinId == "trigger:sx_pinch")) pinMatches = true;
                        else if (subVoice == 1 && r.sourcePinId == "trigger:sx_pinch_mid") pinMatches = true;
                        else if (subVoice == 2 && r.sourcePinId == "trigger:sx_pinch_rng") pinMatches = true;
                        else if (subVoice == 3 && r.sourcePinId == "trigger:sx_pinch_pky") pinMatches = true;
                        else if (r.sourcePinId.startsWith("trigger:sx_")) pinMatches = true;
                    }
                    if (r.sourcePinId == "trigger:out") pinMatches = true;
                }
                else if (r.sourcePinId.startsWith("trigger:") || r.targetPinId == "trigger:in")
                {
                    pinMatches = true;
                }

                if (matchSource && pinMatches)
                {
                    if (std::find(targetInstruments.begin(), targetInstruments.end(), r.targetNodeId) == targetInstruments.end())
                        targetInstruments.push_back(r.targetNodeId);
                }
            }

            // 2. Se non ci sono connessioni trigger esplicite, usa il routing convenzionale
            if (targetInstruments.empty())
            {
                for (const auto& r : activeRoutes)
                {
                    const bool matchSource = (r.sourceNodeId == sourceId ||
                                             (sourceId.startsWith("hw-") && r.sourceNodeId.startsWith("hw-drum")) ||
                                             (sourceId.startsWith("hw-") && r.sourceNodeId.startsWith("hw-mic")));

                    if (matchSource)
                    {
                        // Se la destinazione è la Base Centrale ("rockpi"), propaga ai nodi collegati alla Base
                        if (r.targetNodeId == "rockpi")
                        {
                            for (const auto& r2 : activeRoutes)
                                if (r2.sourceNodeId == "rockpi" && !r2.isModulation)
                                    targetInstruments.push_back(r2.targetNodeId);
                        }
                        else if (!r.isModulation)
                        {
                            targetInstruments.push_back(r.targetNodeId);
                        }
                    }
                }
            }
        }

        // Se non ci sono cavi diretti ma c'è un'assegnazione globale, attiva di default il drum/synth
        if (targetInstruments.empty())
        {
            triggerInstrument("default-instrument", subVoice, velocity);
        }
        else
        {
            for (const auto& target : targetInstruments)
                triggerInstrument(target, subVoice, velocity);
        }
    }

    // Modulazione continua di parametri (CV / X-Y-Z Occhio)
    void setModulation(const juce::String& paramKey, float valueNorm)
    {
        if (paramKey == "cutoff")
        {
            // Scala esponenziale 40 Hz .. 16000 Hz
            const float hz = 40.0f * std::pow(400.0f, juce::jlimit(0.0f, 1.0f, valueNorm));
            filterCutoffHz.store(hz);
        }
        else if (paramKey == "res" || paramKey == "resonance")
        {
            filterReso.store(juce::jlimit(0.2f, 12.0f, 0.2f + valueNorm * 8.0f));
        }
        else if (paramKey == "delay_time")
        {
            delayTimeMs.store(juce::jlimit(20.0f, 1200.0f, 20.0f + valueNorm * 900.0f));
        }
        else if (paramKey == "delay_feedback")
        {
            delayFeedback.store(juce::jlimit(0.0f, 90.0f, valueNorm * 85.0f));
        }
        else if (paramKey == "reverb_room")
        {
            reverbRoomSize.store(juce::jlimit(0.0f, 100.0f, valueNorm * 100.0f));
        }
        else if (paramKey == "reverb_mix")
        {
            reverbWetMix.store(juce::jlimit(0.0f, 100.0f, valueNorm * 100.0f));
        }
    }

    void setModulation(const juce::String& /*nodeId*/, const juce::String& paramKey, float valueNorm)
    {
        setModulation(paramKey, valueNorm);
    }

    void setMasterVolume(float gain)
    {
        masterGain.store(juce::jlimit(0.0f, 2.0f, gain));
    }

    float getMasterPeak() const noexcept
    {
        return masterPeak.load();
    }

    bool isRunning() const noexcept { return isAudioRunning; }

    // ========================================================================
    // AudioIODeviceCallback Implementation (Real-Time Audio Thread)
    // ========================================================================
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        if (device != nullptr)
        {
            currentSampleRate = device->getCurrentSampleRate();
            resetDSP(currentSampleRate);
        }
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                          int /*numInputChannels*/,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& /*context*/) override
    {
        if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
            return;

        float* outL = outputChannelData[0];
        float* outR = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

        if (outL != nullptr)
            juce::FloatVectorOperations::clear(outL, numSamples);
        if (outR != nullptr)
            juce::FloatVectorOperations::clear(outR, numSamples);

        // 1. Processa eventi di trigger in coda
        std::vector<TriggerEvent> triggersToProcess;
        {
            const std::lock_guard<std::mutex> lock(triggerMutex);
            triggersToProcess.swap(pendingTriggers);
        }

        {
            const std::lock_guard<std::mutex> lock(dspMutex);

            for (const auto& ev : triggersToProcess)
            {
                // Identifica il tipo di strumento dal nome nodo o voce
                if (ev.targetNodeId.containsIgnoreCase("felt") || ev.targetNodeId.containsIgnoreCase("piano") || ev.targetNodeId.containsIgnoreCase("synth"))
                {
                    // Scala melodica pentatonica (C3, D3, E3, G3, A3, C4)
                    const float freqs[] = { 130.81f, 146.83f, 164.81f, 196.00f, 220.0f, 261.63f, 293.66f, 329.63f };
                    const float f = freqs[juce::jlimit(0, 7, ev.subVoice % 8)];
                    synthVoice.triggerNote(f, ev.velocity, false);
                }
                else if (ev.targetNodeId.containsIgnoreCase("808") || ev.targetNodeId.containsIgnoreCase("sub"))
                {
                    subVoice.triggerNote(48.0f, ev.velocity, true);
                }
                else
                {
                    // Default: Drum Kit Analogico (Cassa, Rullante, Hat, Percussione)
                    drumKit.trigger(ev.subVoice % 4, ev.velocity);
                }
            }

            // 2. Genera campioni da strumenti attivi
            drumKit.renderNextBlock(outL, outR, numSamples, 1.0f);

            synthVoice.setMacroControls(filterCutoffHz.load(), filterReso.load(), 0.35f);
            synthVoice.renderNextBlock(outL, outR, numSamples, 0.95f);

            subVoice.setMacroControls(400.0f, 1.0f, 0.1f);
            subVoice.renderNextBlock(outL, outR, numSamples, 1.05f);

            // 3. Processa catena effetti (SVF Filter ZDF -> Tape Delay -> Lush Reverb)
            const float cut = filterCutoffHz.load();
            const float res = filterReso.load();
            svfFilterL.setParameters(cut, res, dsp::SVF_ZDF::Mode::LowPass);
            svfFilterR.setParameters(cut, res, dsp::SVF_ZDF::Mode::LowPass);

            for (int i = 0; i < numSamples; ++i)
            {
                if (outL != nullptr) outL[i] = svfFilterL.process(outL[i]);
                if (outR != nullptr) outR[i] = svfFilterR.process(outR[i]);
            }

            tapeDelay.setParameters(delayTimeMs.load(), delayFeedback.load(), 3800.0f, delayWetMix.load());
            tapeDelay.processBlock(outL, outR, numSamples);

            reverb.setParameters(reverbRoomSize.load(), 35.0f, reverbWetMix.load());
            reverb.processBlock(outL, outR, numSamples);
        }

        // 4. Guadagno Master & Soft Limiter Anti-Clipping (Udo Zölzer)
        const float mg = masterGain.load();
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (outL != nullptr)
            {
                outL[i] = dsp::softClipTanh(outL[i] * mg);
                peak = std::max(peak, std::abs(outL[i]));
            }
            if (outR != nullptr)
            {
                outR[i] = dsp::softClipTanh(outR[i] * mg);
                peak = std::max(peak, std::abs(outR[i]));
            }
        }

        masterPeak.store(peak);
    }

private:
    struct TriggerEvent
    {
        juce::String targetNodeId;
        int subVoice = 0;
        float velocity = 0.8f;
    };

    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    bool isAudioRunning = false;
    double currentSampleRate = 48000.0;

    std::mutex dspMutex;
    dsp::AnalogDrumKit drumKit;
    dsp::MelodicSynthVoice synthVoice;
    dsp::MelodicSynthVoice subVoice;

    dsp::SVF_ZDF svfFilterL;
    dsp::SVF_ZDF svfFilterR;
    dsp::StereoTapeDelay tapeDelay;
    dsp::LushReverb reverb;

    std::mutex triggerMutex;
    std::vector<TriggerEvent> pendingTriggers;

    std::mutex routeMutex;
    std::vector<NodeAudioRoute> activeRoutes;

    // Parametri Atomici Macro
    std::atomic<float> masterGain { 1.20f };
    std::atomic<float> masterPeak { 0.0f };

    std::atomic<float> filterCutoffHz { 4500.0f };
    std::atomic<float> filterReso { 1.2f };

    std::atomic<float> delayTimeMs { 320.0f };
    std::atomic<float> delayFeedback { 35.0f };
    std::atomic<float> delayWetMix { 25.0f };

    std::atomic<float> reverbRoomSize { 65.0f };
    std::atomic<float> reverbWetMix { 25.0f };
};

} // namespace mimo::audio
