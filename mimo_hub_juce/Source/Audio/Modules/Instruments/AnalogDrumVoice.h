#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include "../Effects/SVFFilter.h"
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// Generatore Percussivo "Percussioni" (ex Analog Drum Kit) (ins-drum-analog)
// Riferimenti: Émilie Gillet (Mutable Plaits/Anushri) & Roland TR-808/909
// 4 voci discrete (Cassa, Rullante, Charleston, Tom) con sound design granulare
// ============================================================================
class AnalogDrumKit : public mimo::audio::NodeAudioProcessor
{
public:
    enum class Preset
    {
        Roland808 = 0,
        AcousticWarm = 1,
        ElectroTight = 2,
        LofiDust = 3,
        Custom = 4
    };

    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        invSr = 1.0f / static_cast<float>(sr);

        filterKick.reset(sr);
        filterSnare.reset(sr);
        filterHat.reset(sr);
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

        loadPreset(currentPreset);
    }

    void loadPreset(Preset p) noexcept
    {
        currentPreset = p;
        switch (p)
        {
            case Preset::Roland808:
                kickPitchBase = 44.0f;
                kickPunchAmount = 0.85f;
                kickDecayTime = 0.85f; // Sub profondo 808
                snareToneHz = 180.0f;
                snareSnappyAmount = 1.15f;
                snareDecayTime = 0.18f;
                hatCutoffHz = 8200.0f;
                hatSizzleQ = 1.4f;
                hatDecayTime = 0.045f;
                tomPitchHz = 82.0f;
                tomDecayTime = 0.45f;
                drive = 0.20f;
                break;

            case Preset::AcousticWarm:
                kickPitchBase = 58.0f;
                kickPunchAmount = 1.35f;
                kickDecayTime = 0.28f; // Cassa punchy acustica
                snareToneHz = 215.0f;
                snareSnappyAmount = 0.85f;
                snareDecayTime = 0.22f;
                hatCutoffHz = 6200.0f;
                hatSizzleQ = 1.1f;
                hatDecayTime = 0.09f;
                tomPitchHz = 130.0f;
                tomDecayTime = 0.32f;
                drive = 0.10f;
                break;

            case Preset::ElectroTight:
                kickPitchBase = 68.0f;
                kickPunchAmount = 1.70f;
                kickDecayTime = 0.16f; // Click club stretto
                snareToneHz = 245.0f;
                snareSnappyAmount = 1.55f;
                snareDecayTime = 0.12f;
                hatCutoffHz = 9800.0f;
                hatSizzleQ = 2.0f;
                hatDecayTime = 0.03f;
                tomPitchHz = 160.0f;
                tomDecayTime = 0.18f;
                drive = 0.42f;
                break;

            case Preset::LofiDust:
                kickPitchBase = 46.0f;
                kickPunchAmount = 0.55f;
                kickDecayTime = 0.38f; // Cassa soffice e ovattata
                snareToneHz = 160.0f;
                snareSnappyAmount = 0.65f;
                snareDecayTime = 0.28f;
                hatCutoffHz = 4800.0f;
                hatSizzleQ = 0.85f;
                hatDecayTime = 0.065f;
                tomPitchHz = 95.0f;
                tomDecayTime = 0.50f;
                drive = 0.30f;
                break;

            case Preset::Custom:
            default:
                break;
        }
        updateCoefficients();
    }

    void trigger(int subVoice, float velocity) noexcept override
    {
        const float v = juce::jlimit(0.15f, 1.0f, velocity);
        switch (subVoice % 4)
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

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);

        // Parametri Macro Generali
        if (paramKey == "vol" || paramKey == "volume")
        {
            masterVol = v * 1.5f;
        }
        else if (paramKey == "pitch")
        {
            globalPitchMult = std::pow(2.0f, (v - 0.5f) * 2.0f); // -1 ottava .. +1 ottava
        }
        else if (paramKey == "decay")
        {
            globalDecayMult = 0.25f + v * 2.25f; // 0.25x .. 2.5x
            updateCoefficients();
        }
        else if (paramKey == "drive" || paramKey == "dist")
        {
            drive = v * 0.90f;
        }
        else if (paramKey == "preset")
        {
            const int pIdx = juce::roundToInt(v * 4.0f);
            loadPreset(static_cast<Preset>(juce::jlimit(0, 4, pIdx)));
        }
        // Parametri Dettagliati di Arrangiamento Cassa
        else if (paramKey == "kick_pitch")
        {
            kickPitchBase = 35.0f + v * 85.0f; // 35 .. 120 Hz
        }
        else if (paramKey == "kick_punch")
        {
            kickPunchAmount = 0.2f + v * 2.0f;
        }
        else if (paramKey == "kick_decay")
        {
            kickDecayTime = 0.05f + v * 1.35f; // 50ms .. 1400ms
            updateCoefficients();
        }
        // Parametri Dettagliati Rullante
        else if (paramKey == "snare_tone")
        {
            snareToneHz = 110.0f + v * 270.0f; // 110 .. 380 Hz
        }
        else if (paramKey == "snare_snappy")
        {
            snareSnappyAmount = v * 1.8f;
        }
        else if (paramKey == "snare_decay")
        {
            snareDecayTime = 0.04f + v * 0.65f; // 40ms .. 690ms
            updateCoefficients();
        }
        // Parametri Dettagliati Hi-Hat
        else if (paramKey == "hat_cutoff")
        {
            hatCutoffHz = 3200.0f + v * 9000.0f; // 3200 .. 12200 Hz
        }
        else if (paramKey == "hat_sizzle")
        {
            hatSizzleQ = 0.6f + v * 3.2f;
        }
        else if (paramKey == "hat_decay")
        {
            hatDecayTime = 0.02f + v * 0.45f; // 20ms .. 470ms
            updateCoefficients();
        }
        // Parametri Dettagliati Tom
        else if (paramKey == "tom_pitch")
        {
            tomPitchHz = 60.0f + v * 200.0f; // 60 .. 260 Hz
        }
        else if (paramKey == "tom_decay")
        {
            tomDecayTime = 0.08f + v * 1.10f; // 80ms .. 1180ms
            updateCoefficients();
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        renderNextBlock(left, right, numSamples, masterVol);
    }

    void renderNextBlock(float* left, float* right, int numSamples, float gain = 1.0f) noexcept
    {
        if (kickEnv <= 0.0005f && snareEnv <= 0.0005f && hatEnv <= 0.0005f && tomEnv <= 0.0005f)
            return;

        filterHat.setParameters(hatCutoffHz * globalPitchMult, hatSizzleQ, SVF_ZDF::Mode::HighPass);

        for (int i = 0; i < numSamples; ++i)
        {
            float smp = 0.0f;

            // --- 1. CASSA (KICK) ---
            if (kickEnv > 0.0005f)
            {
                const float instantPitch = (kickPitchBase + kickPitch * 115.0f * kickPunchAmount) * globalPitchMult;
                kickPhase += instantPitch * invSr;
                if (kickPhase >= 1.0f) kickPhase -= 1.0f;

                // Click transient iniziale stile 909
                const float click = (kickPitch > 0.7f) ? (kickPitch * 0.35f * kickPunchAmount) : 0.0f;
                const float rawKick = std::sin(kickPhase * juce::MathConstants<float>::twoPi) + click;
                smp += softClipTanh(rawKick * (1.2f + drive * 1.5f)) * kickEnv * 1.15f;

                kickPitch *= kickPitchRate;
                kickEnv *= kickEnvCoeff;
            }

            // --- 2. RULLANTE (SNARE) ---
            if (snareEnv > 0.0005f)
            {
                snarePhase += (snareToneHz * globalPitchMult) * invSr;
                if (snarePhase >= 1.0f) snarePhase -= 1.0f;
                const float body = std::sin(snarePhase * juce::MathConstants<float>::twoPi) * 0.45f;

                // Cordiera / Noise modellato
                rng = rng * 1664525u + 1013904223u;
                const float white = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
                filterSnare.setParameters(snareToneHz * 7.5f * globalPitchMult, 1.6f, SVF_ZDF::Mode::BandPass);
                const float rattle = filterSnare.process(white) * snareSnappyAmount;

                smp += (body + rattle) * snareEnv * 0.95f;
                snareEnv *= snareEnvCoeff;
            }

            // --- 3. CHARLESTON (HI-HAT) ---
            if (hatEnv > 0.0005f)
            {
                hatPhase += 385.0f * invSr;
                if (hatPhase >= 1.0f) hatPhase -= 1.0f;

                // 6 Oscillatori ad onda quadra disarmonici per timbro metallico
                float metal = 0.0f;
                metal += (std::fmod(hatPhase * 1.00f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.34f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 1.78f, 1.0f) > 0.5f ? 1.0f : -1.0f);
                metal += (std::fmod(hatPhase * 2.24f, 1.0f) > 0.5f ? 1.0f : -1.0f);

                const float filteredHat = filterHat.process(metal * 0.25f);
                smp += filteredHat * hatEnv * 0.75f;
                hatEnv *= hatEnvCoeff;
            }

            // --- 4. TOM / KALIMBA ---
            if (tomEnv > 0.0005f)
            {
                tomPhase += (tomPitchHz * globalPitchMult) * invSr;
                if (tomPhase >= 1.0f) tomPhase -= 1.0f;
                const float tomWave = std::sin(tomPhase * juce::MathConstants<float>::twoPi)
                                    + 0.22f * std::sin(tomPhase * juce::MathConstants<float>::twoPi * 2.0f);
                smp += tomWave * tomEnv * 0.85f;
                tomEnv *= tomEnvCoeff;
            }

            // Saturazione analogica complessiva (Drive)
            if (drive > 0.02f)
                smp = softClipTanh(smp * (1.0f + drive * 1.8f));

            smp = sanitizeSample(smp * gain);
            if (left != nullptr)  left[i]  += smp;
            if (right != nullptr) right[i] += smp;
        }
    }

    Preset getPreset() const noexcept { return currentPreset; }

private:
    void updateCoefficients() noexcept
    {
        const float effKickDecay = std::max(0.02f, kickDecayTime * globalDecayMult);
        const float effSnareDecay = std::max(0.02f, snareDecayTime * globalDecayMult);
        const float effHatDecay = std::max(0.01f, hatDecayTime * globalDecayMult);
        const float effTomDecay = std::max(0.03f, tomDecayTime * globalDecayMult);

        kickEnvCoeff = std::exp(-invSr / effKickDecay);
        kickPitchRate = std::exp(-invSr * 28.0f);
        snareEnvCoeff = std::exp(-invSr / effSnareDecay);
        hatEnvCoeff = std::exp(-invSr / effHatDecay);
        tomEnvCoeff = std::exp(-invSr / effTomDecay);
    }

    double sr = 48000.0;
    float invSr = 1.0f / 48000.0f;
    uint32_t rng = 123456789u;

    SVF_ZDF filterKick;
    SVF_ZDF filterSnare;
    SVF_ZDF filterHat;

    // Inviluppi e Fasi
    float kickEnv = 0.0f;
    float kickPitch = 0.0f;
    float kickPhase = 0.0f;

    float snareEnv = 0.0f;
    float snarePhase = 0.0f;

    float hatEnv = 0.0f;
    float hatPhase = 0.0f;

    float tomEnv = 0.0f;
    float tomPhase = 0.0f;

    // Coefficienti calcolati
    float kickEnvCoeff = 0.999f;
    float kickPitchRate = 0.995f;
    float snareEnvCoeff = 0.998f;
    float hatEnvCoeff = 0.995f;
    float tomEnvCoeff = 0.999f;

    // Parametri Specifici Voci
    Preset currentPreset = Preset::Roland808;

    float kickPitchBase = 44.0f;
    float kickPunchAmount = 0.85f;
    float kickDecayTime = 0.85f;

    float snareToneHz = 180.0f;
    float snareSnappyAmount = 1.15f;
    float snareDecayTime = 0.18f;

    float hatCutoffHz = 8200.0f;
    float hatSizzleQ = 1.4f;
    float hatDecayTime = 0.045f;

    float tomPitchHz = 82.0f;
    float tomDecayTime = 0.45f;

    // Macro Globali
    float masterVol = 1.0f;
    float globalPitchMult = 1.0f;
    float globalDecayMult = 1.0f;
    float drive = 0.20f;
};

} // namespace mimo::dsp
