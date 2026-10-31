#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <cmath>
#include <algorithm>

namespace mimo::dsp
{

// ============================================================================
// Filtro Moog Ladder a 4 Poli (24 dB/ottava) con Saturazione Transistor
// Architettura: Vadim Zavalishin / Houvilainen VA Ladder Model
// ============================================================================
class MoogLadderNode : public mimo::audio::NodeAudioProcessor
{
public:
    MoogLadderNode() = default;

    void reset(double sampleRate) override
    {
        sr = std::max(8000.0, sampleRate);
        sL[0] = sL[1] = sL[2] = sL[3] = 0.0f;
        sR[0] = sR[1] = sR[2] = sR[3] = 0.0f;
        updateCoeffs();
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "cutoff")
        {
            // Frequenza di taglio esponenziale: 20 Hz .. 18000 Hz
            cutoffHz = 20.0f * std::pow(900.0f, v);
        }
        else if (paramKey == "res" || paramKey == "resonance")
        {
            // Risonanza da 0.0 a 3.9 (auto-oscillazione controllata)
            resonance = v * 3.9f;
        }
        else if (paramKey == "drive")
        {
            // Saturazione d'ingresso da 0 dB a 24 dB
            driveDb = v * 24.0f;
        }
        else if (paramKey == "mix" || paramKey == "drywet")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
        updateCoeffs();
    }

    void processBlock(float* left, float* right, int numSamples) override
    {
        if (left == nullptr || numSamples <= 0)
            return;

        const float drv = driveLinear;
        const float k = resonance;
        const float gComp = 1.0f + 0.5f * (k / 4.0f); // Compensazione perdita basse frequenze

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = (right != nullptr) ? right[i] : inL;

            // Elaborazione Canale Sinistro (4 Poli con saturazione tanh a transistor)
            const float filteredL = processLadder(inL * drv, sL, k) * gComp;
            // Elaborazione Canale Destro
            const float filteredR = processLadder(inR * drv, sR, k) * gComp;

            left[i] = sanitizeSample(filteredL);
            if (right != nullptr)
                right[i] = sanitizeSample(filteredR);
        }
    }

private:
    void updateCoeffs() noexcept
    {
        const float fc = juce::jlimit(20.0f, static_cast<float>(sr * 0.45), cutoffHz);
        const float wd = static_cast<float>(juce::MathConstants<double>::twoPi * fc / sr);
        g = juce::jlimit(0.001f, 0.999f, wd * 0.5f);
        driveLinear = std::pow(10.0f, driveDb / 20.0f);
    }

    float processLadder(float in, float s[4], float k) noexcept
    {
        // Loop di feedback non-lineare
        const float feedback = k * s[3];
        const float u = std::tanh(in - feedback);

        // Stadio 1
        const float v0 = (u - s[0]) * g;
        const float y0 = v0 + s[0];
        s[0] = sanitizeSample(y0 + v0);

        // Stadio 2
        const float v1 = (std::tanh(y0) - s[1]) * g;
        const float y1 = v1 + s[1];
        s[1] = sanitizeSample(y1 + v1);

        // Stadio 3
        const float v2 = (std::tanh(y1) - s[2]) * g;
        const float y2 = v2 + s[2];
        s[2] = sanitizeSample(y2 + v2);

        // Stadio 4
        const float v3 = (std::tanh(y2) - s[3]) * g;
        const float y3 = v3 + s[3];
        s[3] = sanitizeSample(y3 + v3);

        return y3;
    }

    double sr = 48000.0;
    float cutoffHz = 1200.0f;
    float resonance = 0.5f;
    float driveDb = 6.0f;
    float driveLinear = 2.0f;
    float g = 0.1f;

    float sL[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float sR[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

} // namespace mimo::dsp
