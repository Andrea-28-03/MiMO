#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"

namespace mimo::dsp
{

// ============================================================================
// Sintetizzatore Virtual Analog (Melodic Synth & Sub Bass)
// Riferimenti: Vadim Zavalishin (ZDF Filter) & Will Pirkle (PolyBLEP)
// ============================================================================
class MelodicSynthVoice : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        filter.reset(sr);
        adsr.reset(sr);
        adsr.setParameters(0.008f, 0.22f, 0.45f, 0.35f);
        phase = 0.0f;
        freq = 220.0f;
        targetFreq = 220.0f;
        subPitchEnv = 0.0f;
    }

    void trigger(int subVoice, float velocity) noexcept override
    {
        // Scala pentatonica default se chiamato via trigger generico
        const float freqs[] = { 130.81f, 146.83f, 164.81f, 196.00f, 220.0f, 261.63f, 293.66f, 329.63f };
        const float f = freqs[juce::jlimit(0, 7, subVoice % 8)];
        triggerNote(f, velocity, isSub);
    }

    void triggerNote(float frequencyHz, float velocity, bool isSubBass = false) noexcept
    {
        targetFreq = juce::jlimit(20.0f, 4000.0f, frequencyHz);
        if (!isSub)
            freq = targetFreq;
        isSub = isSubBass;
        subPitchEnv = isSub ? 1.0f : 0.0f;
        adsr.noteOn(velocity);
    }

    void noteOff() noexcept override
    {
        adsr.noteOff();
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "vol" || paramKey == "volume")
        {
            masterGain = v * 1.5f;
        }
        else if (paramKey == "cutoff")
        {
            macroCutoff = 60.0f * std::pow(250.0f, v); // 60 Hz .. 15000 Hz
        }
        else if (paramKey == "res" || paramKey == "resonance")
        {
            macroReso = 0.2f + v * 9.8f;
        }
        else if (paramKey == "decay")
        {
            const float decTime = 0.05f + v * 2.95f;
            adsr.setParameters(0.008f, decTime, 0.35f, decTime * 0.8f);
        }
        else if (paramKey == "morph")
        {
            morph = v;
        }
    }

    void setMacroControls(float cutoffHz, float resonance, float timbreMorph) noexcept
    {
        macroCutoff = cutoffHz;
        macroReso = resonance;
        morph = juce::jlimit(0.0f, 1.0f, timbreMorph);
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        renderNextBlock(left, right, numSamples, masterGain);
    }

    void renderNextBlock(float* left, float* right, int numSamples, float gain = 1.0f) noexcept
    {
        if (!adsr.isActive())
            return;

        const float invSr = 1.0f / static_cast<float>(sr);
        for (int i = 0; i < numSamples; ++i)
        {
            // Portamento / pitch slide dolce
            freq += (targetFreq - freq) * 0.008f;
            const float effectiveFreq = isSub ? (freq + subPitchEnv * 45.0f) : freq;
            subPitchEnv *= 0.9994f;

            phase += effectiveFreq * invSr;
            if (phase >= 1.0f) phase -= 1.0f;

            // Oscillatore con Morph continuo: Sinusoide (0.0) -> Dente di sega (0.5) -> Onda quadra (1.0)
            const float sine = std::sin(phase * juce::MathConstants<float>::twoPi);
            const float saw = 2.0f * phase - 1.0f;
            const float square = (phase < 0.5f) ? 0.85f : -0.85f;

            float raw = sine;
            if (morph <= 0.5f)
            {
                const float t = morph * 2.0f;
                raw = sine * (1.0f - t) + saw * t;
            }
            else
            {
                const float t = (morph - 0.5f) * 2.0f;
                raw = saw * (1.0f - t) + square * t;
            }

            const float env = adsr.getNextSample();

            // Modulazione filtro da inviluppo + macro
            const float dynamicCutoff = juce::jlimit(60.0f, 16000.0f, macroCutoff * (0.3f + env * 0.85f));
            filter.setParameters(dynamicCutoff, macroReso, SVF_ZDF::Mode::LowPass);
            float filtered = filter.process(raw * env);

            if (isSub)
                filtered = softClipTanh(filtered * 1.4f) * 1.1f; // Calore armonico 808

            filtered = sanitizeSample(filtered * gain);
            if (left != nullptr)  left[i]  += filtered;
            if (right != nullptr) right[i] += filtered;
        }
    }

private:
    double sr = 48000.0;
    SVF_ZDF filter;
    AnalogADSR adsr;

    float phase = 0.0f;
    float freq = 220.0f;
    float targetFreq = 220.0f;
    float macroCutoff = 2200.0f;
    float macroReso = 1.4f;
    float morph = 0.2f;
    bool isSub = false;
    float subPitchEnv = 0.0f;
    float masterGain = 0.95f;
};

} // namespace mimo::dsp
