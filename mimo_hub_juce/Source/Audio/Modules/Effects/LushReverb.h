#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <vector>

namespace mimo::dsp
{

// ============================================================================
// Effetto: Riverbero Spaziale Diffuso (Standard V2)
// Riferimenti: Julius O. Smith III & Schroeder-Moorer
// ============================================================================
class LushReverb : public mimo::audio::NodeAudioProcessor
{
public:
    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        const float scale = static_cast<float>(sr / 44100.0);

        // 4 Comb delays accordati su numeri primi per densità priva di risonanze metalliche
        const int cDelays[4] = {
            static_cast<int>(1116 * scale),
            static_cast<int>(1188 * scale),
            static_cast<int>(1277 * scale),
            static_cast<int>(1356 * scale)
        };
        for (int i = 0; i < 4; ++i)
        {
            combBuffers[i].assign(std::max(16, cDelays[i]), 0.0f);
            combPos[i] = 0;
            combDamp[i] = 0.0f;
        }

        // 2 Allpass filters in cascata per diffusione e decorrelazione stereo
        const int aDelays[2] = {
            static_cast<int>(225 * scale),
            static_cast<int>(556 * scale)
        };
        for (int i = 0; i < 2; ++i)
        {
            allpassBuffers[i].assign(std::max(16, aDelays[i]), 0.0f);
            allpassPos[i] = 0;
        }

        setDryWet(0.35f);
        setOutputGain(1.0f);
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);
        if (paramKey == "room" || paramKey == "room_size" || paramKey == "reverb_room")
        {
            roomSize = 0.70f + v * 0.24f; // 0.70 .. 0.94
        }
        else if (paramKey == "damp" || paramKey == "damping")
        {
            damp = 0.05f + v * 0.80f;
        }
        else if (paramKey == "mix" || paramKey == "drywet" || paramKey == "reverb_mix")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
    }

    void setParameters(float roomSizePct, float dampingPct, float wetDryPct) noexcept
    {
        roomSize = juce::jlimit(0.1f, 0.94f, 0.70f + (roomSizePct * 0.01f) * 0.24f);
        damp = juce::jlimit(0.05f, 0.85f, dampingPct * 0.01f);
        setDryWet(wetDryPct * 0.01f);
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (left == nullptr || numSamples <= 0 || combBuffers[0].empty())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = (right != nullptr) ? right[i] : inL;
            const float monoIn = (inL + inR) * 0.5f;

            float combSum = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                auto& buf = combBuffers[c];
                const int sz = static_cast<int>(buf.size());
                const float outVal = buf[combPos[c]];
                combDamp[c] = outVal * (1.0f - damp) + combDamp[c] * damp;
                buf[combPos[c]] = monoIn + combDamp[c] * roomSize;
                combPos[c] = (combPos[c] + 1) % sz;
                combSum += outVal;
            }

            float diff = combSum * 0.25f;
            for (int a = 0; a < 2; ++a)
            {
                auto& buf = allpassBuffers[a];
                const int sz = static_cast<int>(buf.size());
                const float bufVal = buf[allpassPos[a]];
                const float pass = -diff + bufVal;
                buf[allpassPos[a]] = diff + bufVal * 0.5f;
                allpassPos[a] = (allpassPos[a] + 1) % sz;
                diff = pass;
            }

            // Uscita wet pura decorrelata (leggero sfasamento stereo naturale)
            left[i] = sanitizeSample(diff * 1.35f);
            if (right != nullptr)
                right[i] = sanitizeSample((diff + combBuffers[1][combPos[1]] * 0.15f) * 1.35f);
        }
    }

private:
    double sr = 48000.0;
    std::vector<float> combBuffers[4];
    int combPos[4] = { 0, 0, 0, 0 };
    float combDamp[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    std::vector<float> allpassBuffers[2];
    int allpassPos[2] = { 0, 0 };

    float roomSize = 0.82f;
    float damp = 0.35f;
};

} // namespace mimo::dsp
