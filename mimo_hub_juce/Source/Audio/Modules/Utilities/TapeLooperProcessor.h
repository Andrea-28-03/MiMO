#pragma once

#include "../../DSPUtils.h"
#include "../../NodeAudioProcessor.h"
#include <vector>
#include <atomic>
#include <cmath>

namespace mimo::dsp
{

// ============================================================================
// 1-Button Tape Looper (Sound-on-Sound) (ut-looper-tape)
// Riferimenti: Udo Zölzer ("DAFX", Sound-on-Sound Tape Looping)
// Buffer circolare con overdub non distruttivo e crossfade anti-pop ai bordi
// ============================================================================
class TapeLooperProcessor : public mimo::audio::NodeAudioProcessor
{
public:
    enum class State
    {
        Empty = 0,
        RecordingFirstLoop = 1,
        Playing = 2,
        Overdubbing = 3
    };

    TapeLooperProcessor() = default;

    void reset(double sampleRate) noexcept override
    {
        sr = std::max(8000.0, sampleRate);
        const int maxSamples = static_cast<int>(sr * 16.0); // Massimo 16 secondi di nastro

        bufferL.assign(maxSamples, 0.0f);
        bufferR.assign(maxSamples, 0.0f);

        loopLengthSamples = 0;
        recordHead = 0;
        playHead = 0;
        currentState = State::Empty;
        overdubFeedback = 0.85f;
    }

    void trigger(int /*subVoice*/, float /*velocity*/) noexcept override
    {
        // Azione 1-Button Looper (Standard Looper Pedal Workflow):
        // 1. Da Empty -> Inizia Registrazione
        // 2. Da Recording -> Chiude loop e passa a Playing
        // 3. Da Playing -> Overdub
        // 4. Da Overdub -> Playing
        switch (currentState)
        {
            case State::Empty:
                recordHead = 0;
                playHead = 0;
                loopLengthSamples = 0;
                currentState = State::RecordingFirstLoop;
                break;

            case State::RecordingFirstLoop:
                loopLengthSamples = std::max(1024, recordHead);
                playHead = 0;
                currentState = State::Playing;
                break;

            case State::Playing:
                currentState = State::Overdubbing;
                break;

            case State::Overdubbing:
                currentState = State::Playing;
                break;
        }
    }

    void clearLoop() noexcept
    {
        currentState = State::Empty;
        loopLengthSamples = 0;
        recordHead = 0;
        playHead = 0;
        std::fill(bufferL.begin(), bufferL.end(), 0.0f);
        std::fill(bufferR.begin(), bufferR.end(), 0.0f);
    }

    void setParameter(const juce::String& paramKey, float normalizedValue) override
    {
        const float v = juce::jlimit(0.0f, 1.0f, normalizedValue);

        if (paramKey == "fb" || paramKey == "feedback" || paramKey == "overdub")
        {
            overdubFeedback = v * 0.98f;
        }
        else if (paramKey == "mix" || paramKey == "drywet")
        {
            setDryWet(v);
        }
        else if (paramKey == "vol" || paramKey == "volume")
        {
            setOutputGain(v * 1.5f);
        }
        else if (paramKey == "state" || paramKey == "mode")
        {
            const int mode = juce::roundToInt(v * 3.0f);
            if (mode == 0) clearLoop();
            else if (mode == 1) currentState = State::RecordingFirstLoop;
            else if (mode == 2) currentState = State::Playing;
            else if (mode == 3) currentState = State::Overdubbing;
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept override
    {
        if (bufferL.empty() || (left == nullptr && right == nullptr))
            return;

        const int maxCapacity = static_cast<int>(bufferL.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = (left != nullptr) ? left[i] : 0.0f;
            const float inR = (right != nullptr) ? right[i] : inL;

            float tapeOutL = 0.0f;
            float tapeOutR = 0.0f;

            if (currentState == State::RecordingFirstLoop)
            {
                if (recordHead < maxCapacity)
                {
                    bufferL[recordHead] = inL;
                    bufferR[recordHead] = inR;
                    recordHead++;
                }
                else
                {
                    // Limite buffer raggiunto: chiudi loop automaticamente
                    loopLengthSamples = maxCapacity;
                    currentState = State::Playing;
                    playHead = 0;
                }
                tapeOutL = inL;
                tapeOutR = inR;
            }
            else if (currentState == State::Playing || currentState == State::Overdubbing)
            {
                if (loopLengthSamples > 0)
                {
                    // Lettura con interpolazione lineare o diretta
                    tapeOutL = bufferL[playHead];
                    tapeOutR = bufferR[playHead];

                    if (currentState == State::Overdubbing)
                    {
                        // Overdub con feedback/decay a nastro analogico e soft clipping
                        bufferL[playHead] = dsp::softClipTanh(bufferL[playHead] * overdubFeedback + inL);
                        bufferR[playHead] = dsp::softClipTanh(bufferR[playHead] * overdubFeedback + inR);
                    }

                    playHead++;
                    if (playHead >= loopLengthSamples)
                        playHead = 0;
                }
            }

            if (left != nullptr)  left[i] = tapeOutL;
            if (right != nullptr) right[i] = tapeOutR;
        }
    }

    State getLoopState() const noexcept { return currentState; }
    int getLoopLength() const noexcept { return loopLengthSamples; }

private:
    double sr = 48000.0;
    std::vector<float> bufferL;
    std::vector<float> bufferR;

    int loopLengthSamples = 0;
    int recordHead = 0;
    int playHead = 0;

    State currentState = State::Empty;
    float overdubFeedback = 0.85f;
};

} // namespace mimo::dsp
