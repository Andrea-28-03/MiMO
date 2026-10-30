#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
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
                val = 1.35f - (1.35f - val) * attCoeff; // Target sovradimensionato per curva d'attacco rapida
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
// Generatore Percussivo "Analog Drum Kit"
// Riferimenti: Émilie Gillet (Plaits) & Will Pirkle
// Include 4 voci: Cassa (Kick), Rullante (Snare), Hi-Hat (Hat) e Tom/Kalimba
// ============================================================================
class AnalogDrumKit
{
public:
    void reset(double sampleRate) noexcept
    {
        sr = std::max(8000.0, sampleRate);
        filterKick.reset(sr);
        filterSnare.reset(sr);
        filterHat.reset(sr);
        filterHat.setParameters(7500.0f, 1.2f, SVF_ZDF::Mode::HighPass);
        rng = 123456789u;

        kickEnv = 0.0f;
        kickPitch = 0.0f;
        kickPhase = 0.0f;

        snareEnv = 0.0f;
        snarePhase = 0.0f;

        hatEnv = 0.0f;
        hatPhase = 0.0f;

        tomEnv = 0.0f;
        tomPhase = 0.0f;
    }

    void trigger(int subVoice, float velocity) noexcept
    {
        const float v = juce::jlimit(0.15f, 1.0f, velocity);
        switch (subVoice)
        {
            case 0: // Cassa (Kick)
                kickEnv = v;
                kickPitch = 1.0f;
                kickPhase = 0.0f;
                break;
            case 1: // Rullante (Snare)
                snareEnv = v;
                snarePhase = 0.0f;
                break;
            case 2: // Hi-Hat
                hatEnv = v;
                break;
            case 3: // Tom / Percussione
            default:
                tomEnv = v;
                tomPhase = 0.0f;
                break;
        }
    }

    void renderNextBlock(float* left, float* right, int numSamples, float gain = 1.0f) noexcept
    {
        if (kickEnv <= 0.0005f && snareEnv <= 0.0005f && hatEnv <= 0.0005f && tomEnv <= 0.0005f)
            return;

        const float invSr = 1.0f / static_cast<float>(sr);
        const float kickDecay = std::exp(-invSr * 2.8f);       // ~350ms
        const float kickPitchDecay = std::exp(-invSr * 28.0f); // ~35ms drop
        const float snareDecay = std::exp(-invSr * 6.5f);      // ~150ms
        const float hatDecay = std::exp(-invSr * 22.0f);       // ~45ms
        const float tomDecay = std::exp(-invSr * 3.8f);        // ~260ms

        for (int i = 0; i < numSamples; ++i)
        {
            float smp = 0.0f;

            // --- 1. KICK ---
            if (kickEnv > 0.0005f)
            {
                const float freq = 42.0f + kickPitch * 115.0f; // 157 Hz -> 42 Hz
                kickPhase += freq * invSr;
                if (kickPhase >= 1.0f) kickPhase -= 1.0f;

                const float click = (kickPitch > 0.6f) ? (kickPitch * 0.25f) : 0.0f;
                const float rawKick = std::sin(kickPhase * juce::MathConstants<float>::twoPi) + click;
                smp += softClipTanh(rawKick * 1.3f) * kickEnv * 1.1f;

                kickPitch *= kickPitchDecay;
                kickEnv *= kickDecay;
            }

            // --- 2. SNARE ---
            if (snareEnv > 0.0005f)
            {
                snarePhase += 175.0f * invSr;
                if (snarePhase >= 1.0f) snarePhase -= 1.0f;
                const float body = std::sin(snarePhase * juce::MathConstants<float>::twoPi) * 0.45f;

                rng = rng * 1664525u + 1013904223u;
                const float white = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
                filterSnare.setParameters(1900.0f, 1.8f, SVF_ZDF::Mode::BandPass);
                const float rattle = filterSnare.process(white) * 1.1f;

                smp += (body + rattle) * snareEnv * 0.90f;
                snareEnv *= snareDecay;
            }

            // --- 3. HI-HAT ---
            if (hatEnv > 0.0005f)
            {
                // Cluster inarmonico a 6 oscillatori metallici (stile 808/Plaits)
                hatPhase += 380.0f * invSr;
                if (hatPhase >= 1.0f) hatPhase -= 1.0f;

                float metal = 0.0f;
                metal += (std::fmod(hatPhase * 1.00f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.34f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.78f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 2.24f, 1.0f) > 0.5f ? 1.0f : -1.0f);

                const float filteredHat = filterHat.process(metal * 0.25f);
                smp += filteredHat * hatEnv * 0.70f;
                hatEnv *= hatDecay;
            }

            // --- 4. TOM / KALIMBA ---
            if (tomEnv > 0.0005f)
            {
                tomPhase += 120.0f * invSr;
                if (tomPhase >= 1.0f) tomPhase -= 1.0f;
                const float tomWave = std::sin(tomPhase * juce::MathConstants<float>::twoPi)
                                    + 0.25f * std::sin(tomPhase * juce::MathConstants<float>::twoPi * 2.0f);
                smp += tomWave * tomEnv * 0.85f;
                tomEnv *= tomDecay;
            }

            smp = sanitizeSample(smp * gain);
            if (left != nullptr)  left[i]  += smp;
            if (right != nullptr) right[i] += smp;
        }
    }

private:
    double sr = 48000.0;
    uint32_t rng = 123456789u;

    SVF_ZDF filterKick;
    SVF_ZDF filterSnare;
    SVF_ZDF filterHat;

    float kickEnv = 0.0f;
    float kickPitch = 0.0f;
    float kickPhase = 0.0f;

    float snareEnv = 0.0f;
    float snarePhase = 0.0f;

    float hatEnv = 0.0f;
    float hatPhase = 0.0f;

    float tomEnv = 0.0f;
    float tomPhase = 0.0f;
};

// ============================================================================
// Sintetizzatore Virtual Analog (Melodic Synth & Sub Bass)
// Riferimenti: Vadim Zavalishin (ZDF Filter) & Will Pirkle (PolyBLEP)
// ============================================================================
class MelodicSynthVoice
{
public:
    void reset(double sampleRate) noexcept
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

    void triggerNote(float frequencyHz, float velocity, bool isSubBass = false) noexcept
    {
        targetFreq = juce::jlimit(20.0f, 4000.0f, frequencyHz);
        if (!isSub)
            freq = targetFreq;
        isSub = isSubBass;
        subPitchEnv = isSub ? 1.0f : 0.0f;
        adsr.noteOn(velocity);
    }

    void noteOff() noexcept
    {
        adsr.noteOff();
    }

    void setMacroControls(float cutoffHz, float resonance, float timbreMorph) noexcept
    {
        macroCutoff = cutoffHz;
        macroReso = resonance;
        morph = juce::jlimit(0.0f, 1.0f, timbreMorph);
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
};

// ============================================================================
// Effetto: Stereo Tape Delay con Damping
// Riferimenti: Udo Zölzer ("DAFX", Cap. 2)
// ============================================================================
class StereoTapeDelay
{
public:
    void reset(double sampleRate) noexcept
    {
        sr = std::max(8000.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 2.2); // Fino a 2.2 secondi
        bufferL.assign(maxDelaySamples, 0.0f);
        bufferR.assign(maxDelaySamples, 0.0f);
        writePos = 0;
        dampL = 0.0f;
        dampR = 0.0f;
    }

    void setParameters(float timeMs, float feedbackPct, float dampingCutoffHz, float wetDryPct) noexcept
    {
        delayTimeSamples = juce::jlimit(1.0f, static_cast<float>(bufferL.size() - 2), timeMs * 0.001f * static_cast<float>(sr));
        feedback = juce::jlimit(0.0f, 0.94f, feedbackPct * 0.01f);
        wetMix = juce::jlimit(0.0f, 1.0f, wetDryPct * 0.01f);

        // Coefficiente di smorzamento nastro a un polo
        const float dampFreq = juce::jlimit(300.0f, 18000.0f, dampingCutoffHz);
        dampCoeff = std::exp(-juce::MathConstants<float>::twoPi * dampFreq / static_cast<float>(sr));
    }

    void processBlock(float* left, float* right, int numSamples) noexcept
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

            left[i] = sanitizeSample(inL * (1.0f - wetMix) + delayedL * wetMix);
            if (right != nullptr)
                right[i] = sanitizeSample(inR * (1.0f - wetMix) + delayedR * wetMix);
        }
    }

private:
    double sr = 48000.0;
    std::vector<float> bufferL;
    std::vector<float> bufferR;
    int writePos = 0;

    float delayTimeSamples = 18000.0f;
    float feedback = 0.45f;
    float wetMix = 0.35f;
    float dampCoeff = 0.35f;
    float dampL = 0.0f;
    float dampR = 0.0f;
};

// ============================================================================
// Effetto: Riverbero Spaziale Diffuso (Comb & Allpass)
// Riferimenti: Julius O. Smith III & Schroeder-Moorer
// ============================================================================
class LushReverb
{
public:
    void reset(double sampleRate) noexcept
    {
        sr = std::max(8000.0, sampleRate);
        const float scale = static_cast<float>(sr / 44100.0);

        // 4 Comb delays accordati su numeri primi per densità priva di battimenti metallici
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
    }

    void setParameters(float roomSizePct, float dampingPct, float wetDryPct) noexcept
    {
        roomSize = juce::jlimit(0.1f, 0.94f, 0.70f + (roomSizePct * 0.01f) * 0.24f);
        damp = juce::jlimit(0.05f, 0.85f, dampingPct * 0.01f);
        wetMix = juce::jlimit(0.0f, 1.0f, wetDryPct * 0.01f);
    }

    void processBlock(float* left, float* right, int numSamples) noexcept
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

            left[i] = sanitizeSample(inL * (1.0f - wetMix) + diff * wetMix * 1.2f);
            if (right != nullptr)
                right[i] = sanitizeSample(inR * (1.0f - wetMix) + diff * wetMix * 1.2f);
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
    float wetMix = 0.30f;
};

} // namespace mimo::dsp
