#pragma once

#include <juce_core/juce_core.h>

namespace mimo::audio
{

// ============================================================================
// Interfaccia Base Polimorfica per Tutti i Moduli Audio di MiMO
// ============================================================================
class NodeAudioProcessor
{
public:
    virtual ~NodeAudioProcessor() = default;

    // Reset del motore DSP alla frequenza di campionamento corrente
    virtual void reset(double sampleRate) = 0;

    // Elaborazione a blocchi di campioni stereo in-place
    virtual void processBlock(float* left, float* right, int numSamples) = 0;

    // Ricezione di impulsi / colpi (da piezo batteria, pinch dell'occhio o sequencer)
    virtual void trigger(int voiceIndex, float velocity) {}

    // Rilascio nota per strumenti con inviluppo sustain
    virtual void noteOff() {}

    // Impostazione di un parametro o modulazione CV continua (valore normalizzato 0.0 - 1.0)
    virtual void setParameter(const juce::String& paramKey, float normalizedValue) {}
};

} // namespace mimo::audio
