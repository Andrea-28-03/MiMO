#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <vector>

namespace mimo::dsp
{

// ============================================================================
// Effetto: Stereo Tape Delay con Damping (Standard V2)
// Riferimenti: Udo Zölzer ("DAFX", Cap. 2)
// ============================================================================
class StereoTapeDelay : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 2.2); // Fino a 2.2 secondi
        bufferL.assign(maxDelaySamples, 0.0f);
        bufferR.assign(maxDelaySamples, 0.0f);
        writePos = 0;
        dampL = 0.0f;
        dampR = 0.0f;
        delayTimeSamples = static_cast<float>(sr) * 0.32f; // ~320 ms
        setDryWet(0.35f);
        setOutputGain(1.0f);
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "time" || paramKey == "delay_time")
        {
            // 20 ms .. 1200 ms
            const float timeMs = 20.0f + v * 1180.0f;
            delayTimeSamples = juce::jlimit(1.0f, static_cast<float>(bufferL.size() - 2), timeMs * 0.001f * static_cast<float>(sr));
        }
        else if (paramKey == "fb" || paramKey == "feedback" || paramKey == "delay_feedback")
        {
            feedback = juce::jlimit(0.0f, 0.94f, v * 0.94f);
        }
        else if (paramKey == "tone" || paramKey == "damping")
        {
            const float dampFreq = 300.0f + v * 15000.0f;
            dampCoeff = std::exp(-juce::MathConstants<float>::twoPi * dampFreq / static_cast<float>(sr));
        }
        else if (paramKey == "mix" || paramKey == "drywet" || paramKey == "wet")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
    }

    void setParameters(float timeMs, float feedbackPct, float dampingCutoffHz, float wetDryPct) noexcept
    {
        delayTimeSamples = juce::jlimit(1.0f, static_cast<float>(bufferL.size() - 2), timeMs * 0.001f * static_cast<float>(sr));
        feedback = juce::jlimit(0.0f, 0.94f, feedbackPct * 0.01f);
        setDryWet(wetDryPct * 0.01f);

        // Coefficiente di smorzamento nastro a un polo
        const float dampFreq = juce::jlimit(300.0f, 18000.0f, dampingCutoffHz);
        dampCoeff = std::exp(-juce::MathConstants<float>::twoPi * dampFreq / static_cast<float>(sr));
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (left == nullptr || numSamples <= 0 || bufferL.empty())
            return;

        const int bufSize = static_cast<int>(bufferL.size());
        const float offsetR = delayTimeSamples * 1.333f; // Leggero sfasamento stereo ping-pong

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = (right != nullptr) ? right[i] : inL;

            // Lettura interpolata dal buffer circolare
            float readPosL = static_cast<float>(writePos) - delayTimeSamples;
            while (readPosL < 0.0f) readPosL += static_cast<float>(bufSize);
            const int idxL0 = static_cast<int>(readPosL);
            const int idxL1 = (idxL0 + 1) % bufSize;
            const float fracL = readPosL - static_cast<float>(idxL0);
            const float delayedL = bufferL[idxL0] * (1.0f - fracL) + bufferL[idxL1] * fracL;

            float readPosR = static_cast<float>(writePos) - offsetR;
            while (readPosR < 0.0f) readPosR += static_cast<float>(bufSize);
            const int idxR0 = static_cast<int>(readPosR);
            const int idxR1 = (idxR0 + 1) % bufSize;
            const float fracR = readPosR - static_cast<float>(idxR0);
            const float delayedR = bufferR[idxR0] * (1.0f - fracR) + bufferR[idxR1] * fracR;

            // Smorzamento alte frequenze (Warm Tape Rolloff)
            dampL = delayedL * (1.0f - dampCoeff) + dampL * dampCoeff;
            dampR = delayedR * (1.0f - dampCoeff) + dampR * dampCoeff;

            bufferL[writePos] = sanitizeSample(inL + dampL * feedback);
            bufferR[writePos] = sanitizeSample(inR + dampR * feedback);

            writePos = (writePos + 1) % bufSize;

            left[i] = sanitizeSample(delayedL);
            if (right != nullptr)
                right[i] = sanitizeSample(delayedR);
        }
    }

private:
    double sr = 48000.0;
    std::vector<float> bufferL;
    std::vector<float> bufferR;
    int writePos = 0;

    float delayTimeSamples = 16000.0f;
    float feedback = 0.45f;
    float dampCoeff = 0.5f;
    float dampL = 0.0f;
    float dampR = 0.0f;
};

} // namespace mimo::dsp
