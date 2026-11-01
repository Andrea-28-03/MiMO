#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace mimo
{

/**
 * PitchDetector
 * Monophonic pitch detector and dynamic envelope follower optimized for vocal and acoustic input.
 * Real-time, zero allocation in process(), using autocorrelation with parabolic interpolation.
 */
class PitchDetector
{
public:
    PitchDetector()
    {
        buffer.resize(maxBufferSize, 0.0f);
    }

    void prepare(double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        bufferWritePos = 0;
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        envelope = 0.0f;
    }

    struct Result
    {
        float frequencyHz = 0.0f;
        int midiNote = 0;
        float normalizedCv = 0.0f; // 0.0 to 1.0 (covering MIDI 36 / C2 to MIDI 84 / C6)
        float envelopeCv = 0.0f;   // 0.0 to 1.0 (smooth RMS / peak envelope)
        juce::String noteName;     // e.g. "A3", "C4"
        bool isVoiced = false;
    };

    Result processBlock(const float* inputSamples, int numSamples)
    {
        Result res;
        if (inputSamples == nullptr || numSamples <= 0)
            return res;

        // 1. Dynamic Envelope Follower
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float absVal = std::abs(inputSamples[i]);
            if (absVal > peak)
                peak = absVal;
        }

        const float attackCoeff = 0.45f;
        const float releaseCoeff = 0.08f;
        if (peak > envelope)
            envelope = envelope + attackCoeff * (peak - envelope);
        else
            envelope = envelope + releaseCoeff * (peak - envelope);

        res.envelopeCv = juce::jlimit(0.0f, 1.0f, envelope * 2.2f);

        // 2. Accumula campioni nel buffer circolare per pitch detection
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[bufferWritePos] = inputSamples[i];
            bufferWritePos = (bufferWritePos + 1) % maxBufferSize;
        }

        // Se l'ampiezza è troppo bassa, silenzioso
        if (envelope < 0.015f)
        {
            lastFrequency = 0.0f;
            lastNoteName = "--";
            res.noteName = "--";
            res.isVoiced = false;
            return res;
        }

        // 3. Normalized Autocorrelation per stima f0
        const int minLag = static_cast<int>(sr / 1000.0); // max 1000 Hz
        const int maxLag = static_cast<int>(sr / 65.0);   // min 65 Hz (C2)
        const int windowSize = 512;

        if (maxLag + windowSize > maxBufferSize)
            return res;

        // Copia finestra lineare per calcolo
        float linearBuf[1024];
        const int startRead = (bufferWritePos - (maxLag + windowSize) + maxBufferSize) % maxBufferSize;
        for (int i = 0; i < maxLag + windowSize && i < 1024; ++i)
            linearBuf[i] = buffer[(startRead + i) % maxBufferSize];

        float bestAutocorr = -1.0f;
        int bestLag = -1;

        // Energia a lag zero
        float energy0 = 0.0001f;
        for (int i = 0; i < windowSize; ++i)
            energy0 += linearBuf[i] * linearBuf[i];

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float corr = 0.0f;
            float energyLag = 0.0001f;
            for (int i = 0; i < windowSize; ++i)
            {
                const float s0 = linearBuf[i];
                const float sLag = linearBuf[i + lag];
                corr += s0 * sLag;
                energyLag += sLag * sLag;
            }

            const float normCorr = corr / std::sqrt(energy0 * energyLag);
            if (normCorr > bestAutocorr)
            {
                bestAutocorr = normCorr;
                bestLag = lag;
            }
        }

        // Soglia di confidenza periodica (chiarezza timbrica vocale)
        if (bestAutocorr >= 0.58f && bestLag > 0)
        {
            float refinedLag = static_cast<float>(bestLag);
            // Interpolazione parabolica dei picchi per intonazione precisa al centesimo
            if (bestLag > minLag && bestLag < maxLag)
            {
                float corrL = 0.0f, corrR = 0.0f;
                for (int i = 0; i < windowSize; ++i)
                {
                    corrL += linearBuf[i] * linearBuf[i + (bestLag - 1)];
                    corrR += linearBuf[i] * linearBuf[i + (bestLag + 1)];
                }
                const float delta = (corrR - corrL) / (2.0f * (2.0f * bestAutocorr - corrL - corrR) + 0.0001f);
                if (std::abs(delta) < 1.0f)
                    refinedLag += delta;
            }

            const float rawFreq = static_cast<float>(sr / refinedLag);
            // Filtro IIR leggero sulla frequenza per evitare jitter
            lastFrequency = (lastFrequency > 20.0f) ? (lastFrequency * 0.40f + rawFreq * 0.60f) : rawFreq;
            res.frequencyHz = lastFrequency;

            // Calcolo MIDI Note
            const float midiFloat = 69.0f + 12.0f * std::log2(std::max(1.0f, lastFrequency) / 440.0f);
            res.midiNote = juce::jlimit(24, 96, static_cast<int>(std::round(midiFloat)));

            // CV Normalizzato: da MIDI 36 (C2) a 84 (C6) -> 0.0 .. 1.0
            res.normalizedCv = juce::jlimit(0.0f, 1.0f, (midiFloat - 36.0f) / 48.0f);

            static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int noteIdx = (res.midiNote % 12 + 12) % 12;
            const int octave = (res.midiNote / 12) - 1;
            res.noteName = juce::String(noteNames[noteIdx]) + juce::String(octave);
            lastNoteName = res.noteName;
            res.isVoiced = true;
        }
        else
        {
            res.frequencyHz = lastFrequency;
            res.noteName = lastNoteName.isNotEmpty() ? lastNoteName : "--";
            res.isVoiced = false;
        }

        return res;
    }

private:
    static constexpr int maxBufferSize = 2048;
    std::vector<float> buffer;
    int bufferWritePos = 0;
    double sr = 44100.0;
    float envelope = 0.0f;
    float lastFrequency = 0.0f;
    juce::String lastNoteName = "--";
};

} // namespace mimo
