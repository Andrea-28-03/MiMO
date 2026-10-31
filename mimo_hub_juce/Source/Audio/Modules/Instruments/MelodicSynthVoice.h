#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// Sintetizzatore Virtual Analog & Piano Feltro (Melodic Synth & Felt Piano)
// Riferimenti: Vadim Zavalishin (ZDF Filter) & Will Pirkle (ADSR + PolyBLEP)
// Supporta modalità Synth Lead / Bass e modalità Piano Feltro con decadimento naturale
// ============================================================================
class MelodicSynthVoice : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        invSr = 1.0f / static_cast<float>(sr);
        filter.reset(sr);
        adsr.reset(sr);

        phase = 0.0f;
        freq = 220.0f;
        targetFreq = 220.0f;
        subPitchEnv = 0.0f;
        feltHammerThud = 0.0f;

        updateEnvelope();
    }

    void setIsPianoFelt(bool isFelt) noexcept
    {
        isPianoFelt = isFelt;
        if (isPianoFelt)
        {
            morph = 0.08f; // Timbro morbido sinusoidale + armonica calda
            macroCutoff = 3600.0f;
            macroReso = 0.8f;
        }
        updateEnvelope();
    }

    bool getIsPianoFelt() const noexcept { return isPianoFelt; }

    void trigger(int subVoice, float velocity) noexcept override
    {
        // Scala pentatonica (C3, D3, E3, G3, A3, C4, D4, E4)
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

        // Martelletto in feltro per il Piano Feltro
        if (isPianoFelt)
            feltHammerThud = juce::jlimit(0.1f, 1.0f, velocity) * 0.45f;

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
            decayNormalized = v;
            updateEnvelope();
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
        if (!adsr.isActive() && feltHammerThud <= 0.0005f)
            return;

        const float invSampleRate = invSr;
        const float thudDecay = std::exp(-invSampleRate * 85.0f); // ~12ms martelletto

        for (int i = 0; i < numSamples; ++i)
        {
            // Portamento / pitch slide dolce
            freq += (targetFreq - freq) * 0.012f;
            const float effectiveFreq = isSub ? (freq + subPitchEnv * 45.0f) : freq;
            subPitchEnv *= 0.9994f;

            phase += effectiveFreq * invSampleRate;
            if (phase >= 1.0f) phase -= 1.0f;

            float raw = 0.0f;
            if (isPianoFelt)
            {
                // Timbro Piano Feltro: fondamentale arrotondata + 2ª armonica dolce + risonanza legno
                const float s1 = std::sin(phase * juce::MathConstants<float>::twoPi);
                const float s2 = std::sin(phase * juce::MathConstants<float>::twoPi * 2.0f) * 0.28f;
                const float s3 = std::sin(phase * juce::MathConstants<float>::twoPi * 3.0f) * 0.09f;
                raw = (s1 + s2 + s3);

                // Rumore del feltro/martelletto all'attacco
                if (feltHammerThud > 0.0005f)
                {
                    raw += feltHammerThud * 0.65f;
                    feltHammerThud *= thudDecay;
                }
            }
            else
            {
                // Oscillatore Synth con morphing PolyBLEP continuo: Sinusoide -> Saw -> Square
                const float sine = std::sin(phase * juce::MathConstants<float>::twoPi);
                const float saw = 2.0f * phase - 1.0f;
                const float square = (phase < 0.5f) ? 0.85f : -0.85f;

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
            }

            const float env = adsr.getNextSample();

            // Modulazione filtro naturale: il suono si scurisce man mano che decade
            const float decayToneTrack = isPianoFelt ? (0.25f + 0.75f * env) : (0.35f + 0.65f * env);
            const float dynamicCutoff = juce::jlimit(50.0f, 16000.0f, macroCutoff * decayToneTrack);
            filter.setParameters(dynamicCutoff, macroReso, SVF_ZDF::Mode::LowPass);

            float filtered = filter.process(raw * env);

            if (isSub)
                filtered = softClipTanh(filtered * 1.4f) * 1.1f;

            filtered = sanitizeSample(filtered * gain);
            if (left != nullptr)  left[i]  += filtered;
            if (right != nullptr) right[i] += filtered;
        }
    }

private:
    void updateEnvelope() noexcept
    {
        if (isPianoFelt)
        {
            // Decadimento naturale a campana: 0.08s (staccato secco) fino a 4.2s (coda lunga da pianoforte a coda)
            // Sustain = 0.0f per garantire che la nota si estingua completamente seguendo il decay!
            const float decTime = 0.08f + std::pow(decayNormalized, 2.0f) * 4.12f;
            adsr.setParameters(0.004f, decTime, 0.0f, decTime * 0.5f);
        }
        else
        {
            // Synth VA con inviluppo rapido per lead e arpeggi
            const float decTime = 0.04f + decayNormalized * 2.96f;
            adsr.setParameters(0.008f, decTime, 0.0f, decTime * 0.75f);
        }
    }

    double sr = 48000.0;
    float invSr = 1.0f / 48000.0f;
    SVF_ZDF filter;
    AnalogADSR adsr;

    float phase = 0.0f;
    float freq = 220.0f;
    float targetFreq = 220.0f;
    float macroCutoff = 3200.0f;
    float macroReso = 1.2f;
    float morph = 0.2f;
    float decayNormalized = 0.55f;
    bool isSub = false;
    bool isPianoFelt = false;
    float subPitchEnv = 0.0f;
    float feltHammerThud = 0.0f;
    float masterGain = 0.95f;
};

} // namespace mimo::dsp
