#pragma once

#include "AudioDSP.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <algorithm>

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
// Descrittore di un Nodo nel Grafo Audio (disaccoppiato dalla GUI)
// ============================================================================
struct NodeGraphDescriptor
{
    juce::String id;
    juce::String label;
    juce::String category;
    juce::String engine;
    bool isEffect = false;
    bool isInstrument = false;
    bool isUtility = false;
    bool isBypassed = false;
};

// ============================================================================
// Motore Audio Modulare di MiMO Hub (C++20 / JUCE)
// Architettura V2: Grafo Dinamico di Processori DSP con Routing Reale
// ============================================================================
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    struct GraphNode
    {
        juce::String id;
        juce::String label;
        juce::String category;
        juce::String engine;
        bool isInstrument = false;
        bool isEffect = false;
        bool isUtility = false;

        std::unique_ptr<NodeAudioProcessor> processor;
        std::atomic<bool> isBypassed { false };
        std::atomic<float> outputGain { 1.0f };
        std::atomic<float> dryWet { 1.0f };
        std::atomic<float> realtimePeak { 0.0f };

        // Buffer audio pre-allocati per rendering a 0 allocazioni real-time
        std::vector<float> bufferL;
        std::vector<float> bufferR;
    };

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

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = sampleRate;
        setup.bufferSize = bufferSize;

        const auto result = deviceManager->initialise(0, 2, nullptr, true);
        if (result.isNotEmpty())
        {
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
        const std::lock_guard<std::mutex> lock(graphMutex);

        // Reset processori standalone di riserva
        drumKit.reset(currentSampleRate);
        synthVoice.reset(currentSampleRate);
        subVoice.reset(currentSampleRate);

        svfFilterL.reset(currentSampleRate);
        svfFilterR.reset(currentSampleRate);
        tapeDelay.reset(currentSampleRate);
        reverb.reset(currentSampleRate);

        // Reset tutti i nodi nel grafo dinamico
        for (auto& node : graphNodes)
        {
            if (node->processor != nullptr)
                node->processor->reset(currentSampleRate);
            node->bufferL.assign(2048, 0.0f);
            node->bufferR.assign(2048, 0.0f);
        }
    }

    // ========================================================================
    // Sincronizzazione Dinamica del Grafo Audio (Thread-Safe)
    // ========================================================================
    void syncGraph(const std::vector<NodeGraphDescriptor>& descriptors, const std::vector<NodeAudioRoute>& routes)
    {
        std::vector<std::shared_ptr<GraphNode>> newNodes;

        for (const auto& d : descriptors)
        {
            // Cerca se esiste già un nodo attivo con lo stesso ID
            std::shared_ptr<GraphNode> existingNode;
            {
                const std::lock_guard<std::mutex> lock(graphMutex);
                for (const auto& n : graphNodes)
                {
                    if (n->id == d.id)
                    {
                        existingNode = n;
                        break;
                    }
                }
            }

            if (existingNode != nullptr)
            {
                // Riutilizza il nodo esistente per preservare lo stato dei buffer/code
                existingNode->isInstrument = d.isInstrument;
                existingNode->isEffect = d.isEffect;
                existingNode->isUtility = d.isUtility;
                existingNode->isBypassed.store(d.isBypassed);
                if (existingNode->processor != nullptr)
                    existingNode->processor->setBypass(d.isBypassed);
                newNodes.push_back(existingNode);
            }
            else
            {
                // Crea nuovo nodo con il relativo processore DSP
                auto node = std::make_shared<GraphNode>();
                node->id = d.id;
                node->label = d.label;
                node->category = d.category;
                node->engine = d.engine;
                node->isInstrument = d.isInstrument;
                node->isEffect = d.isEffect;
                node->isUtility = d.isUtility;
                node->isBypassed.store(d.isBypassed);

                node->processor = createProcessorForNode(d.id, d.label, d.category, d.engine);
                if (node->processor != nullptr)
                {
                    node->processor->reset(currentSampleRate);
                    node->processor->setBypass(d.isBypassed);
                }
                node->bufferL.assign(2048, 0.0f);
                node->bufferR.assign(2048, 0.0f);

                newNodes.push_back(node);
            }
        }

        const std::lock_guard<std::mutex> lock(graphMutex);
        graphNodes = std::move(newNodes);
        activeRoutes = routes;
    }

    void syncGraphRoutes(const std::vector<NodeAudioRoute>& routes)
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        activeRoutes = routes;
    }

    // ========================================================================
    // Controlli Standard V2 per Nodo: Volume, Dry/Wet, Bypass, VU-Meter
    // ========================================================================
    void setNodeVolume(const juce::String& nodeId, float gainLinear)
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (auto& node : graphNodes)
        {
            if (node->id == nodeId)
            {
                node->outputGain.store(gainLinear);
                if (node->processor != nullptr)
                    node->processor->setOutputGain(gainLinear);
                break;
            }
        }
    }

    void setNodeDryWet(const juce::String& nodeId, float mixNormalized)
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (auto& node : graphNodes)
        {
            if (node->id == nodeId)
            {
                node->dryWet.store(mixNormalized);
                if (node->processor != nullptr)
                    node->processor->setDryWet(mixNormalized);
                break;
            }
        }
    }

    void setNodeBypass(const juce::String& nodeId, bool isBypassed)
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (auto& node : graphNodes)
        {
            if (node->id == nodeId)
            {
                node->isBypassed.store(isBypassed);
                if (node->processor != nullptr)
                    node->processor->setBypass(isBypassed);
                break;
            }
        }
    }

    bool getNodeBypass(const juce::String& nodeId) const
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (const auto& node : graphNodes)
        {
            if (node->id == nodeId)
                return node->isBypassed.load();
        }
        return false;
    }

    float getNodePeak(const juce::String& nodeId) const
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (const auto& node : graphNodes)
        {
            if (node->id == nodeId)
                return node->realtimePeak.load();
        }
        return 0.0f;
    }

    float getNodeModulationValue(const juce::String& nodeId, const juce::String& pinId = "mod:out") const
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (const auto& node : graphNodes)
        {
            if (node->id == nodeId)
            {
                if (node->processor != nullptr)
                    return node->processor->getModulationValue(pinId);
                return -1.0f;
            }
        }
        return -1.0f;
    }

    // ========================================================================
    // Trigger ed Eventi
    // ========================================================================
    void triggerInstrument(const juce::String& targetNodeId, int subVoice, float velocity)
    {
        TriggerEvent ev;
        ev.targetNodeId = targetNodeId;
        ev.subVoice = subVoice;
        ev.velocity = juce::jlimit(0.1f, 1.0f, velocity);

        const std::lock_guard<std::mutex> lock(triggerMutex);
        pendingTriggers.push_back(ev);
    }

    struct TargetAction
    {
        juce::String nodeId;
        int voiceIdx = 0;
    };

    void triggerFromSource(const juce::String& sourceId, int subVoice, float velocity, bool isSecondary = false)
    {
        std::vector<TargetAction> targets;
        {
            const std::lock_guard<std::mutex> lock(graphMutex);

            // 1. Cerca connessioni trigger dirette verso nodi o prese specifiche di voce
            for (const auto& r : activeRoutes)
            {
                const bool matchSource = (r.sourceNodeId == sourceId ||
                                         (sourceId == "hw-eye" && r.sourceNodeId == "hw-webcam") ||
                                         (sourceId == "hw-webcam" && r.sourceNodeId == "hw-eye"));

                bool pinMatches = false;
                int specificVoice = subVoice;

                if (r.targetPinId == "trigger:kick")
                {
                    specificVoice = 0;
                    pinMatches = true;
                }
                else if (r.targetPinId == "trigger:snare")
                {
                    specificVoice = 1;
                    pinMatches = true;
                }
                else if (r.targetPinId == "trigger:hat")
                {
                    specificVoice = 2;
                    pinMatches = true;
                }
                else if (r.targetPinId == "trigger:tom")
                {
                    specificVoice = 3;
                    pinMatches = true;
                }
                else if (r.sourcePinId == "trigger:out")
                {
                    pinMatches = true;
                }
                else if (sourceId == "hw-eye" || sourceId == "hw-webcam")
                {
                    if (!isSecondary)
                    {
                        if (subVoice == 0 && (r.sourcePinId == "trigger:dx_pinch_idx" || r.sourcePinId == "trigger:pinch_idx")) pinMatches = true;
                        else if (subVoice == 1 && (r.sourcePinId == "trigger:dx_pinch_mid" || r.sourcePinId == "trigger:pinch_mid")) pinMatches = true;
                        else if (subVoice == 2 && (r.sourcePinId == "trigger:dx_pinch_rng" || r.sourcePinId == "trigger:pinch_rng")) pinMatches = true;
                        else if (subVoice == 3 && (r.sourcePinId == "trigger:dx_pinch_pky" || r.sourcePinId == "trigger:pinch_pky")) pinMatches = true;
                    }
                    else
                    {
                        if (subVoice == 0 && (r.sourcePinId == "trigger:sx_pinch_idx" || r.sourcePinId == "trigger:sx_pinch")) pinMatches = true;
                        else if (subVoice == 1 && r.sourcePinId == "trigger:sx_pinch_mid") pinMatches = true;
                        else if (subVoice == 2 && r.sourcePinId == "trigger:sx_pinch_rng") pinMatches = true;
                        else if (subVoice == 3 && r.sourcePinId == "trigger:sx_pinch_pky") pinMatches = true;
                    }
                }
                else if (r.sourcePinId.startsWith("trigger:") || r.targetPinId == "trigger:in")
                {
                    pinMatches = true;
                }

                if (matchSource && pinMatches)
                {
                    targets.push_back({ r.targetNodeId, specificVoice });
                }
            }

            // 2. Connessioni convenzionali da base rockpi o cavo audio diretto
            if (targets.empty())
            {
                for (const auto& r : activeRoutes)
                {
                    const bool matchSource = (r.sourceNodeId == sourceId ||
                                             (sourceId == "hw-eye" && r.sourceNodeId == "hw-webcam") ||
                                             (sourceId == "hw-webcam" && r.sourceNodeId == "hw-eye"));

                    if (matchSource)
                    {
                        if (r.targetNodeId == "rockpi")
                        {
                            for (const auto& r2 : activeRoutes)
                                if (r2.sourceNodeId == "rockpi" && !r2.isModulation)
                                    targets.push_back({ r2.targetNodeId, subVoice });
                        }
                        else if (!r.isModulation)
                        {
                            targets.push_back({ r.targetNodeId, subVoice });
                        }
                    }
                }
            }
        }

        // Se non ci sono cavi espliciti verso strumenti
        if (targets.empty())
        {
            if (sourceId == "hw-eye" || sourceId == "hw-webcam")
            {
                const int globalIdx = isSecondary ? (subVoice + 4) : subVoice;
                const int sndId = getEyeFingerSound(globalIdx);
                triggerSoundById(sndId, velocity);
            }
            else
            {
                triggerInstrument("default-instrument", subVoice, velocity);
            }
        }
        else
        {
            for (const auto& target : targets)
                triggerInstrument(target.nodeId, target.voiceIdx, velocity);
        }
    }

    // ========================================================================
    // Modulazione Parametri (CV / X-Y-Z Occhio)
    // ========================================================================
    void setModulation(const juce::String& nodeId, const juce::String& paramKey, float valueNorm)
    {
        const float v = juce::jlimit(0.0f, 1.0f, valueNorm);

        const std::lock_guard<std::mutex> lock(graphMutex);
        for (auto& node : graphNodes)
        {
            if (node->id == nodeId)
            {
                if (paramKey == "vol" || paramKey == "volume")
                {
                    node->outputGain.store(v * 1.5f);
                    if (node->processor != nullptr)
                        node->processor->setOutputGain(v * 1.5f);
                }
                else if (paramKey == "mix" || paramKey == "drywet")
                {
                    node->dryWet.store(v);
                    if (node->processor != nullptr)
                        node->processor->setDryWet(v);
                }

                if (node->processor != nullptr)
                    node->processor->setParameter(paramKey, v);

                // Sincronizza anche le voci integrate dedicate se il nodo corrisponde a percussioni o synth
                const juce::String fullTag = (node->id + " " + node->label + " " + node->category).toLowerCase();
                if (fullTag.contains("drum") || fullTag.contains("percuss") || fullTag.contains("kit"))
                {
                    drumKit.setParameter(paramKey, v);
                    if (paramKey == "vol" || paramKey == "volume")
                        drumKit.setOutputGain(v * 1.5f);
                }
                else if (fullTag.contains("synth") || fullTag.contains("va") || fullTag.contains("lead"))
                {
                    synthVoice.setParameter(paramKey, v);
                }
                return;
            }
        }

        // Se il nodo non è presente in graphNodes ma il suo ID richiama percussioni o synth
        const juce::String targetLower = nodeId.toLowerCase();
        if (targetLower.contains("drum") || targetLower.contains("percuss") || targetLower == "hw-drum")
        {
            drumKit.setParameter(paramKey, v);
            if (paramKey == "vol" || paramKey == "volume")
                drumKit.setOutputGain(v * 1.5f);
        }
        else if (targetLower.contains("synth"))
        {
            synthVoice.setParameter(paramKey, v);
        }

        // Fallback su parametri globali
        setModulation(paramKey, v);
    }

    void setModulation(const juce::String& paramKey, float valueNorm)
    {
        const float v = juce::jlimit(0.0f, 1.0f, valueNorm);
        if (paramKey == "cutoff")
        {
            const float hz = 40.0f * std::pow(400.0f, v);
            filterCutoffHz.store(hz);
        }
        else if (paramKey == "res" || paramKey == "resonance")
        {
            filterReso.store(juce::jlimit(0.2f, 12.0f, 0.2f + v * 8.0f));
        }
        else if (paramKey == "delay_time" || paramKey == "time")
        {
            delayTimeMs.store(juce::jlimit(20.0f, 1200.0f, 20.0f + v * 900.0f));
        }
        else if (paramKey == "delay_feedback" || paramKey == "fb")
        {
            delayFeedback.store(juce::jlimit(0.0f, 90.0f, v * 85.0f));
        }
        else if (paramKey == "reverb_room" || paramKey == "room")
        {
            reverbRoomSize.store(juce::jlimit(0.0f, 100.0f, v * 100.0f));
        }
        else if (paramKey == "reverb_mix" || paramKey == "mix")
        {
            reverbWetMix.store(juce::jlimit(0.0f, 100.0f, v * 100.0f));
        }
    }

    // Trasmissione continua delle coordinate gestuali 3D (Theremin & Sensoristica)
    void setHand3D(float x, float y, float z, bool isPresent)
    {
        const std::lock_guard<std::mutex> lock(graphMutex);
        for (auto& node : graphNodes)
        {
            if (auto* theremin = dynamic_cast<dsp::Theremin3DVoice*>(node->processor.get()))
            {
                theremin->setHand3D(x, y, z, isPresent);
            }
        }
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
    // AudioIODeviceCallback Implementation (Real-Time Audio Thread - 0 Allocazioni)
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

        const int safeSamples = std::min(numSamples, 2048);

        // 1. Processa eventi di trigger in coda
        std::vector<TriggerEvent> triggersToProcess;
        {
            const std::lock_guard<std::mutex> lock(triggerMutex);
            triggersToProcess.swap(pendingTriggers);
        }

        const std::lock_guard<std::mutex> lock(graphMutex);

        for (const auto& ev : triggersToProcess)
        {
            bool triggered = false;
            for (auto& node : graphNodes)
            {
                if (node->id == ev.targetNodeId && node->processor != nullptr)
                {
                    node->processor->trigger(ev.subVoice, ev.velocity);
                    triggered = true;
                    break;
                }
            }

            // Fallback se il nodo target non è trovato o è generico
            if (!triggered)
            {
                for (auto& node : graphNodes)
                {
                    if (node->isInstrument && node->processor != nullptr)
                    {
                        node->processor->trigger(ev.subVoice, ev.velocity);
                        triggered = true;
                        break;
                    }
                }
                if (!triggered)
                {
                    triggerSoundById(ev.subVoice, ev.velocity);
                }
            }
        }

        // 2. Rendering degli Strumenti (Generazione suono indipendente per ciascun nodo)
        for (auto& node : graphNodes)
        {
            if (node->isInstrument && node->processor != nullptr)
            {
                juce::FloatVectorOperations::clear(node->bufferL.data(), safeSamples);
                juce::FloatVectorOperations::clear(node->bufferR.data(), safeSamples);

                node->processor->processNode(node->bufferL.data(), node->bufferR.data(), safeSamples, false);
                node->realtimePeak.store(node->processor->getPeakLevel());
            }
        }

        // 2b. Rendering dei Nodi di Utilità & Modulatori CV (Multi-Wave LFO, Envelope Follower, Looper)
        for (auto& node : graphNodes)
        {
            if (node->isUtility && node->processor != nullptr)
            {
                juce::FloatVectorOperations::clear(node->bufferL.data(), safeSamples);
                juce::FloatVectorOperations::clear(node->bufferR.data(), safeSamples);

                // Somma eventuali ingressi audio (es. Envelope Follower o Looper)
                for (const auto& r : activeRoutes)
                {
                    if (r.targetNodeId == node->id && (r.targetPinId == "audio:in" || !r.isModulation))
                    {
                        for (auto& src : graphNodes)
                        {
                            if (src->id == r.sourceNodeId)
                            {
                                juce::FloatVectorOperations::add(node->bufferL.data(), src->bufferL.data(), safeSamples);
                                juce::FloatVectorOperations::add(node->bufferR.data(), src->bufferR.data(), safeSamples);
                            }
                        }
                    }
                }

                node->processor->processNode(node->bufferL.data(), node->bufferR.data(), safeSamples, true);
                node->realtimePeak.store(node->processor->getPeakLevel());

                // Controllo Trigger di fine ciclo LFO (Clock Trigger Out)
                if (auto* lfo = dynamic_cast<dsp::MultiWaveLFONode*>(node->processor.get()))
                {
                    if (lfo->pullCycleTrigger())
                    {
                        for (const auto& r : activeRoutes)
                        {
                            if (r.sourceNodeId == node->id && (r.sourcePinId == "trigger:out" || r.sourcePinId.startsWith("trigger:")))
                            {
                                int voiceIdx = 0;
                                if (r.targetPinId == "trigger:snare") voiceIdx = 1;
                                else if (r.targetPinId == "trigger:hat") voiceIdx = 2;
                                else if (r.targetPinId == "trigger:tom") voiceIdx = 3;

                                for (auto& target : graphNodes)
                                {
                                    if (target->id == r.targetNodeId && target->processor != nullptr)
                                    {
                                        target->processor->trigger(voiceIdx, 0.9f);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 3. Elaborazione della Catena di Effetti lungo i cavi del Canvas
        for (auto& node : graphNodes)
        {
            if (node->isEffect && node->processor != nullptr)
            {
                juce::FloatVectorOperations::clear(node->bufferL.data(), safeSamples);
                juce::FloatVectorOperations::clear(node->bufferR.data(), safeSamples);

                // Somma tutti i cavi in ingresso che puntano a questo effetto
                for (const auto& r : activeRoutes)
                {
                    if (r.targetNodeId == node->id && (r.targetPinId == "audio:in" || !r.isModulation))
                    {
                        for (auto& src : graphNodes)
                        {
                            if (src->id == r.sourceNodeId)
                            {
                                juce::FloatVectorOperations::add(node->bufferL.data(), src->bufferL.data(), safeSamples);
                                juce::FloatVectorOperations::add(node->bufferR.data(), src->bufferR.data(), safeSamples);
                            }
                        }
                    }
                }

                // Processa con gestione Dry/Wet, Gain compensato, e Bypass pulito
                node->processor->processNode(node->bufferL.data(), node->bufferR.data(), safeSamples, true);
                node->realtimePeak.store(node->processor->getPeakLevel());
            }
        }

        // 4. Somma al Master Bus
        // Un nodo fluisce al master se è collegato a "master-out" / "rockpi",
        // oppure se è uno strumento/effetto senza cavi audio a valle (per evitare nodi muti).
        int nodesSummedToMaster = 0;
        for (auto& node : graphNodes)
        {
            if (!node->isInstrument && !node->isEffect && !node->isUtility)
                continue;

            // Se è un LFO, è puramente CV / Trigger e non fluisce sul master audio
            if (node->isUtility && dynamic_cast<dsp::MultiWaveLFONode*>(node->processor.get()) != nullptr)
                continue;

            bool hasRouteToMaster = false;
            bool hasRouteToOtherEffect = false;

            for (const auto& r : activeRoutes)
            {
                if (r.sourceNodeId == node->id && !r.isModulation)
                {
                    if (r.targetNodeId == "master-out" || r.targetNodeId == "rockpi")
                    {
                        hasRouteToMaster = true;
                    }
                    else
                    {
                        for (const auto& other : graphNodes)
                        {
                            if (other->id == r.targetNodeId && other->isEffect)
                            {
                                hasRouteToOtherEffect = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (hasRouteToMaster || !hasRouteToOtherEffect)
            {
                if (outL != nullptr)
                    juce::FloatVectorOperations::add(outL, node->bufferL.data(), safeSamples);
                if (outR != nullptr)
                    juce::FloatVectorOperations::add(outR, node->bufferR.data(), safeSamples);
                nodesSummedToMaster++;
            }
        }

        // Fallback di riserva se il canvas non ha strumenti attivi
        if (graphNodes.empty() || nodesSummedToMaster == 0)
        {
            drumKit.renderNextBlock(outL, outR, safeSamples, 1.0f);
            synthVoice.setMacroControls(filterCutoffHz.load(), filterReso.load(), 0.35f);
            synthVoice.renderNextBlock(outL, outR, safeSamples, 0.95f);
        }

        // 5. Guadagno Master & Soft Limiter Anti-Clipping Tanh (Udo Zölzer)
        const float mg = masterGain.load();
        float peak = 0.0f;

        for (int i = 0; i < safeSamples; ++i)
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

    // ========================================================================
    // Funzioni Ausiliarie Retrocompatibili (Modulo Occhio e Suoni Default)
    // ========================================================================
    void setEyeFingerSound(int globalIndex, int soundId)
    {
        switch (globalIndex)
        {
            case 0: eyeFingerSound0.store(soundId); break;
            case 1: eyeFingerSound1.store(soundId); break;
            case 2: eyeFingerSound2.store(soundId); break;
            case 3: eyeFingerSound3.store(soundId); break;
            case 4: eyeFingerSound4.store(soundId); break;
            case 5: eyeFingerSound5.store(soundId); break;
            case 6: eyeFingerSound6.store(soundId); break;
            case 7: eyeFingerSound7.store(soundId); break;
            default: break;
        }
    }

    int getEyeFingerSound(int globalIndex) const
    {
        switch (globalIndex)
        {
            case 0: return eyeFingerSound0.load();
            case 1: return eyeFingerSound1.load();
            case 2: return eyeFingerSound2.load();
            case 3: return eyeFingerSound3.load();
            case 4: return eyeFingerSound4.load();
            case 5: return eyeFingerSound5.load();
            case 6: return eyeFingerSound6.load();
            case 7: return eyeFingerSound7.load();
            default: return 0;
        }
    }

    void triggerSoundById(int soundId, float velocity)
    {
        const float v = juce::jlimit(0.15f, 1.0f, velocity);
        switch (soundId)
        {
            case 0: drumKit.trigger(0, v); break;
            case 1: drumKit.trigger(1, v); break;
            case 2: drumKit.trigger(2, v); break;
            case 3: drumKit.trigger(3, v); break;
            case 4: synthVoice.triggerNote(130.81f, v, false); break;
            case 5: synthVoice.triggerNote(146.83f, v, false); break;
            case 6: synthVoice.triggerNote(164.81f, v, false); break;
            case 7: synthVoice.triggerNote(196.00f, v, false); break;
            case 8: synthVoice.triggerNote(220.00f, v, false); break;
            case 9: synthVoice.triggerNote(261.63f, v, false); break;
            case 10: synthVoice.triggerNote(261.63f, v, false); break;
            case 11: synthVoice.triggerNote(329.63f, v, false); break;
            case 12: subVoice.triggerNote(48.0f, v, true); break;
            case 13: drumKit.trigger(1, v); break;
            case 14: synthVoice.triggerNote(523.25f, v, false); break;
            default: drumKit.trigger(soundId % 4, v); break;
        }
    }

private:
    static std::unique_ptr<NodeAudioProcessor> createProcessorForNode(
        const juce::String& id, const juce::String& label, const juce::String& category, const juce::String& engine)
    {
        const juce::String full = (id + " " + label + " " + category + " " + engine).toLowerCase();

        // Strumenti
        if (full.contains("theremin"))
            return std::make_unique<dsp::Theremin3DVoice>();
        if (full.contains("karplus") || full.contains("pluck") || full.contains("kalimba") || full.contains("box") || full.contains("carillon"))
            return std::make_unique<dsp::KarplusPluckVoice>();
        if (full.contains("felt") || full.contains("piano"))
        {
            auto piano = std::make_unique<dsp::MelodicSynthVoice>();
            piano->setIsPianoFelt(true);
            return piano;
        }
        if (full.contains("drum") || full.contains("kit") || full.contains("toy") || full.contains("percuss") || full.contains("cassa"))
            return std::make_unique<dsp::AnalogDrumKit>();
        if (full.contains("synth") || full.contains("va") || full.contains("lead") || full.contains("bass") || full.contains("chalk"))
            return std::make_unique<dsp::MelodicSynthVoice>();

        // Effetti
        if (full.contains("saturaz") || full.contains("drive") || full.contains("soft") || full.contains("overdrive"))
            return std::make_unique<dsp::SoftSaturatorNode>();
        if (full.contains("moog") || full.contains("ladder"))
            return std::make_unique<dsp::MoogLadderNode>();
        if (full.contains("chorus") || full.contains("bbd") || full.contains("ensemble"))
            return std::make_unique<dsp::BBDChorusNode>();
        if (full.contains("filtro") || full.contains("filter") || full.contains("svf") || full.contains("naif"))
            return std::make_unique<dsp::SVFFilterNode>();
        if (full.contains("eco") || full.contains("delay") || full.contains("nastro"))
            return std::make_unique<dsp::StereoTapeDelay>();
        if (full.contains("riverbero") || full.contains("reverb") || full.contains("fdn") || full.contains("cattedrale"))
            return std::make_unique<dsp::LushReverb>();

        // Utility
        if (full.contains("lfo") || full.contains("mod_lfo"))
            return std::make_unique<dsp::MultiWaveLFONode>();
        if (full.contains("scale") || full.contains("quantiz"))
            return std::make_unique<dsp::ScaleLock>();
        if (full.contains("follower") || full.contains("env"))
            return std::make_unique<dsp::EnvelopeFollower>();
        if (full.contains("looper") || full.contains("tape_loop"))
            return std::make_unique<dsp::TapeLooperProcessor>();

        // Fallback in base alla categoria
        if (category.containsIgnoreCase("Effett") || id.startsWithIgnoreCase("fx-"))
            return std::make_unique<dsp::SVFFilterNode>();

        return std::make_unique<dsp::AnalogDrumKit>();
    }

    struct TriggerEvent
    {
        juce::String targetNodeId;
        int subVoice = 0;
        float velocity = 0.8f;
    };

    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    bool isAudioRunning = false;
    double currentSampleRate = 48000.0;

    // Grafo Dinamico di Nodi Audio
    mutable std::mutex graphMutex;
    std::vector<std::shared_ptr<GraphNode>> graphNodes;
    std::vector<NodeAudioRoute> activeRoutes;

    // Trigger Thread-Safe
    std::mutex triggerMutex;
    std::vector<TriggerEvent> pendingTriggers;

    // Voci di Riserva
    dsp::AnalogDrumKit drumKit;
    dsp::MelodicSynthVoice synthVoice;
    dsp::MelodicSynthVoice subVoice;
    dsp::SVF_ZDF svfFilterL;
    dsp::SVF_ZDF svfFilterR;
    dsp::StereoTapeDelay tapeDelay;
    dsp::LushReverb reverb;

    // Suoni per ciascun dito del Modulo Occhio (0..3: DX, 4..7: SX)
    std::atomic<int> eyeFingerSound0 { 0 };
    std::atomic<int> eyeFingerSound1 { 1 };
    std::atomic<int> eyeFingerSound2 { 2 };
    std::atomic<int> eyeFingerSound3 { 3 };
    std::atomic<int> eyeFingerSound4 { 4 };
    std::atomic<int> eyeFingerSound5 { 5 };
    std::atomic<int> eyeFingerSound6 { 6 };
    std::atomic<int> eyeFingerSound7 { 10 };

    // Parametri Atomici Macro Master
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
