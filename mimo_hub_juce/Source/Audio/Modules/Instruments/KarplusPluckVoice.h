#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"
#include <vector>
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// Karplus-Strong Pluck & Kalimba Voice (ins-karplus-pluck)
// Riferimenti: Kevin Karplus & Alex Strong (1983) & Julius O. Smith III
// Sintesi per modelli fisici di corda o lamella metallica (Kalimba / Felt Chime)
// ============================================================================
class KarplusPluckVoice : public mimo::audio::NodeAudioProcessor
{
public:
    static constexpr int kMaxPolyphony = 4;
    static constexpr int kMaxDelaySamples = 4096;

    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        for (int v = 0; v < kMaxPolyphony; ++v)
        {
            voices[v].reset(sr);
        }
        rng = 987654321u;
        nextVoiceIdx = 0;
    }

    void trigger(int voiceIndex, float velocity) noexcept override
    {
        const float vel = juce::jlimit(0.1f, 1.0f, velocity);

        // Scala pentatonica o accordo naturale basato sul voiceIndex (0..7)
        // C3, D3, E3, G3, A3, C4, D4, E4
        const float scaleFreqs[8] = { 130.81f, 146.83f, 164.81f, 196.00f, 220.00f, 261.63f, 293.66f, 329.63f };
        const float baseFreq = scaleFreqs[juce::jlimit(0, 7, voiceIndex % 8)] * pitchMultiplier;

        voices[nextVoiceIdx].pluck(baseFreq, vel, brilliance, dampingDecay, rng);
        nextVoiceIdx = (nextVoiceIdx + 1) % kMaxPolyphony;
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);

        if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
        else if (paramKey == "pitch" || paramKey == "tune")
        {
            pitchMultiplier = std::pow(2.0f, (v - 0.5f) * 2.0f); // +/- 1 ottava
        }
        else if (paramKey == "brilliance" || paramKey == "tone")
        {
            brilliance = v;
        }
        else if (paramKey == "decay" || paramKey == "damping")
        {
            dampingDecay = 0.88f + v * 0.118f; // 0.88 .. 0.998 feedback
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            float smp = 0.0f;
            for (int v = 0; v < kMaxPolyphony; ++v)
            {
                if (voices[v].active)
                    smp += voices[v].getNextSample();
            }

            smp = sanitizeSample(smp * 0.75f);
            if (left != nullptr)  left[i]  = smp;
            if (right != nullptr) right[i] = smp;
        }
    }

private:
    struct SingleString
    {
        float buffer[kMaxDelaySamples];
        int writePos = 0;
        float readPos = 0.0f;
        float delayLength = 200.0f;
        float prevSample = 0.0f;
        float feedback = 0.985f;
        float lpCoeff = 0.5f;
        bool active = false;
        double sr = 48000.0;

        void reset(double sampleRate) noexcept
        {
            sr = sampleRate;
            std::fill(std::begin(buffer), std::end(buffer), 0.0f);
            writePos = 0;
            readPos = 0.0f;
            prevSample = 0.0f;
            active = false;
        }

        void pluck(float freqHz, float vel, float brill, float decayFactor, uint32_t& rngState) noexcept
        {
            const float clampedFreq = juce::jlimit(40.0f, 2200.0f, freqHz);
            delayLength = static_cast<float>(sr) / clampedFreq - 0.5f;
            delayLength = juce::jlimit(2.0f, static_cast<float>(kMaxDelaySamples - 2), delayLength);

            feedback = juce::jlimit(0.85f, 0.999f, decayFactor);
            // Più è morbido il tocco (vel bassa), più il filtro iniziale è scuro (feltro)
            lpCoeff = 0.25f + (brill * 0.45f + vel * 0.25f);

            // Eccitazione iniziale: burst di rumore sagomato sulla lunghezza della corda
            const int burstLen = std::min(static_cast<int>(delayLength), 350);
            float prevNoise = 0.0f;
            for (int i = 0; i < burstLen; ++i)
            {
                rngState = rngState * 1664525u + 1013904223u;
                const float white = (static_cast<float>(rngState & 0xFFFFu) / 32768.0f) - 1.0f;
                // Filtraggio iniziale dell'impulso
                prevNoise = prevNoise * (1.0f - lpCoeff) + white * lpCoeff;
                buffer[i] = prevNoise * vel * 0.95f;
            }
            for (int i = burstLen; i < kMaxDelaySamples; ++i)
            {
                buffer[i] = 0.0f;
            }

            writePos = burstLen;
            readPos = 0.0f;
            prevSample = 0.0f;
            active = true;
        }

        float getNextSample() noexcept
        {
            // Lettura con interpolazione lineare sulla linea di ritardo
            const int rInt = static_cast<int>(readPos);
            const float frac = readPos - static_cast<float>(rInt);
            const int rNext = (rInt + 1) % kMaxDelaySamples;

            const float current = buffer[rInt] * (1.0f - frac) + buffer[rNext] * frac;

            // Filtro di smorzamento passa-basso nel loop di feedback (media mobile a due punti)
            const float filtered = (current * 0.5f + prevSample * 0.5f) * feedback;
            prevSample = current;

            // Scrivi nel buffer circolare
            buffer[writePos] = filtered;
            writePos = (writePos + 1) % kMaxDelaySamples;

            // Avanza puntatore di lettura
            readPos += 1.0f;
            if (readPos >= delayLength)
                readPos -= delayLength;

            // Spegni se il segnale è completamente decaduto
            if (std::abs(filtered) < 0.0001f && std::abs(prevSample) < 0.0001f)
                active = false;

            return current;
        }
    };

    double sr = 48000.0;
    uint32_t rng = 987654321u;
    SingleString voices[kMaxPolyphony];
    int nextVoiceIdx = 0;

    float pitchMultiplier = 1.0f;
    float brilliance = 0.65f;
    float dampingDecay = 0.985f;
};

} // namespace mimo::dsp
