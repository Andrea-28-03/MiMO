#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"

namespace mimo::dsp
{

// ============================================================================
// Filtro SVF ZDF (State Variable Filter - Zero-Delay Feedback)
// Architettura: Vadim Zavalishin ("The Art of VA Filter Design", Cap. 3-4)
// Metodo TPT: Stabilità assoluta a frequenze di taglio estreme e risonanza elevata
// ============================================================================
class SVF_ZDF
{
public:
    enum class Mode { LowPass, BandPass, HighPass, Notch };

    SVF_ZDF() = default;

    void reset(double sampleRate) noexcept
    {
        sr = std::max(8000.0, sampleRate);
        s1 = 0.0f;
        s2 = 0.0f;
    }

    void setParameters(float cutoffHz, float resonanceQ, Mode filterMode = Mode::LowPass) noexcept
    {
        cutoff = juce::jlimit(20.0f, static_cast<float>(sr * 0.45), cutoffHz);
        q = juce::jlimit(0.1f, 15.0f, resonanceQ);
        mode = filterMode;

        // Discretizzazione conforme TPT (Bilinear Transform senza approssimazione)
        const float wd = juce::MathConstants<float>::pi * cutoff / static_cast<float>(sr);
        const float wa = std::tan(wd);
        g = wa;
        r = 1.0f / (2.0f * q);
        h = 1.0f / (1.0f + 2.0f * r * g + g * g);
    }

    float process(float in) noexcept
    {
        const float hp = (in - (2.0f * r + g) * s1 - s2) * h;
        const float bp = g * hp + s1;
        s1 = g * hp + bp; // Integratore trapezoidale 1
        const float lp = g * bp + s2;
        s2 = g * bp + lp; // Integratore trapezoidale 2

        s1 = sanitizeSample(s1);
        s2 = sanitizeSample(s2);

        switch (mode)
        {
            case Mode::LowPass:  return lp;
            case Mode::BandPass: return bp;
            case Mode::HighPass: return hp;
            case Mode::Notch:    return hp + lp;
            default:             return lp;
        }
    }

private:
    double sr = 48000.0;
    float cutoff = 1800.0f;
    float q = 1.0f;
    Mode mode = Mode::LowPass;

    float g = 0.1f;
    float r = 0.5f;
    float h = 1.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
};

// ============================================================================
// Modulo Effetto Stereo SVF (Standard V2)
// ============================================================================
class SVFFilterNode : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) override
    {
        filterL.reset(sampleRate);
        filterR.reset(sampleRate);
        updateFilterParams();
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "cutoff")
        {
            // Curva esponenziale 20 Hz .. 18000 Hz
            cutoffHz = 20.0f * std::pow(900.0f, v);
        }
        else if (paramKey == "res" || paramKey == "resonance")
        {
            resoQ = 0.1f + v * 9.9f;
        }
        else if (paramKey == "mix" || paramKey == "drywet")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
        else if (paramKey == "mode")
        {
            const int m = juce::roundToInt(v * 2.0f);
            currentMode = (m == 0) ? SVF_ZDF::Mode::LowPass : ((m == 1) ? SVF_ZDF::Mode::BandPass : SVF_ZDF::Mode::HighPass);
        }
        updateFilterParams();
    }

    void processBlock(float* left, float* right, int numSamples) override
    {
        if (left == nullptr || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = (right != nullptr) ? right[i] : inL;

            left[i] = sanitizeSample(filterL.process(inL));
            if (right != nullptr)
                right[i] = sanitizeSample(filterR.process(inR));
        }
    }

private:
    void updateFilterParams()
    {
        filterL.setParameters(cutoffHz, resoQ, currentMode);
        filterR.setParameters(cutoffHz, resoQ, currentMode);
    }

    SVF_ZDF filterL;
    SVF_ZDF filterR;

    float cutoffHz = 1800.0f;
    float resoQ = 1.2f;
    SVF_ZDF::Mode currentMode = SVF_ZDF::Mode::LowPass;
};

} // namespace mimo::dsp
