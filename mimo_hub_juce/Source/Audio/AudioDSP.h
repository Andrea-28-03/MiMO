#pragma once

// ============================================================================
// MiMO Audio DSP - Umbrella Header
// Include modulare di tutti i processori DSP ed effetti dell'ecosistema MiMO
// ============================================================================

#include "DSPUtils.h"
#include "NodeAudioProcessor.h"

// Moduli Strumenti (Instruments)
#include "Modules/Instruments/AnalogDrumVoice.h"
#include "Modules/Instruments/MelodicSynthVoice.h"

// Moduli Effetti (Effects)
#include "Modules/Effects/SVFFilter.h"
#include "Modules/Effects/StereoTapeDelay.h"
#include "Modules/Effects/LushReverb.h"
