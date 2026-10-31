#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <cmath>
#include <atomic>

namespace mimo::dsp
{

// ============================================================================
// Seguitore d'Inviluppo Audio (Envelope Follower) (mod-env-follower)
// Riferimenti: Udo Zölzer ("DAFX", Cap. 5: Envelope Extractors)
// Converte la dinamica del segnale audio in un segnale di controllo CV (0.0 .. 1.0)
// ============================================================================
class EnvelopeFollower : public mimo::audio::NodeAudioProcessor
{
public:
    EnvelopeFollower() = default;

    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        invSr = 1.0f / static_cast<float>(sr);
        envelope = 0.0f;
        targetEnv = 0.0f;
        updateCoefficients();
    }

    void setParameters(float attackMs, float releaseMs, float sensitivityGain, bool invertEnv = false) noexcept
    {
        attackTimeMs = juce::jlimit(1.0f, 150.0f, attackMs);
        releaseTimeMs = juce::jlimit(10.0f, 1200.0f, releaseMs);
        gain = juce::jlimit(0.1f, 8.0f, sensitivityGain);
        inverted = invertEnv;
        updateCoefficients();
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "attack" || paramKey == "att")
        {
            attackTimeMs = 1.0f + v * 99.0f; // 1 .. 100 ms
            updateCoefficients();
        }
        else if (paramKey == "release" || paramKey == "rel")
        {
            releaseTimeMs = 10.0f + v * 790.0f; // 10 .. 800 ms
            updateCoefficients();
        }
        else if (paramKey == "gain" || paramKey == "sens" || paramKey == "sensitivity")
        {
            gain = 0.2f + v * 4.8f; // 0.2x .. 5.0x
        }
        else if (paramKey == "invert")
        {
            inverted = (v >= 0.5f);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (left == nullptr && right == nullptr)
            return;

        float maxPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = (left != nullptr) ? left[i] : 0.0f;
            const float r = (right != nullptr) ? right[i] : 0.0f;

            // Rilevatore di picco a onda intera (Full-wave peak detection)
            const float inAbs = std::max(std::abs(l), std::abs(r)) * gain;
            maxPeak = std::max(maxPeak, inAbs);

            // Ballistica analogica asimmetrica: Attacco rapido vs Rilascio esponenziale
            if (inAbs > envelope)
                envelope += attackCoeff * (inAbs - envelope);
            else
                envelope += releaseCoeff * (inAbs - envelope);

            // Pass-thru audio inalterato (l'envelope follower fa da tap trasparente)
        }

        const float clampedEnv = juce::jlimit(0.0f, 1.0f, envelope);
        const float finalVal = inverted ? (1.0f - clampedEnv) : clampedEnv;
        currentOutputValue.store(finalVal);
        cachedPeak.store(std::min(1.5f, maxPeak));
    }

    // Lettura CV thread-safe per modulazione verso altri nodi
    float getEnvelopeCV() const noexcept
    {
        return currentOutputValue.load();
    }

private:
    void updateCoefficients() noexcept
    {
        const float attSec = std::max(0.0005f, attackTimeMs * 0.001f);
        const float relSec = std::max(0.005f, releaseTimeMs * 0.001f);

        attackCoeff = 1.0f - std::exp(-invSr / attSec);
        releaseCoeff = 1.0f - std::exp(-invSr / relSec);
    }

    double sr = 48000.0;
    float invSr = 1.0f / 48000.0f;

    float attackTimeMs = 12.0f;
    float releaseTimeMs = 180.0f;
    float gain = 1.8f;
    bool inverted = false;

    float attackCoeff = 0.1f;
    float releaseCoeff = 0.01f;
    float envelope = 0.0f;
    float targetEnv = 0.0f;

    std::atomic<float> currentOutputValue { 0.0f };
    std::atomic<float> cachedPeak { 0.0f };
};

} // namespace mimo::dsp
