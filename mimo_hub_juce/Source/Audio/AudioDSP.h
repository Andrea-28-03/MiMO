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
#include "Modules/Instruments/Theremin3DVoice.h"
#include "Modules/Instruments/KarplusPluckVoice.h"

// Moduli Effetti (Effects)
#include "Modules/Effects/SVFFilter.h"
#include "Modules/Effects/SoftSaturator.h"
#include "Modules/Effects/StereoTapeDelay.h"
#include "Modules/Effects/LushReverb.h"

// Moduli Modulatori & Utility (Utilities & CV)
#include "Modules/Utilities/ScaleLock.h"
#include "Modules/Utilities/EnvelopeFollower.h"
#include "Modules/Utilities/TapeLooperProcessor.h"
