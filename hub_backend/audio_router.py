#!/usr/bin/env python3
"""
MiMO Hub - Router Audio e Selettore Operativo a Doppio Binario (@AUDIO / @COMMS / @HUB)
Target: Software Desktop su PC / Mac / Linux

Architettura a Doppio Binario:
1. INTERNAL_ENGINE_MODE (Neofiti / Stand-Alone):
   - Riceve eventi dalla Rock Pi (SBC) e coordinate mano (Modulo Occhio) a 60 FPS.
   - Instrada gli eventi al motore ibrido interno:
     * `sfizz` (.sfz): campionatore percussivo (Modulo Batteria multi-velocity) e acustico.
     * `Faust` DSP: sintetizzatori 3D spaziali e catena effetti con macro semantiche One-Knob.
   - Permette di salvare lo stato corrente di STUDIO in uno dei 5 slot Scena LIVE.

2. PRO_DAW_BRIDGE_MODE (Professionisti / DAW Bypass):
   - Disattiva il motore DSP interno per azzerare il carico CPU.
   - Trasforma istantaneamente i controlli fisici della Rock Pi e le coordinate 3D in:
     * Messaggi MIDI standard (Note On/Off, Control Change 14-bit, Program Change).
     * Pacchetti UDP / OSC verso Ableton Live, Bitwig Studio, Logic Pro, Kontakt o VST3.
   - Sincronizza bidirezionalmente lo stato di connessione e i nomi delle tracce con
     la Top Bar dello schermo SPI da 3.5" della Rock Pi.
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
from enum import Enum
import json
import logging
import socket
import time
from typing import Any, Callable, Dict, List, Optional, Tuple

logger = logging.getLogger("mimo.hub.audio_router")
logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(asctime)s - %(message)s")


# =====================================================================
# MODALITÀ OPERATIVE E TIPI EVENTI
# =====================================================================

class HubOperatingMode(Enum):
    INTERNAL_ENGINE = "INTERNAL_ENGINE"  # Neofiti: sfizz + Faust DSP interno
    PRO_DAW_BRIDGE = "PRO_DAW_BRIDGE"    # Professionisti: Trasparente MIDI & OSC per DAW


@dataclass
class EyeGesture3D:
    """Coordinate normalizzate tridimensionali ricevute dal Modulo Occhio a 60Hz."""
    x: float  # [0.0 .. 1.0] Movimento orizzontale
    y: float  # [0.0 .. 1.0] Altezza mano (pitch/filtro)
    z: float  # [0.0 .. 1.0] Profondità / distanza (volume/riverbero)
    timestamp: float = 0.0

    def __post_init__(self):
        if self.timestamp == 0.0:
            self.timestamp = time.time()


@dataclass
class DrumTriggerEvent:
    """Colpo percussivo ricevuto dal Modulo Batteria (piezoelettrico via Pico)."""
    pad_id: int          # 0..7 (Cassa, Rullante, Tom, Charleston, Crash, ecc.)
    velocity: int        # 0..127 Dinamica colpo
    timestamp: float = 0.0


# =====================================================================
# MOTORE AUDIO INTERNO: sfizz (.sfz) + Faust DSP (@AUDIO)
# =====================================================================

class InternalAudioEngine:
    """
    Motore sonoro integrato dentro MiMO Hub.
    Unisce il campionatore sfizz e il DSP sintetico Faust.
    """

    def __init__(self):
        self.active = False
        self.master_volume = 0.85
        
        # Stato canali e suoni caricati
        self.loaded_sfz_preset: Optional[str] = "acoustic_drumkit_808.sfz"
        self.loaded_faust_dsp: Optional[str] = "expressive_ambient_3d.dsp"

        # Catena effetti attivi e macro One-Knob
        self.effects_state: Dict[str, Dict[str, Any]] = {
            "reverb": {"name": "REVERB HALL", "val": 65, "bypass": False},
            "delay":  {"name": "TAPE DELAY",  "val": 40, "bypass": False},
            "filter": {"name": "LOWPASS 24dB","val": 80, "bypass": False},
            "drive":  {"name": "OVERDRIVE",   "val": 25, "bypass": True},
        }

        # Parametri correnti sintetizzatore 3D (pilotati dall'Occhio)
        self.synth_params = {
            "pitch_hz": 440.0,
            "filter_cutoff": 2500.0,
            "resonance": 1.5,
            "space_wet": 0.45
        }

    def start(self):
        self.active = True
        logger.info("[INTERNAL_ENGINE] Motore Ibrido sfizz + Faust DSP AVVIATO.")

    def stop(self):
        self.active = False
        logger.info("[INTERNAL_ENGINE] Motore DSP interno ARRESTATO (CPU risparmiata).")

    def process_drum_hit(self, hit: DrumTriggerEvent):
        """Invia il colpo percussivo al campionatore sfizz."""
        if not self.active:
            return
        logger.debug(f"[sfizz] Pad {hit.pad_id} -> Velocity {hit.velocity} caricato con curve multi-layer.")

    def process_eye_coordinates(self, coords: EyeGesture3D):
        """Modula la sintesi spaziale 3D in Faust DSP."""
        if not self.active:
            return
        # Mappatura semantica diretta:
        # X -> Pan e tono armonico
        # Y -> Cutoff filtro o frequenza tonale quantizzata
        # Z -> Dimensione stanza / decadimento riverbero
        self.synth_params["filter_cutoff"] = 200.0 + (coords.y * 7800.0)
        self.synth_params["space_wet"] = coords.z * 0.95
        logger.debug(f"[Faust DSP] Aggiornato stato 3D: Cutoff={self.synth_params['filter_cutoff']:.1f}Hz, Wet={self.synth_params['space_wet']:.2f}")

    def set_effect_macro(self, effect_id: str, value: int):
        """Imposta la macro semantica One-Knob per l'effetto specificato."""
        if effect_id in self.effects_state:
            self.effects_state[effect_id]["val"] = max(0, min(100, value))
            logger.debug(f"[Faust DSP] Macro {effect_id} regolata al {value}%.")

    def set_effect_bypass(self, effect_id: str, bypass: bool):
        """Attiva o disattiva il bypass dell'effetto."""
        if effect_id in self.effects_state:
            self.effects_state[effect_id]["bypass"] = bypass
            stato = "BYPASS" if bypass else "ATTIVO"
            logger.debug(f"[Faust DSP] Effetto {effect_id} posto in {stato}.")


# =====================================================================
# BRIDGE PRO DAW: MIDI VIRTUALE & OSC TRASPARENTE (@COMMS)
# =====================================================================

class ProDawBridge:
    """
    Bridge trasparente ad alte prestazioni per workstation esterne.
    Traduce i segnali MiMO in messaggi MIDI CC, Note On/Off e pacchetti OSC.
    """

    def __init__(self, midi_port_name: str = "MiMO Virtual Out", osc_host: str = "127.0.0.1", osc_port: int = 9000):
        self.active = False
        self.midi_port_name = midi_port_name
        self.osc_host = osc_host
        self.osc_port = osc_port
        self._udp_sock: Optional[socket.socket] = None
        self._midi_out: Any = None

        self._init_transports()

    def _init_transports(self):
        # Socket UDP per OSC
        try:
            self._udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        except Exception as e:
            logger.warning(f"Impossibile creare socket UDP OSC: {e}")

        # Inizializzazione porta virtuale MIDI (se disponibile mido/rtmidi)
        try:
            import mido
            self._midi_out = mido.open_output(self.midi_port_name, virtual=True)
            logger.info(f"[PRO_BRIDGE] Porta virtuale MIDI '{self.midi_port_name}' aperta.")
        except Exception as e:
            logger.info(f"[PRO_BRIDGE] mido/rtmidi non presente o porta virtuale non supportata ({e}). MIDI opererà in log/mock.")
            self._midi_out = None

    def start(self):
        self.active = True
        logger.info("[PRO_BRIDGE] Bridge DAW Professionale ATTIVO (Ableton/Bitwig/Kontakt).")

    def stop(self):
        self.active = False
        logger.info("[PRO_BRIDGE] Bridge DAW Professionale DISATTIVATO.")

    def close(self):
        self.stop()
        if self._udp_sock is not None:
            try:
                self._udp_sock.close()
            except Exception:
                pass
            self._udp_sock = None
        if self._midi_out is not None:
            try:
                self._midi_out.close()
            except Exception:
                pass
            self._midi_out = None

    def emit_midi_note(self, channel: int, note: int, velocity: int):
        """Invia un messaggio MIDI Note On (e Note Off ritardato)."""
        if not self.active:
            return
        if self._midi_out is not None:
            try:
                import mido
                self._midi_out.send(mido.Message('note_on', channel=channel, note=note, velocity=velocity))
            except Exception:
                pass
        else:
            logger.debug(f"[MIDI OUT] Ch {channel} | Note On: {note} | Vel: {velocity}")

    def emit_midi_cc(self, channel: int, control: int, value: int):
        """Invia un messaggio MIDI Control Change (0..127)."""
        if not self.active:
            return
        clamped_val = max(0, min(127, value))
        if self._midi_out is not None:
            try:
                import mido
                self._midi_out.send(mido.Message('control_change', channel=channel, control=control, value=clamped_val))
            except Exception:
                pass
        else:
            logger.debug(f"[MIDI OUT] Ch {channel} | CC {control} -> {clamped_val}")

    def emit_osc_message(self, path: str, args: List[Any]):
        """Invia un pacchetto OSC minimale via UDP verso la DAW o VST."""
        if not self.active or self._udp_sock is None:
            return
        try:
            # Codifica payload OSC semplice in binario
            payload = self._build_simple_osc(path, args)
            self._udp_sock.sendto(payload, (self.osc_host, self.osc_port))
        except Exception as e:
            logger.debug(f"Errore invio OSC ({path}): {e}")

    def _build_simple_osc(self, path: str, args: List[Any]) -> bytes:
        """Costruisce un pacchetto binario standard conforme a OSC 1.0."""
        # Indirizzo con allineamento a 4 byte
        p_bytes = path.encode('utf-8') + b'\x00'
        p_bytes += b'\x00' * ((4 - len(p_bytes) % 4) % 4)

        # Type tag string
        types = ","
        values_bytes = b""
        for a in args:
            if isinstance(a, float):
                types += "f"
                import struct
                values_bytes += struct.pack(">f", a)
            elif isinstance(a, int):
                types += "i"
                import struct
                values_bytes += struct.pack(">i", a)
            else:
                types += "s"
                s_bytes = str(a).encode('utf-8') + b'\x00'
                s_bytes += b'\x00' * ((4 - len(s_bytes) % 4) % 4)
                values_bytes += s_bytes

        t_bytes = types.encode('utf-8') + b'\x00'
        t_bytes += b'\x00' * ((4 - len(t_bytes) % 4) % 4)

        return p_bytes + t_bytes + values_bytes


# =====================================================================
# SINCRONIZZATORE BIDIREZIONALE ROCK PI 4 (@COMMS)
# =====================================================================

class RockPiSyncClient:
    """Mantiene la sincronizzazione di stato con la Base Centrale Rock Pi 4."""

    def __init__(self, rockpi_host: str = "192.168.1.183", rockpi_port: int = 8888):
        self.rockpi_host = rockpi_host
        self.rockpi_port = rockpi_port
        self.is_connected = False

    def notify_pc_connection_state(self, connected: bool):
        """Notifica la Rock Pi per accendere il pallino verde o spegnerlo sulla Top Bar."""
        self.is_connected = connected
        payload = json.dumps({"type": "SYNC_PC_CONNECTION", "connected": connected})
        self._send_payload(payload)
        logger.info(f"[COMMS -> Rock Pi] Stato connessione PC impostato su: {'CONNESSO (Verde)' if connected else 'STANDALONE (Spento)'}")

    def sync_track_names(self, tracks: List[str]):
        """Invia i nomi delle tracce della DAW per aggiornare il display SPI da 3.5"."""
        payload = json.dumps({"type": "SYNC_TRACK_NAMES", "tracks": tracks})
        self._send_payload(payload)

    def _send_payload(self, json_str: str):
        # Invia messaggio UDP/WebSocket alla Rock Pi
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(json_str.encode('utf-8'), (self.rockpi_host, self.rockpi_port))
            sock.close()
        except Exception:
            pass


# =====================================================================
# ROUTER AUDIO PRINCIPALE A DOPPIO BINARIO (@AUDIO / @HUB)
# =====================================================================

class MiMOAudioRouter:
    """
    Architetto del flusso audio di MiMO Hub.
    Gestisce la commutazione tra Motore Interno (Neofiti) e Bridge DAW (Pro).
    """

    def __init__(self, mode: HubOperatingMode = HubOperatingMode.INTERNAL_ENGINE):
        self.mode = mode
        self.internal_engine = InternalAudioEngine()
        self.pro_bridge = ProDawBridge()
        self.rockpi_sync = RockPiSyncClient()

        # Cache dello stato STUDIO (strumenti per porta, macro effetti)
        self.studio_state = {
            "instruments": {
                "port_1": "Pico Drum Kit 808",
                "port_2": "Ambient 3D Pad",
                "port_3": "Analog Sub Bass",
                "port_4": "Granular Texture",
                "port_5": "Lead Synth"
            },
            "effects": [
                {"id": "reverb", "val": 65, "bypass": False},
                {"id": "delay",  "val": 40, "bypass": False},
                {"id": "filter", "val": 80, "bypass": False},
                {"id": "drive",  "val": 25, "bypass": True}
            ]
        }

        # 5 Scene LIVE
        self.live_scenes: List[Dict[str, Any]] = [
            {"scene_id": i + 1, "name": f"SCENA {i+1}", "data": None} for i in range(5)
        ]

        # Avvio della modalità predefinita
        self.set_operating_mode(self.mode)

    def set_operating_mode(self, new_mode: HubOperatingMode):
        """Commuta dinamicamente il binario operativo del software desktop."""
        self.mode = new_mode

        if new_mode == HubOperatingMode.INTERNAL_ENGINE:
            # Attiva sfizz + Faust, disattiva bridge
            self.pro_bridge.stop()
            self.internal_engine.start()
            logger.info(">>> MODALITÀ ATTIVA: [BINARIO 1] Neofiti / Stand-Alone (sfizz + Faust DSP) <<<")
        else:
            # Disattiva DSP per risparmiare CPU, attiva flussi trasparenti DAW
            self.internal_engine.stop()
            self.pro_bridge.start()
            logger.info(">>> MODALITÀ ATTIVA: [BINARIO 2] Professionisti / DAW Bypass (USB MIDI & UDP/OSC) <<<")

        # Notifica la Rock Pi per mantenere il pallino PC Sync sincronizzato
        self.rockpi_sync.notify_pc_connection_state(connected=True)

    # =====================================================================
    # INSTRADAMENTO INGRESSI ESTERNI (MODULO OCCHIO & BATTERIA)
    # =====================================================================

    def route_eye_gesture(self, coords: EyeGesture3D):
        """Riceve le coordinate 3D della mano a 60 FPS e le instrada in base al binario."""
        if self.mode == HubOperatingMode.INTERNAL_ENGINE:
            # Binario 1: Modula direttamente i parametri del sintetizzatore Faust
            self.internal_engine.process_eye_coordinates(coords)
        else:
            # Binario 2: Converte in MIDI CC standard e pacchetti OSC
            cc_x = int(coords.x * 127)
            cc_y = int(coords.y * 127)
            cc_z = int(coords.z * 127)

            # Canale MIDI 1 dedicato al Modulo Occhio
            self.pro_bridge.emit_midi_cc(channel=1, control=16, value=cc_x)
            self.pro_bridge.emit_midi_cc(channel=1, control=17, value=cc_y)
            self.pro_bridge.emit_midi_cc(channel=1, control=18, value=cc_z)

            # Invia anche flussi continui OSC ad alta risoluzione float
            self.pro_bridge.emit_osc_message("/mimo/eye/xyz", [coords.x, coords.y, coords.z])

    def route_drum_hit(self, hit: DrumTriggerEvent):
        """Riceve un colpo percussivo dal Modulo Batteria."""
        if self.mode == HubOperatingMode.INTERNAL_ENGINE:
            # Binario 1: Riproduzione campione sfizz
            self.internal_engine.process_drum_hit(hit)
        else:
            # Binario 2: Generazione messaggio MIDI Note On su Canale 10 (Standard Percussivo)
            midi_note_map = {0: 36, 1: 38, 2: 42, 3: 46, 4: 41, 5: 45, 6: 49, 7: 51}
            midi_note = midi_note_map.get(hit.pad_id, 36)
            self.pro_bridge.emit_midi_note(channel=10, note=midi_note, velocity=hit.velocity)
            self.pro_bridge.emit_osc_message("/mimo/drum/hit", [hit.pad_id, hit.velocity])

    # =====================================================================
    # CONTROLLI DALLA BASE CENTRALE (ROCK PI 4)
    # =====================================================================

    def on_rockpi_encoder_event(self, encoder_name: str, action: str, delta: int = 0):
        """Gestisce rotazioni, click ed eventi Press & Turn provenienti dalla Rock Pi."""
        logger.debug(f"[Rock Pi Event] Encoder: {encoder_name} | Action: {action} | Delta: {delta}")

        if self.mode == HubOperatingMode.INTERNAL_ENGINE:
            # Modifica macro su motore Faust
            if encoder_name == "EFFECTS":
                if action in ("PRESS_ROTATE_CW", "PRESS_ROTATE_CCW"):
                    self.internal_engine.set_effect_macro("reverb", delta * 5)
                elif action == "SHORT_CLICK":
                    # Toggle bypass
                    cur = self.internal_engine.effects_state["reverb"]["bypass"]
                    self.internal_engine.set_effect_bypass("reverb", not cur)
        else:
            # Emette MIDI CC per pilotare la DAW
            cc_map = {"WORKSPACE": 20, "EFFECTS": 21, "INSTRUMENTS": 22}
            cc_num = cc_map.get(encoder_name, 21)
            # Invia valore relativo o trigger
            self.pro_bridge.emit_midi_cc(channel=1, control=cc_num, value=64 + (delta * 4))
            self.pro_bridge.emit_osc_message(f"/mimo/encoder/{encoder_name.lower()}", [action, delta])

    def on_scene_change(self, scene_index: int):
        """Riceve il cambio scena (Pulsanti 1-5 o pedale sustain)."""
        idx = max(0, min(scene_index, 4))
        logger.info(f"Cambio Scena richiesto -> Scena {idx + 1}")

        if self.mode == HubOperatingMode.INTERNAL_ENGINE:
            # Carica preset di scena interno senza interruzioni (Seamless Spillover)
            pass
        else:
            # Invia Program Change standard alla DAW (0..4)
            if self.pro_bridge.active and self.pro_bridge._midi_out is not None:
                try:
                    import mido
                    self.pro_bridge._midi_out.send(mido.Message('program_change', channel=1, program=idx))
                except Exception:
                    pass
            self.pro_bridge.emit_osc_message("/mimo/scene", [idx + 1])

    # =====================================================================
    # SALVATAGGIO DA STUDIO A LIVE (NEOFITI / STAND-ALONE)
    # =====================================================================

    def save_studio_to_live_scene(self, target_slot: int):
        """Salva la configurazione attuale dello Studio nello slot Scena LIVE (0..4)."""
        idx = max(0, min(target_slot, 4))
        scene_data = {
            "name": f"SCENA {idx + 1} (STUDIO PRESET)",
            "instruments": dict(self.studio_state["instruments"]),
            "effects": [dict(fx) for fx in self.studio_state["effects"]],
            "saved_at": time.time()
        }
        self.live_scenes[idx]["data"] = scene_data
        logger.info(f"[STUDIO -> LIVE] Configurazione corrente salvata con successo nello slot Scena {idx + 1}!")

        # Notifica la Rock Pi del nuovo nome scena per aggiornare la Top Bar
        self.rockpi_sync.sync_track_names([s["name"] for s in self.live_scenes])
