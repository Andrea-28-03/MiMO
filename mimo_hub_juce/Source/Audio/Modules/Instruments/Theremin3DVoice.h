#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"
#include <array>

namespace mimo::dsp
{

// ============================================================================
// Theremin 3D Gestuale a Scala Quantizzata (ins-theremin-3d)
// Riferimenti: Udo Zölzer (DAFX Cap. 3) & Serge XT Scale Engine
// Modulazione fluida continua dagli assi 3D della mano via MediaPipe
// ============================================================================
class Theremin3DVoice : public mimo::audio::NodeAudioProcessor
{
public:
    enum class ScaleType
    {
        Chromatic = 0,
        Pentatonic = 1,
        Minor = 2,
        Major = 3,
        Blues = 4,
        Dorian = 5
    };

    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        invSr = 1.0f / static_cast<float>(sr);

        pitchSmoother.reset(sr, 0.045f); // 45 ms smoothing portamento di default
        ampSmoother.reset(sr, 0.025f);   // 25 ms smoothing volume
        filterSmoother.reset(sr, 0.030f);

        pitchSmoother.setCurrentAndTarget(220.0f);
        ampSmoother.setCurrentAndTarget(0.0f);
        filterSmoother.setCurrentAndTarget(1200.0f);

        filter.reset(sr);
        filter.setParameters(1200.0f, 2.2f, SVF_ZDF::Mode::LowPass);

        phase = 0.0f;
        vibratoPhase = 0.0f;
        handPresent = false;
        triggerHit = 0.0f;
    }

    void setScale(ScaleType s) noexcept { scale = s; }

    void setHand3D(float x, float y, float z, bool isPresent) noexcept
    {
        rawX = juce::jlimit(0.0f, 1.0f, x);
        rawY = juce::jlimit(0.0f, 1.0f, y);
        rawZ = juce::jlimit(0.0f, 1.0f, z);
        handPresent = isPresent;

        updatePitchTarget();
        const float cutoff = 250.0f + std::pow(rawY, 1.6f) * 7500.0f;
        filterSmoother.setTarget(cutoff);
        ampSmoother.setTarget(handPresent ? (0.2f + rawZ * 0.8f) : 0.0f);
    }

    void trigger(int /*voiceIndex*/, float velocity) noexcept override
    {
        triggerHit = juce::jlimit(0.2f, 1.0f, velocity);
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);

        if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
        else if (paramKey == "scale")
        {
            const int idx = juce::roundToInt(v * 5.0f);
            scale = static_cast<ScaleType>(juce::jlimit(0, 5, idx));
        }
        else if (paramKey == "glide" || paramKey == "portamento")
        {
            const float t = 0.005f + v * 0.35f; // 5ms .. 350ms
            pitchSmoother.reset(sr, t);
        }
        else if (paramKey == "vibrato")
        {
            vibratoDepth = v * 0.06f; // Fino a +/- 6% di pitch
        }
        else if (paramKey == "mod:x" || paramKey == "x")
        {
            // Asse X: Intonazione (Pitch)
            rawX = v;
            updatePitchTarget();
        }
        else if (paramKey == "mod:y" || paramKey == "y")
        {
            // Asse Y: Filtro timbrico / Apertura vocale
            rawY = v;
            const float cutoff = 250.0f + std::pow(v, 1.6f) * 7500.0f;
            filterSmoother.setTarget(cutoff);
        }
        else if (paramKey == "mod:z" || paramKey == "z" || paramKey == "dist")
        {
            // Asse Z: Prossimità / Volume / Presenza della mano
            rawZ = v;
            handPresent = (v > 0.04f);
            ampSmoother.setTarget(handPresent ? (0.2f + v * 0.8f) : 0.0f);
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float currentFreq = pitchSmoother.getNextValue();
            const float currentAmp = ampSmoother.getNextValue();
            const float currentCutoff = filterSmoother.getNextValue();

            if (currentAmp <= 0.0005f && triggerHit <= 0.0005f)
            {
                if (left != nullptr)  left[i] = 0.0f;
                if (right != nullptr) right[i] = 0.0f;
                continue;
            }

            // LFO Vibrato naturale a 5.4 Hz
            vibratoPhase += 5.4f * invSr;
            if (vibratoPhase >= 1.0f) vibratoPhase -= 1.0f;
            const float vib = std::sin(vibratoPhase * juce::MathConstants<float>::twoPi) * vibratoDepth;

            // Oscillatore con leggera modulazione di frequenza vibrato
            const float instFreq = currentFreq * (1.0f + vib);
            phase += instFreq * invSr;
            if (phase >= 1.0f) phase -= 1.0f;

            // Generazione timbro theremin caldo: fondamentale pura + armonica dolce di 2° ordine
            const float sin1 = std::sin(phase * juce::MathConstants<float>::twoPi);
            const float sin2 = std::sin(phase * juce::MathConstants<float>::twoPi * 2.0f);
            const float rawTone = sin1 * 0.82f + sin2 * 0.18f;

            // Saturazione soft per calore valvolare
            const float saturated = softClipTanh(rawTone * 1.35f);

            // Filtraggio dinamico guidato dall'asse Y
            filter.setParameters(currentCutoff, 2.0f, SVF_ZDF::Mode::LowPass);
            const float filtered = filter.process(saturated);

            // Attenuazione inviluppo trigger colpo + presenza mano
            float effectiveAmp = currentAmp;
            if (triggerHit > 0.001f)
            {
                effectiveAmp = std::max(effectiveAmp, triggerHit);
                triggerHit *= 0.9992f; // Decadimento rapido se attivato da trigger
            }

            const float sample = sanitizeSample(filtered * effectiveAmp * 0.90f);

            if (left != nullptr)  left[i]  = sample;
            if (right != nullptr) right[i] = sample;
        }
    }

private:
    void updatePitchTarget() noexcept
    {
        // 4 ottave musicali da C2 (65.4 Hz) a C6 (1046.5 Hz)
        constexpr float baseMidi = 36.0f; // C2
        constexpr float spanMidi = 48.0f; // 4 ottave

        const float continuousMidi = baseMidi + rawX * spanMidi;
        float finalMidi = continuousMidi;

        if (scale != ScaleType::Chromatic)
        {
            finalMidi = quantizeToScale(continuousMidi, scale);
        }

        const float targetHz = 440.0f * std::pow(2.0f, (finalMidi - 69.0f) / 12.0f);
        pitchSmoother.setTarget(targetHz);
    }

    static float quantizeToScale(float midiNote, ScaleType s) noexcept
    {
        const int noteInt = juce::roundToInt(midiNote);
        const int octave = noteInt / 12;
        const int pitchClass = (noteInt % 12 + 12) % 12;

        const auto getScaleNotes = [s]() -> std::vector<int>
        {
            switch (s)
            {
                case ScaleType::Pentatonic: return { 0, 2, 4, 7, 9 };       // Pentatonica Maggiore (C, D, E, G, A)
                case ScaleType::Minor:      return { 0, 2, 3, 5, 7, 8, 10 }; // Naturale Minore (Eolia)
                case ScaleType::Major:      return { 0, 2, 4, 5, 7, 9, 11 }; // Maggiore (Ionica)
                case ScaleType::Blues:      return { 0, 3, 5, 6, 7, 10 };    // Scala Blues esatonica
                case ScaleType::Dorian:     return { 0, 2, 3, 5, 7, 9, 10 }; // Modo Dorico
                case ScaleType::Chromatic:
                default:                    return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
            }
        };

        const auto scaleNotes = getScaleNotes();
        int closestNote = scaleNotes[0];
        int minDist = 999;
        for (int sn : scaleNotes)
        {
            const int dist = std::abs(pitchClass - sn);
            if (dist < minDist)
            {
                minDist = dist;
                closestNote = sn;
            }
        }

        return static_cast<float>(octave * 12 + closestNote);
    }

    double sr = 48000.0;
    float invSr = 1.0f / 48000.0f;

    ScaleType scale = ScaleType::Pentatonic;
    OnePoleSmoother pitchSmoother;
    OnePoleSmoother ampSmoother;
    OnePoleSmoother filterSmoother;

    SVF_ZDF filter;

    float rawX = 0.5f;
    float rawY = 0.5f;
    float rawZ = 0.0f;
    bool handPresent = false;

    float phase = 0.0f;
    float vibratoPhase = 0.0f;
    float vibratoDepth = 0.015f;
    float triggerHit = 0.0f;
};

} // namespace mimo::dsp
