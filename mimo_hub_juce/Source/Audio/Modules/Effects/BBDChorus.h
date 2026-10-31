#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace mimo::dsp
{

// ============================================================================
// BBD Chorus Stereo Analogico (Standard V2)
// Modello: Linee di ritardo a secchiello (Bucket-Brigade Device) con LFO in quadratura
// Ispirazione: Roland Juno-106 Chorus & Dimension D
// ============================================================================
class BBDChorusNode : public mimo::audio::NodeAudioProcessor
{
public:
    BBDChorusNode() = default;

    void reset(double sampleRate) override
    {
        sr = std::max(8000.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 0.06); // Fino a 60 ms
        bufferL.assign(maxDelaySamples, 0.0f);
        bufferR.assign(maxDelaySamples, 0.0f);
        writeIndex = 0;
        lfoPhase = 0.0f;
        lpL = 0.0f;
        lpR = 0.0f;
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "rate" || paramKey == "speed")
        {
            // Frequenza LFO: da 0.1 Hz a 8.0 Hz (esponenziale)
            rateHz = 0.1f * std::pow(80.0f, v);
        }
        else if (paramKey == "depth")
        {
            depthNorm = v; // 0 .. 100%
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

    void processBlock(float* left, float* right, int numSamples) override
    {
        if (left == nullptr || numSamples <= 0)
            return;

        const int bufSize = static_cast<int>(bufferL.size());
        if (bufSize < 100)
            return;

        const float lfoPhaseInc = static_cast<float>(juce::MathConstants<double>::twoPi * rateHz / sr);
        // Ritardo base ~8.5ms, escursione modulazione fino a ~5.5ms
        const float baseDelaySamples = static_cast<float>(sr * 0.0085);
        const float maxModSamples = static_cast<float>(sr * 0.0055) * depthNorm;

        // Filtro di ricostruzione BBD (passa-basso a ~8.5 kHz)
        const float alphaLp = static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi * 8500.0 / sr));

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = (right != nullptr) ? right[i] : inL;

            // Scrittura nel buffer circolare
            bufferL[writeIndex] = inL;
            bufferR[writeIndex] = inR;

            // Due LFO sfasati di 90 gradi per effetto ensemble spaziale
            const float modL = std::sin(lfoPhase);
            const float modR = std::cos(lfoPhase);

            const float curDelayL = baseDelaySamples + modL * maxModSamples;
            const float curDelayR = baseDelaySamples + modR * maxModSamples;

            // Lettura frazionaria con interpolazione lineare
            const float wetL = readDelayLinear(bufferL, writeIndex, curDelayL, bufSize);
            const float wetR = readDelayLinear(bufferR, writeIndex, curDelayR, bufSize);

            // Filtro anti-imaging vintage BBD
            lpL += alphaLp * (wetL - lpL);
            lpR += alphaLp * (wetR - lpR);

            left[i] = sanitizeSample(lpL);
            if (right != nullptr)
                right[i] = sanitizeSample(lpR);

            lfoPhase += lfoPhaseInc;
            if (lfoPhase >= juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            if (++writeIndex >= bufSize)
                writeIndex = 0;
        }
    }

private:
    static inline float readDelayLinear(const std::vector<float>& buf, int wIdx, float delaySamples, int size) noexcept
    {
        float readPos = static_cast<float>(wIdx) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(size);
        while (readPos >= static_cast<float>(size))
            readPos -= static_cast<float>(size);

        const int i0 = static_cast<int>(readPos);
        const int i1 = (i0 + 1 >= size) ? 0 : (i0 + 1);
        const float frac = readPos - static_cast<float>(i0);

        return buf[i0] + frac * (buf[i1] - buf[i0]);
    }

    double sr = 48000.0;
    std::vector<float> bufferL;
    std::vector<float> bufferR;
    int writeIndex = 0;

    float rateHz = 1.25f;
    float depthNorm = 0.65f;
    float lfoPhase = 0.0f;

    float lpL = 0.0f;
    float lpR = 0.0f;
};

} // namespace mimo::dsp
