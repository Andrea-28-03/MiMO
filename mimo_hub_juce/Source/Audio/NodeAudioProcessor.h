#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSPUtils.h"
#include <atomic>
#include <vector>

namespace mimo::audio
{

// ============================================================================
// Interfaccia Base Polimorfica per Tutti i Moduli Audio di MiMO (Standard V2)
// ============================================================================
class NodeAudioProcessor
{
public:
    virtual ~NodeAudioProcessor() = default;

    // Reset del motore DSP alla frequenza di campionamento corrente
    virtual void reset(double sampleRate) = 0;

    // Elaborazione a blocchi di campioni stereo in-place pura dell'algoritmo
    virtual void processBlock(float* left, float* right, int numSamples) = 0;

    // Ricezione di impulsi / colpi (da piezo batteria, pinch dell'occhio o sequencer)
    virtual void trigger(int voiceIndex, float velocity) {}

    // Rilascio nota per strumenti con inviluppo sustain
    virtual void noteOff() {}

    // Impostazione di un parametro o modulazione CV continua (valore normalizzato 0.0 - 1.0)
    virtual void setParameter(const juce::String& paramKey, float normalizedValue) {}

    // ========================================================================
    // Standard V2: Controlli Universali di Nodo (Bypass, Volume, Mix Dry/Wet, VU-Meter)
    // ========================================================================
    void setOutputGain(float linearGain) noexcept { outputGain.store(juce::jlimit(0.0f, 4.0f, linearGain)); }
    float getOutputGain() const noexcept { return outputGain.load(); }

    void setDryWet(float mixNorm) noexcept { dryWetMix.store(juce::jlimit(0.0f, 1.0f, mixNorm)); }
    float getDryWet() const noexcept { return dryWetMix.load(); }

    void setBypassed(bool bypass) noexcept { isBypassed.store(bypass); }
    void setBypass(bool bypass) noexcept { setBypassed(bypass); }
    bool getBypassed() const noexcept { return isBypassed.load(); }

    float getPeakLevel() const noexcept { return peakLevel.load(); }

    // Valore di modulazione CV esposto dal nodo (0.0 - 1.0)
    virtual float getModulationValue(const juce::String& /*pinId*/ = "mod:out") const noexcept { return getPeakLevel(); }

    // Wrapper completo che applica Bypass, Dry/Wet, Output Volume e calcola il Peak per il VU-Meter
    void processNode(float* left, float* right, int numSamples, bool isEffect = false)
    {
        if (numSamples <= 0)
            return;

        if (isBypassed.load())
        {
            if (!isEffect)
            {
                // Se uno strumento è spento/bypassato, non genera suono
                if (left != nullptr)  juce::FloatVectorOperations::clear(left, numSamples);
                if (right != nullptr) juce::FloatVectorOperations::clear(right, numSamples);
                peakLevel.store(0.0f);
            }
            else
            {
                // Se un effetto è bypassato, lascia passare il segnale inalterato
                float p = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                {
                    if (left != nullptr)  p = std::max(p, std::abs(left[i]));
                    if (right != nullptr) p = std::max(p, std::abs(right[i]));
                }
                const float prev = peakLevel.load();
                peakLevel.store(std::max(p, prev * 0.88f));
            }
            return;
        }

        const float mix = isEffect ? dryWetMix.load() : 1.0f;
        const float gain = outputGain.load();

        if (isEffect && mix < 0.999f)
        {
            // Salva campioni Dry in uno scratch buffer locale (massimo 1024 campioni su stack)
            constexpr int kMaxScratch = 1024;
            const int procCount = std::min(numSamples, kMaxScratch);
            float dryL[kMaxScratch];
            float dryR[kMaxScratch];

            if (left != nullptr)  std::memcpy(dryL, left, sizeof(float) * static_cast<size_t>(procCount));
            if (right != nullptr) std::memcpy(dryR, right, sizeof(float) * static_cast<size_t>(procCount));

            processBlock(left, right, procCount);

            float p = 0.0f;
            for (int i = 0; i < procCount; ++i)
            {
                if (left != nullptr)
                {
                    const float wetVal = left[i];
                    left[i] = (dryL[i] * (1.0f - mix) + wetVal * mix) * gain;
                    p = std::max(p, std::abs(left[i]));
                }
                if (right != nullptr)
                {
                    const float wetVal = right[i];
                    right[i] = (dryR[i] * (1.0f - mix) + wetVal * mix) * gain;
                    p = std::max(p, std::abs(right[i]));
                }
            }
            const float prev = peakLevel.load();
            peakLevel.store(std::max(p, prev * 0.88f));
        }
        else
        {
            processBlock(left, right, numSamples);

            float p = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                if (left != nullptr)
                {
                    left[i] *= gain;
                    p = std::max(p, std::abs(left[i]));
                }
                if (right != nullptr)
                {
                    right[i] *= gain;
                    p = std::max(p, std::abs(right[i]));
                }
            }
            const float prev = peakLevel.load();
            peakLevel.store(std::max(p, prev * 0.88f));
        }
    }

protected:
    std::atomic<float> outputGain { 1.0f };
    std::atomic<float> dryWetMix { 1.0f };
    std::atomic<bool> isBypassed { false };
    std::atomic<float> peakLevel { 0.0f };
};

} // namespace mimo::audio

namespace mimo::dsp
{
    using NodeAudioProcessor = mimo::audio::NodeAudioProcessor;
}
