#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"

namespace mimo::dsp
{

// ============================================================================
// Generatore Percussivo "Analog Drum Kit"
// Riferimenti: Émilie Gillet (Plaits) & Will Pirkle
// Include 4 voci: Cassa (Kick), Rullante (Snare), Hi-Hat (Hat) e Tom/Kalimba
// ============================================================================
class AnalogDrumKit : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        filterKick.reset(sr);
        filterSnare.reset(sr);
        filterHat.reset(sr);
        filterHat.setParameters(7500.0f, 1.2f, SVF_ZDF::Mode::HighPass);
        rng = 123456789u;

        kickEnv = 0.0f;
        kickPitch = 0.0f;
        kickPhase = 0.0f;

        snareEnv = 0.0f;
        snarePhase = 0.0f;

        hatEnv = 0.0f;
        hatPhase = 0.0f;

        tomEnv = 0.0f;
        tomPhase = 0.0f;
    }

    void trigger(int subVoice, float velocity) noexcept override
    {
        const float v = juce::jlimit(0.15f, 1.0f, velocity);
        switch (subVoice % 4)
        {
            case 0: // Cassa (Kick)
                kickEnv = v;
                kickPitch = 1.0f;
                kickPhase = 0.0f;
                break;
            case 1: // Rullante (Snare)
                snareEnv = v;
                snarePhase = 0.0f;
                break;
            case 2: // Hi-Hat
                hatEnv = v;
                break;
            case 3: // Tom / Percussione
            default:
                tomEnv = v;
                tomPhase = 0.0f;
                break;
        }
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "vol" || paramKey == "volume")
        {
            masterVol = v * 1.5f;
        }
        else if (paramKey == "pitch")
        {
            pitchShiftMult = std::pow(2.0f, (v - 0.5f) * 2.0f); // -1 ottava .. +1 ottava
        }
        else if (paramKey == "decay")
        {
            decayFactor = 0.4f + v * 1.6f;
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        renderNextBlock(left, right, numSamples, masterVol);
    }

    void renderNextBlock(float* left, float* right, int numSamples, float gain = 1.0f) noexcept
    {
        if (kickEnv <= 0.0005f && snareEnv <= 0.0005f && hatEnv <= 0.0005f && tomEnv <= 0.0005f)
            return;

        const float invSr = 1.0f / static_cast<float>(sr);
        const float kickDecay = std::exp(-invSr * 2.8f / decayFactor);       // ~350ms
        const float kickPitchDecay = std::exp(-invSr * 28.0f);               // ~35ms drop
        const float snareDecay = std::exp(-invSr * 6.5f / decayFactor);      // ~150ms
        const float hatDecay = std::exp(-invSr * 22.0f / decayFactor);       // ~45ms
        const float tomDecay = std::exp(-invSr * 3.8f / decayFactor);        // ~260ms

        for (int i = 0; i < numSamples; ++i)
        {
            float smp = 0.0f;

            // --- 1. KICK ---
            if (kickEnv > 0.0005f)
            {
                const float freq = (42.0f + kickPitch * 115.0f) * pitchShiftMult;
                kickPhase += freq * invSr;
                if (kickPhase >= 1.0f) kickPhase -= 1.0f;

                const float click = (kickPitch > 0.6f) ? (kickPitch * 0.25f) : 0.0f;
                const float rawKick = std::sin(kickPhase * juce::MathConstants<float>::twoPi) + click;
                smp += softClipTanh(rawKick * 1.3f) * kickEnv * 1.1f;

                kickPitch *= kickPitchDecay;
                kickEnv *= kickDecay;
            }

            // --- 2. SNARE ---
            if (snareEnv > 0.0005f)
            {
                snarePhase += (175.0f * pitchShiftMult) * invSr;
                if (snarePhase >= 1.0f) snarePhase -= 1.0f;
                const float body = std::sin(snarePhase * juce::MathConstants<float>::twoPi) * 0.45f;

                rng = rng * 1664525u + 1013904223u;
                const float white = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
                filterSnare.setParameters(1900.0f * pitchShiftMult, 1.8f, SVF_ZDF::Mode::BandPass);
                const float rattle = filterSnare.process(white) * 1.1f;

                smp += (body + rattle) * snareEnv * 0.90f;
                snareEnv *= snareDecay;
            }

            // --- 3. HI-HAT ---
            if (hatEnv > 0.0005f)
            {
                hatPhase += 380.0f * invSr;
                if (hatPhase >= 1.0f) hatPhase -= 1.0f;

                float metal = 0.0f;
                metal += (std::fmod(hatPhase * 1.00f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.34f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.78f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 2.24f, 1.0f) > 0.5f ? 1.0f : -1.0f);

                const float filteredHat = filterHat.process(metal * 0.25f);
                smp += filteredHat * hatEnv * 0.70f;
                hatEnv *= hatDecay;
            }

            // --- 4. TOM / KALIMBA ---
            if (tomEnv > 0.0005f)
            {
                tomPhase += (120.0f * pitchShiftMult) * invSr;
                if (tomPhase >= 1.0f) tomPhase -= 1.0f;
                const float tomWave = std::sin(tomPhase * juce::MathConstants<float>::twoPi)
                                    + 0.25f * std::sin(tomPhase * juce::MathConstants<float>::twoPi * 2.0f);
                smp += tomWave * tomEnv * 0.85f;
                tomEnv *= tomDecay;
            }

            smp = sanitizeSample(smp * gain);
            if (left != nullptr)  left[i]  += smp;
            if (right != nullptr) right[i] += smp;
        }
    }

private:
    double sr = 48000.0;
    uint32_t rng = 123456789u;

    SVF_ZDF filterKick;
    SVF_ZDF filterSnare;
    SVF_ZDF filterHat;

    float kickEnv = 0.0f;
    float kickPitch = 0.0f;
    float kickPhase = 0.0f;

    float snareEnv = 0.0f;
    float snarePhase = 0.0f;

    float hatEnv = 0.0f;
    float hatPhase = 0.0f;

    float tomEnv = 0.0f;
    float tomPhase = 0.0f;

    float masterVol = 1.0f;
    float pitchShiftMult = 1.0f;
    float decayFactor = 1.0f;
};

} // namespace mimo::dsp
