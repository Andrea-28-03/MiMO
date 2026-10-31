#pragma once

#include <juce_core/juce_core.h>
#include "../../NodeAudioProcessor.h"
#include <vector>
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// Smart Scale Lock: Quantizzatore di Scala Musicale (ut-scale-lock)
// Riferimenti: Surge XT Scale Engine & Microtonal Mapping
// ============================================================================
class ScaleLock : public mimo::audio::NodeAudioProcessor
{
public:
    enum class ScaleType
    {
        PentatonicMajor = 0, // C, D, E, G, A
        NaturalMinor = 1,    // C, D, Eb, F, G, Ab, Bb
        Major = 2,           // C, D, E, F, G, A, B
        Blues = 3,           // C, Eb, F, F#, G, Bb
        Dorian = 4,          // C, D, Eb, F, G, A, Bb
        JapaneseInsen = 5,   // C, Db, F, G, Bb
        Chromatic = 6
    };

    void reset(double /*sampleRate*/) noexcept override {}

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "scale" || paramKey == "type")
        {
            const int idx = juce::roundToInt(v * 6.0f);
            currentScale = static_cast<ScaleType>(juce::jlimit(0, 6, idx));
        }
        else if (paramKey == "root")
        {
            rootNote = juce::roundToInt(v * 11.0f);
        }
        else if (paramKey == "octaves" || paramKey == "range")
        {
            octaveSpan = 1 + juce::roundToInt(v * 3.0f);
        }
        else if (paramKey == "mod:in" || paramKey == "in")
        {
            lastQuantizedHz = quantizeNormalizedToHz(v);
        }
    }

    void processBlock(float* /*left*/, float* /*right*/, int /*numSamples*/) noexcept override
    {
        // ScaleLock è una utility di quantizzazione di controllo CV (trasparente all'audio)
    }

    float getQuantizedHz() const noexcept { return lastQuantizedHz; }

    void setRootNote(int rootMidiPitchClass) noexcept
    {
        rootNote = (rootMidiPitchClass % 12 + 12) % 12;
    }

    void setScaleType(ScaleType type) noexcept
    {
        currentScale = type;
    }

    void setOctaveRange(int minOctave, int numOctaves) noexcept
    {
        baseMidi = juce::jlimit(12, 96, minOctave * 12);
        octaveSpan = juce::jlimit(1, 5, numOctaves);
    }

    // Prende un valore continuo normalizzato (0.0 .. 1.0, es. Asse X mano) e restituisce la frequenza in Hz
    float quantizeNormalizedToHz(float normValue) const noexcept
    {
        const float midiContinuous = static_cast<float>(baseMidi) + juce::jlimit(0.0f, 1.0f, normValue) * static_cast<float>(octaveSpan * 12);
        const float quantizedMidi = quantizeMidi(midiContinuous);
        return 440.0f * std::pow(2.0f, (quantizedMidi - 69.0f) / 12.0f);
    }

    float quantizeMidi(float continuousMidi) const noexcept
    {
        if (currentScale == ScaleType::Chromatic)
            return std::round(continuousMidi);

        const int rounded = juce::roundToInt(continuousMidi);
        const int octave = rounded / 12;
        const int pc = (rounded % 12 + 12) % 12;

        const auto intervals = getScaleIntervals(currentScale);
        int closestDegree = intervals[0];
        int minDist = 999;

        for (int degree : intervals)
        {
            const int noteInKey = (rootNote + degree) % 12;
            const int dist = std::abs(pc - noteInKey);
            if (dist < minDist)
            {
                minDist = dist;
                closestDegree = noteInKey;
            }
        }

        return static_cast<float>(octave * 12 + closestDegree);
    }

private:
    static std::vector<int> getScaleIntervals(ScaleType type)
    {
        switch (type)
        {
            case ScaleType::PentatonicMajor: return { 0, 2, 4, 7, 9 };
            case ScaleType::NaturalMinor:    return { 0, 2, 3, 5, 7, 8, 10 };
            case ScaleType::Major:           return { 0, 2, 4, 5, 7, 9, 11 };
            case ScaleType::Blues:           return { 0, 3, 5, 6, 7, 10 };
            case ScaleType::Dorian:          return { 0, 2, 3, 5, 7, 9, 10 };
            case ScaleType::JapaneseInsen:   return { 0, 1, 5, 7, 10 };
            case ScaleType::Chromatic:
            default:                         return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        }
    }

    int rootNote = 0; // 0 = Do (C)
    ScaleType currentScale = ScaleType::PentatonicMajor;
    int baseMidi = 36; // C2 (65.4 Hz)
    int octaveSpan = 4; // 4 ottave
    float lastQuantizedHz = 220.0f;
};

} // namespace mimo::dsp
