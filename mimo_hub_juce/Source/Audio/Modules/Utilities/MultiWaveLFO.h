#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace mimo::dsp
{

// ============================================================================
// Multi-Wave LFO Modulator (Standard V2)
// Generatore ciclico di tensioni di controllo CV (0.0 .. 1.0)
// Forme d'onda: Sinusoidale, Triangolare, Rampa a dente di sega, Sample & Hold
// ============================================================================
class MultiWaveLFONode : public mimo::audio::NodeAudioProcessor
{
public:
    enum class WaveShape
    {
        Sine = 0,
        Triangle = 1,
        Sawtooth = 2,
        SampleAndHold = 3
    };

    MultiWaveLFONode()
    {
        rng.seed(1337);
        currentShValue = 0.5f;
    }

    void reset(double sampleRate) override
    {
        sr = std::max(8000.0, sampleRate);
        phase = 0.0f;
        prevPhase = 0.0f;
        smoothValue = 0.5f;
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "rate" || paramKey == "speed" || paramKey == "freq")
        {
            // Da 0.05 Hz a 20.0 Hz (esponenziale)
            rateHz = 0.05f * std::pow(400.0f, v);
        }
        else if (paramKey == "wave" || paramKey == "shape")
        {
            const int idx = juce::roundToInt(v * 3.0f);
            currentShape = static_cast<WaveShape>(juce::jlimit(0, 3, idx));
        }
        else if (paramKey == "depth" || paramKey == "amp")
        {
            depth = v;
        }
    }

    void processBlock(float* /*left*/, float* /*right*/, int numSamples) override
    {
        if (numSamples <= 0)
            return;

        const float phaseInc = static_cast<float>(rateHz / sr);

        for (int i = 0; i < numSamples; ++i)
        {
            phase += phaseInc;
            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                // Nuovo valore casuale per Sample & Hold al completamento del ciclo
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                currentShValue = dist(rng);
            }

            float raw = 0.5f;
            switch (currentShape)
            {
                case WaveShape::Sine:
                    raw = 0.5f + 0.5f * std::sin(phase * juce::MathConstants<float>::twoPi);
                    break;
                case WaveShape::Triangle:
                    raw = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
                    break;
                case WaveShape::Sawtooth:
                    raw = phase;
                    break;
                case WaveShape::SampleAndHold:
                    raw = currentShValue;
                    break;
            }

            // Smoothing a un polo per eliminare scatti secchi nel controllo CV
            smoothValue += 0.02f * (raw - smoothValue);
        }

        const float finalVal = 0.5f + (smoothValue - 0.5f) * depth;
        lastOutput = juce::jlimit(0.0f, 1.0f, finalVal);
        peakLevel.store(lastOutput.load());
    }

    float getModulationOutput() const noexcept
    {
        return lastOutput.load();
    }

private:
    double sr = 48000.0;
    float rateHz = 1.5f;
    float depth = 0.8f;
    WaveShape currentShape = WaveShape::Sine;

    float phase = 0.0f;
    float prevPhase = 0.0f;
    float smoothValue = 0.5f;
    float currentShValue = 0.5f;
    std::mt19937 rng;

    std::atomic<float> lastOutput { 0.5f };
};

} // namespace mimo::dsp
