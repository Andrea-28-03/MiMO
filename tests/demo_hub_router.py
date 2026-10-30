#!/usr/bin/env python3
"""
MiMO - Demo Interattiva Desktop per MiMO Hub (@AUDIO / @HUB / @COMMS)
Consente di collaudare su PC il funzionamento del "Doppio Binario":
  1. Modalità Neofiti (sfizz + Faust DSP)
  2. Modalità Pro (Bridge DAW MIDI CC/Note + OSC)
"""

import sys
import os
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from hub_backend.audio_router import (
    MiMOAudioRouter,
    HubOperatingMode,
    EyeGesture3D,
    DrumTriggerEvent
)


def run_interactive_demo():
    print("=" * 65)
    print("   MiMO HUB - DEMO INTERATTIVA DOPPIO BINARIO (@AUDIO / @HUB)   ")
    print("=" * 65)
    
    router = MiMOAudioRouter(mode=HubOperatingMode.INTERNAL_ENGINE)
    
    print("\n[*] COMANDI DISPONIBILI:")
    print("  [1] Attiva BINARIO 1: Neofiti / Stand-Alone (sfizz + Faust DSP)")
    print("  [2] Attiva BINARIO 2: Professionisti / DAW Bypass (MIDI & OSC)")
    print("  [D] Simula Colpo Batteria (Pico Drum - Pad 0 con velocity 110)")
    print("  [G] Simula Gesto 3D Modulo Occhio (X=0.4, Y=0.85, Z=0.60)")
    print("  [S] Salva stato STUDIO su Scena LIVE 1")
    print("  [C] Simula Cambio Scena LIVE da pedaliera (Scena 3)")
    print("  [E] Simula Press & Turn Effetti dalla Rock Pi (+5% Reverb)")
    print("  [Q] Esci dalla demo\n")

    while True:
        try:
            cmd = input(f"[{router.mode.value}] Inserisci comando > ").strip().upper()
        except (KeyboardInterrupt, EOFError):
            break

        if cmd == "1":
            router.set_operating_mode(HubOperatingMode.INTERNAL_ENGINE)
        elif cmd == "2":
            router.set_operating_mode(HubOperatingMode.PRO_DAW_BRIDGE)
        elif cmd == "D":
            print("  -> Invio colpo batteria Pad 0 (Rullante, vel 110)...")
            router.route_drum_hit(DrumTriggerEvent(pad_id=0, velocity=110))
        elif cmd == "G":
            print("  -> Invio coordinate 3D Occhio (X=0.4, Y=0.85, Z=0.6)...")
            router.route_eye_gesture(EyeGesture3D(x=0.4, y=0.85, z=0.6))
            if router.mode == HubOperatingMode.INTERNAL_ENGINE:
                p = router.internal_engine.synth_params
                print(f"     [Faust Synth] Cutoff: {p['filter_cutoff']:.1f} Hz | Wet Space: {p['space_wet']:.2f}")
        elif cmd == "S":
            router.save_studio_to_live_scene(target_slot=0)
            print("     [OK] Studio salvato su Scena 1!")
        elif cmd == "C":
            router.on_scene_change(scene_index=2)
            print("     [OK] Scena 3 attivata!")
        elif cmd == "E":
            router.on_rockpi_encoder_event("EFFECTS", "PRESS_ROTATE_CW", delta=1)
        elif cmd in ("Q", "QUIT", "EXIT"):
            break
        else:
            print("  [!] Comando non riconosciuto.")

    router.pro_bridge.close()
    print("\n[*] Demo terminata.")


if __name__ == "__main__":
    run_interactive_demo()
