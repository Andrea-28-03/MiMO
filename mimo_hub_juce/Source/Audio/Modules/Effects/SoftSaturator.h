#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// Saturazione a Nastro & Calore Valvolare (fx-saturazione-soft)
// Riferimenti: Udo Zölzer (DAFX Cap. 4: Nonlinear Waveshaping)
// ============================================================================
class SoftSaturatorNode : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        lpCoeff = 0.85f;
        prevL = 0.0f;
        prevR = 0.0f;
        driveGain = 1.0f;
        setTone(6500.0f);
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);

        if (paramKey == "drive")
        {
            // 0 dB .. +32 dB di spinta armonica
            driveGain = std::pow(10.0f, (v * 32.0f) / 20.0f);
        }
        else if (paramKey == "tone" || paramKey == "cutoff")
        {
            const float hz = 500.0f * std::pow(32.0f, v); // 500 Hz .. 16000 Hz
            setTone(hz);
        }
        else if (paramKey == "mix" || paramKey == "drywet")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
    }

    void setTone(float freqHz) noexcept
    {
        const float f = juce::jlimit(400.0f, 18000.0f, freqHz);
        lpCoeff = std::exp(-juce::MathConstants<float>::twoPi * f / static_cast<float>(sr));
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (numSamples <= 0)
            return;

        // Guadagno compensato per mantenere il volume percepito costante all'aumentare del drive
        const float compGain = 1.0f / std::sqrt(driveGain);

        for (int i = 0; i < numSamples; ++i)
        {
            if (left != nullptr)
            {
                const float in = left[i];
                // Saturazione asimmetrica calda (armonica pari + dispari stile nastro analogico)
                const float biased = in * driveGain + 0.12f * in * in;
                const float sat = softClipTanh(biased) * compGain;

                // De-enfasi analogica
                prevL = sat * (1.0f - lpCoeff) + prevL * lpCoeff;
                left[i] = sanitizeSample(prevL);
            }

            if (right != nullptr)
            {
                const float in = right[i];
                const float biased = in * driveGain + 0.12f * in * in;
                const float sat = softClipTanh(biased) * compGain;

                prevR = sat * (1.0f - lpCoeff) + prevR * lpCoeff;
                right[i] = sanitizeSample(prevR);
            }
        }
    }

private:
    double sr = 48000.0;
    float driveGain = 1.0f;
    float lpCoeff = 0.85f;
    float prevL = 0.0f;
    float prevR = 0.0f;
};

} // namespace mimo::dsp
