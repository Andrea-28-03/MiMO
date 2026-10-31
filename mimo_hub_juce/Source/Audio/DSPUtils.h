#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace mimo::dsp
{

// ============================================================================
// Utilità Numeriche & Anti-Denormal (Will Pirkle & Vadim Zavalishin)
// ============================================================================
inline float sanitizeSample(float v) noexcept
{
    if (std::isnan(v) || std::isinf(v))
        return 0.0f;
    if (std::abs(v) < 1.0e-15f)
        return 0.0f; // Flush to zero (evita rallentamenti denormalizzati)
    return juce::jlimit(-3.0f, 3.0f, v);
}

inline float softClipTanh(float x) noexcept
{
    // Approssimazione veloce di tanh per saturazione analogica (Udo Zölzer Cap. 4)
    if (x <= -3.0f) return -1.0f;
    if (x >= 3.0f)  return 1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// ============================================================================
// Generatore Inviluppo ADSR Analog-Modeled (Will Pirkle, Cap. 5)
// ============================================================================
class AnalogADSR
{
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    void reset(double sampleRate) noexcept
    {
        sr = std::max(8000.0, sampleRate);
        state = State::Idle;
        val = 0.0f;
    }

    void setParameters(float attackSec, float decaySec, float sustainLvl, float releaseSec) noexcept
    {
        attCoeff = std::exp(-1.0f / (std::max(0.001f, attackSec) * static_cast<float>(sr)));
        decCoeff = std::exp(-1.0f / (std::max(0.005f, decaySec) * static_cast<float>(sr)));
        susLevel = juce::jlimit(0.0f, 1.0f, sustainLvl);
        relCoeff = std::exp(-1.0f / (std::max(0.005f, releaseSec) * static_cast<float>(sr)));
    }

    void noteOn(float velocity = 1.0f) noexcept
    {
        vel = juce::jlimit(0.1f, 1.0f, velocity);
        state = State::Attack;
    }

    void noteOff() noexcept
    {
        if (state != State::Idle)
            state = State::Release;
    }

    float getNextSample() noexcept
    {
        switch (state)
        {
            case State::Attack:
                val = 1.35f - (1.35f - val) * attCoeff;
                if (val >= 1.0f)
                {
                    val = 1.0f;
                    state = State::Decay;
                }
                break;
            case State::Decay:
                val = susLevel + (val - susLevel) * decCoeff;
                if (std::abs(val - susLevel) < 0.001f)
                {
                    val = susLevel;
                    state = State::Sustain;
                }
                break;
            case State::Sustain:
                val = susLevel;
                break;
            case State::Release:
                val = val * relCoeff;
                if (val <= 0.0005f)
                {
                    val = 0.0f;
                    state = State::Idle;
                }
                break;
            case State::Idle:
            default:
                val = 0.0f;
                break;
        }
        return val * vel;
    }

    bool isActive() const noexcept { return state != State::Idle; }

private:
    double sr = 48000.0;
    State state = State::Idle;
    float val = 0.0f;
    float vel = 1.0f;
    float attCoeff = 0.99f;
    float decCoeff = 0.999f;
    float susLevel = 0.7f;
    float relCoeff = 0.999f;
};

// ============================================================================
// Utilità PolyBLEP Anti-Aliasing (Vadim Zavalishin & Will Pirkle)
// ============================================================================
inline float polyBlep(float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// ============================================================================
// OnePoleSmoother (Filtro passa-basso 1° ordine per parametri continui/pitch)
// ============================================================================
class OnePoleSmoother
{
public:
    void reset(double sampleRate, float timeSec) noexcept
    {
        coeff = std::exp(-1.0f / (std::max(0.0005f, timeSec) * static_cast<float>(sampleRate)));
        val = 0.0f;
        target = 0.0f;
    }
    void setTarget(float t) noexcept { target = t; }
    void setCurrentAndTarget(float v) noexcept { val = v; target = v; }
    float getNextValue() noexcept
    {
        val = target + (val - target) * coeff;
        return val;
    }
    float getCurrentValue() const noexcept { return val; }
private:
    float coeff = 0.99f;
    float val = 0.0f;
    float target = 0.0f;
};

} // namespace mimo::dsp
