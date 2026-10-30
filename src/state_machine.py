#!/usr/bin/env python3
"""
MiMO - Macchina a Stati Centrale (@LOGIC)
Target: Rock Pi 4 Plus (SoC RK3399/OP1)

Funzionalità Principali:
1. Gestione Macchina a Stati per i Workspace STUDIO e LIVE.
2. Blocco di sicurezza sull'Encoder Workspace (richiede click per sbloccare la rotazione).
3. Navigazione ergonomica unificata "Press & Turn":
   - Rotazione semplice: navigazione / scorrimento voci o parametri.
   - Press & Turn: incremento/decremento valore numerico/percentuale (+5% / -5%).
   - Click singolo: selezione / conferma / bypass toggle.
   - Doppio click rapido (<300ms): apertura sottomenu impostazioni avanzate.
4. Emissione di `StateSnapshot` immutabile verso @GUI con tracciamento esatto delle
   `dirty_zones` (TOP_BAR, CENTER_AREA, BOTTOM_BAR, ITEM_INDEX) per il rendering parziale.
5. Supporto al salvataggio da STUDIO a LIVE (assegnazione ai 5 pulsanti fisici).
"""

from __future__ import annotations

import copy
from dataclasses import dataclass, field
from enum import Enum, auto
import logging
import time
from typing import Any, Dict, List, Optional, Set

from src.hardware_monitor import EncoderId, EventType, HardwareEvent, ModuleState

logger = logging.getLogger("mimo.logic")


# =====================================================================
# MODALITÀ E ZONE SPORCHE (DIRTY REGIONS)
# =====================================================================

class Workspace(Enum):
    STUDIO = "STUDIO"
    LIVE = "LIVE"


class ViewMode(Enum):
    HOME = "HOME"
    INSTRUMENT_LIST = "INSTRUMENT_LIST"
    INSTRUMENT_SETTINGS = "INSTRUMENT_SETTINGS"
    EFFECT_LIST = "EFFECT_LIST"
    EFFECT_SETTINGS = "EFFECT_SETTINGS"


class DirtyZone(Enum):
    NONE = auto()
    ALL = auto()
    TOP_BAR = auto()
    CENTER_AREA = auto()
    BOTTOM_BAR = auto()
    EFFECT_ITEM = auto()


# =====================================================================
# SNAPSHOT DI STATO IMMUTABILE VERSO @GUI
# =====================================================================

@dataclass(frozen=True)
class StateSnapshot:
    """Istantanea immutabile dello stato di MiMO emessa ad ogni transizione logica."""
    workspace: Workspace
    workspace_unlocked: bool
    hub_connected: bool
    selected_slot: int                    # 0..4 (corrispondente ai 5 pulsanti fisici)
    view_mode: ViewMode
    
    # Nomi di riferimento contestuali per la Top Bar
    current_instrument_name: str
    current_preset_name: str

    # Dati Strumenti
    instruments_by_slot: List[str]
    instrument_library: List[str]
    instrument_list_idx: int
    instrument_params: List[Dict[str, Any]]
    instrument_param_idx: int

    # Dati Effetti
    effects: List[Dict[str, Any]]
    effect_list_idx: int
    effect_param_idx: int

    # Dati Live
    live_scenes: List[Dict[str, Any]]
    current_scene_idx: int

    # Diagnostica Hardware da @HARDWARE
    module_status: Dict[str, Dict[str, str]]

    # Aree modificate per Partial Update (Dirty Rectangles)
    dirty_zones: Set[DirtyZone] = field(default_factory=set)
    dirty_item_idx: Optional[int] = None


# =====================================================================
# MACCHINA A STATI PRINCIPALE (@LOGIC)
# =====================================================================

class MiMOStateMachine:
    """Architetto della logica deterministica ed Event Router per la Base Centrale."""

    def __init__(self):
        self.workspace = Workspace.STUDIO
        self.workspace_unlocked = False
        self.hub_connected = False
        self.selected_slot = 0
        self.view_mode = ViewMode.HOME

        # Libreria e assegnazione iniziale strumenti
        self.instrument_library = [
            "JUNO-60 POLY SYNTH",
            "PICO DRUM KIT 808",
            "THEREMIN OCCHIO 3D",
            "MINIMOOG SUB BASS",
            "GRANULAR PAD VOX",
            "DX7 ELECTRIC PIANO",
            "MELLOTRON STRINGS",
            "TB-303 BASSLINE",
        ]
        self.instruments_by_slot = [
            "JUNO-60 POLY SYNTH",
            "PICO DRUM KIT 808",
            "THEREMIN OCCHIO 3D",
            "MINIMOOG SUB BASS",
            "GRANULAR PAD VOX",
        ]
        self.instrument_list_idx = 0
        self.instrument_param_idx = 0

        # Parametri sonori dettagliati per ciascun generatore
        self.instruments_data = {
            "JUNO-60 POLY SYNTH": [
                {"name": "FILTER CUTOFF", "val": 75, "unit": "%"},
                {"name": "ATTACK TIME",   "val": 20, "unit": "ms"},
                {"name": "RESONANCE",     "val": 45, "unit": "%"},
            ],
            "PICO DRUM KIT 808": [
                {"name": "PITCH TUNE",    "val": 50, "unit": "%"},
                {"name": "DECAY TIME",    "val": 65, "unit": "ms"},
                {"name": "SNAP LEVEL",    "val": 80, "unit": "%"},
            ],
            "THEREMIN OCCHIO 3D": [
                {"name": "SENSITIVITY",   "val": 85, "unit": "%"},
                {"name": "SMOOTHING",     "val": 35, "unit": "ms"},
                {"name": "SCALE LOCK",    "val": 100, "unit": "%"},
            ],
            "MINIMOOG SUB BASS": [
                {"name": "SUB OSC",       "val": 90, "unit": "%"},
                {"name": "FILTER GLIDE",  "val": 25, "unit": "ms"},
                {"name": "OVERDRIVE",     "val": 40, "unit": "%"},
            ],
            "GRANULAR PAD VOX": [
                {"name": "GRAIN SIZE",    "val": 60, "unit": "ms"},
                {"name": "DENSITY",       "val": 70, "unit": "%"},
                {"name": "SPRAY SPREAD",  "val": 40, "unit": "%"},
            ],
        }

        # Catena Effetti STUDIO (16 Moduli DSP)
        self.effects = [
            {"name": "REVERB HALL",     "val": 65, "active": True,  "cat": "SPACE"},
            {"name": "TAPE DELAY",      "val": 40, "active": True,  "cat": "TIME"},
            {"name": "TUBE OVERDRIVE",  "val": 25, "active": False, "cat": "DRIVE"},
            {"name": "LOWPASS 24dB",    "val": 80, "active": True,  "cat": "FILTER"},
            {"name": "STEREO CHORUS",   "val": 50, "active": False, "cat": "MOD"},
            {"name": "OPTO COMPRESSOR", "val": 70, "active": True,  "cat": "DYN"},
            {"name": "ANALOG PHASER",   "val": 35, "active": False, "cat": "MOD"},
            {"name": "BIT CRUSHER",     "val": 15, "active": False, "cat": "LO-FI"},
            {"name": "PARAMETRIC EQ",   "val": 50, "active": True,  "cat": "EQ"},
            {"name": "VINTAGE FLANGER", "val": 45, "active": False, "cat": "MOD"},
            {"name": "TREMOLO PAN",     "val": 60, "active": False, "cat": "DYN"},
            {"name": "PITCH SHIFTER",   "val": 20, "active": False, "cat": "PITCH"},
            {"name": "AUTO-WAH ENVEL",  "val": 55, "active": False, "cat": "FILTER"},
            {"name": "PLATE REVERB",    "val": 40, "active": True,  "cat": "SPACE"},
            {"name": "NOISE GATE",      "val": 85, "active": True,  "cat": "DYN"},
            {"name": "FUZZ DISTORTION", "val": 30, "active": False, "cat": "DRIVE"},
        ]
        for fx in self.effects:
            fx["params"] = [
                {"name": "WET / DRY MIX", "val": fx["val"], "unit": "%"},
                {"name": "TIME / RATE",   "val": 50,         "unit": "ms"},
                {"name": "TONE / DAMP",   "val": 70,         "unit": "%"},
            ]

        self.effect_list_idx = 0
        self.effect_param_idx = 0

        # Scene LIVE preconfigurate (Slot 1..5)
        self.live_scenes = [
            {"name": "SCENA 1: INTRO AMBIENT", "instruments": list(self.instruments_by_slot), "effects_bypass": [False, False, True, False]},
            {"name": "SCENA 2: VERSE GROOVE",  "instruments": list(self.instruments_by_slot), "effects_bypass": [False, True, False, False]},
            {"name": "SCENA 3: LEAD SOLO_01",  "instruments": list(self.instruments_by_slot), "effects_bypass": [False, False, False, False]},
            {"name": "SCENA 4: DROP BASS",     "instruments": list(self.instruments_by_slot), "effects_bypass": [True, True, False, False]},
            {"name": "SCENA 5: OUTRO DELAY",   "instruments": list(self.instruments_by_slot), "effects_bypass": [False, False, True, True]},
        ]
        self.current_scene_idx = 0

        # Stato Diagnostica Moduli iniziale
        self.module_status: Dict[str, Dict[str, str]] = {
            "porta_1": {"name": "Pico Drum", "status": ModuleState.CONNECTED.value},
            "porta_2": {"name": "Occhio",    "status": ModuleState.CONNECTED.value},
            "porta_3": {"name": "",          "status": ModuleState.EMPTY.value},
            "porta_4": {"name": "Sensore X", "status": ModuleState.CONNECTED.value},
            "porta_5": {"name": "",          "status": ModuleState.EMPTY.value},
        }

    # =====================================================================
    # CREAZIONE SNAPSHOT PER LA GUI
    # =====================================================================

    def get_snapshot(self, dirty_zones: Optional[Set[DirtyZone]] = None, dirty_item_idx: Optional[int] = None) -> StateSnapshot:
        """Genera un'istantanea congelata dello stato corrente per il renderer Pygame."""
        curr_inst = self.instruments_by_slot[self.selected_slot]
        preset_name = self.live_scenes[self.current_scene_idx]["name"]
        curr_inst_params = self.instruments_data.get(curr_inst, [
            {"name": "PARAM 1", "val": 50, "unit": "%"},
            {"name": "PARAM 2", "val": 50, "unit": "%"},
            {"name": "PARAM 3", "val": 50, "unit": "%"}
        ])

        return StateSnapshot(
            workspace=self.workspace,
            workspace_unlocked=self.workspace_unlocked,
            hub_connected=self.hub_connected,
            selected_slot=self.selected_slot,
            view_mode=self.view_mode,
            current_instrument_name=curr_inst,
            current_preset_name=preset_name,
            instruments_by_slot=list(self.instruments_by_slot),
            instrument_library=list(self.instrument_library),
            instrument_list_idx=self.instrument_list_idx,
            instrument_params=copy.deepcopy(curr_inst_params),
            instrument_param_idx=self.instrument_param_idx,
            effects=copy.deepcopy(self.effects),
            effect_list_idx=self.effect_list_idx,
            effect_param_idx=self.effect_param_idx,
            live_scenes=copy.deepcopy(self.live_scenes),
            current_scene_idx=self.current_scene_idx,
            module_status=copy.deepcopy(self.module_status),
            dirty_zones=dirty_zones if dirty_zones is not None else {DirtyZone.ALL},
            dirty_item_idx=dirty_item_idx
        )

    # =====================================================================
    # ELABORAZIONE EVENTI HARDWARE
    # =====================================================================

    def process_event(self, event: HardwareEvent) -> StateSnapshot:
        """Elabora un evento fisico ed emette il nuovo StateSnapshot con le dirty_zones calcolate."""
        dirty: Set[DirtyZone] = set()
        dirty_item: Optional[int] = None

        # -------------------------------------------------------------
        # 1. ENCODER 1: WORKSPACE (BLOCCO DI SICUREZZA)
        # -------------------------------------------------------------
        if event.source_id == EncoderId.WORKSPACE.value:
            if event.event_type == EventType.SHORT_CLICK:
                # Sblocca/Blocca la selezione del Workspace
                self.workspace_unlocked = not self.workspace_unlocked
                dirty.add(DirtyZone.TOP_BAR)

            elif event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                if self.workspace_unlocked:
                    # Cambio Workspace consentito solo a manopola sbloccata
                    if self.workspace == Workspace.STUDIO:
                        self.workspace = Workspace.LIVE
                    else:
                        self.workspace = Workspace.STUDIO
                    
                    self.view_mode = ViewMode.HOME
                    self.workspace_unlocked = False
                    dirty.add(DirtyZone.ALL)
                else:
                    logger.debug("Rotazione Workspace ignorata: manopola bloccata (richiede click preventivo).")

        # -------------------------------------------------------------
        # 2. ENCODER 2: EFFETTI AUDIO
        # -------------------------------------------------------------
        elif event.source_id == EncoderId.EFFECTS.value:
            if self.workspace == Workspace.STUDIO:
                dirty |= self._handle_effects_studio(event)
            else:
                dirty |= self._handle_effects_live(event)

        # -------------------------------------------------------------
        # 3. ENCODER 3: STRUMENTI E GENERATORI
        # -------------------------------------------------------------
        elif event.source_id == EncoderId.INSTRUMENTS.value:
            if self.workspace == Workspace.STUDIO:
                dirty |= self._handle_instruments_studio(event)
            else:
                dirty |= self._handle_instruments_live(event)

        # -------------------------------------------------------------
        # 4. I 5 PULSANTI FISICI DELLA SCOCCA (Slot 0..4 su MCP23017)
        # -------------------------------------------------------------
        elif event.event_type == EventType.BUTTON_CLICK:
            slot = max(0, min(event.source_id, 4))
            if self.workspace == Workspace.STUDIO:
                # Selettori di Modulo Target
                self.selected_slot = slot
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.TOP_BAR)
                dirty.add(DirtyZone.CENTER_AREA)
                dirty.add(DirtyZone.BOTTOM_BAR)
            else:
                # Selettori di Scena Istantanei (<1ms)
                self.current_scene_idx = slot
                self.selected_slot = slot
                dirty.add(DirtyZone.TOP_BAR)
                dirty.add(DirtyZone.CENTER_AREA)
                dirty.add(DirtyZone.BOTTOM_BAR)

        # -------------------------------------------------------------
        # 5. PEDALE SUSTAIN (Scene Switcher Hands-Free in LIVE)
        # -------------------------------------------------------------
        elif event.event_type == EventType.PEDAL_TRIGGER:
            if self.workspace == Workspace.LIVE:
                self.current_scene_idx = (self.current_scene_idx + 1) % len(self.live_scenes)
                self.selected_slot = self.current_scene_idx
                dirty.add(DirtyZone.ALL)

        if not dirty:
            dirty.add(DirtyZone.NONE)

        return self.get_snapshot(dirty_zones=dirty, dirty_item_idx=dirty_item)

    # =====================================================================
    # LOGICA DI DETTAGLIO EFFETTI (STUDIO vs LIVE)
    # =====================================================================

    def _handle_effects_studio(self, event: HardwareEvent) -> Set[DirtyZone]:
        dirty: Set[DirtyZone] = set()

        if self.view_mode == ViewMode.HOME:
            if event.event_type == EventType.SHORT_CLICK:
                # 1 Click: apre lista effetti nella Center Area (Top Bar invariata)
                self.view_mode = ViewMode.EFFECT_LIST
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.DOUBLE_CLICK:
                # Doppio click rapido (<300ms): apre impostazioni dell'effetto selezionato
                self.view_mode = ViewMode.EFFECT_SETTINGS
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.ROTATE_CW:
                self.effect_list_idx = (self.effect_list_idx + 1) % len(self.effects)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.ROTATE_CCW:
                self.effect_list_idx = (self.effect_list_idx - 1) % len(self.effects)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.PRESS_ROTATE_CW:
                # Press & Turn: incrementa valore percentuale
                fx = self.effects[self.effect_list_idx]
                fx["val"] = min(100, fx["val"] + 5)
                fx["params"][0]["val"] = fx["val"]
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.PRESS_ROTATE_CCW:
                # Press & Turn: decrementa valore percentuale
                fx = self.effects[self.effect_list_idx]
                fx["val"] = max(0, fx["val"] - 5)
                fx["params"][0]["val"] = fx["val"]
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.EFFECT_LIST:
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.effect_list_idx = (self.effect_list_idx + delta) % len(self.effects)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                # 1 Click su effetto: lo seleziona e torna a HOME
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.DOUBLE_CLICK:
                # Doppio click su effetto: apre impostazioni avanzate
                self.view_mode = ViewMode.EFFECT_SETTINGS
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.EFFECT_SETTINGS:
            fx = self.effects[self.effect_list_idx]
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                # Rotazione semplice: scorre i parametri interni
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.effect_param_idx = (self.effect_param_idx + delta) % len(fx["params"])
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.PRESS_ROTATE_CW, EventType.PRESS_ROTATE_CCW):
                # Press & Turn: modifica valore del parametro selezionato
                delta = 5 if event.event_type == EventType.PRESS_ROTATE_CW else -5
                p = fx["params"][self.effect_param_idx]
                p["val"] = max(0, min(100, p["val"] + delta))
                if self.effect_param_idx == 0:
                    fx["val"] = p["val"]
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                # Click singolo esce dal sottomenu
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.CENTER_AREA)

        return dirty

    def _handle_effects_live(self, event: HardwareEvent) -> Set[DirtyZone]:
        dirty: Set[DirtyZone] = set()

        if self.view_mode == ViewMode.HOME:
            if event.event_type == EventType.SHORT_CLICK:
                # In LIVE, il click sblocca la lista degli effetti del template
                self.view_mode = ViewMode.EFFECT_LIST
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.effect_list_idx = (self.effect_list_idx + delta) % len(self.effects)
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.EFFECT_LIST:
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.effect_list_idx = (self.effect_list_idx + delta) % len(self.effects)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                # In LIVE: 1 Click su un effetto = Bypass Toggle ON/OFF istantaneo!
                fx = self.effects[self.effect_list_idx]
                fx["active"] = not fx["active"]
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.DOUBLE_CLICK:
                # In LIVE: Doppio Click rapido = impostazioni avanzate per ritocchi al volo
                self.view_mode = ViewMode.EFFECT_SETTINGS
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.EFFECT_SETTINGS:
            fx = self.effects[self.effect_list_idx]
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.effect_param_idx = (self.effect_param_idx + delta) % len(fx["params"])
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.PRESS_ROTATE_CW, EventType.PRESS_ROTATE_CCW):
                delta = 5 if event.event_type == EventType.PRESS_ROTATE_CW else -5
                p = fx["params"][self.effect_param_idx]
                p["val"] = max(0, min(100, p["val"] + delta))
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.CENTER_AREA)

        return dirty

    # =====================================================================
    # LOGICA DI DETTAGLIO STRUMENTI (STUDIO vs LIVE)
    # =====================================================================

    def _handle_instruments_studio(self, event: HardwareEvent) -> Set[DirtyZone]:
        dirty: Set[DirtyZone] = set()

        if self.view_mode == ViewMode.HOME:
            if event.event_type == EventType.SHORT_CLICK:
                # Click Encoder Strumenti: apre nella Center Area la lista degli strumenti (Top Bar invariata)
                self.view_mode = ViewMode.INSTRUMENT_LIST
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.DOUBLE_CLICK:
                # Doppio click rapido (<300ms): apre impostazioni dello strumento attivo
                self.view_mode = ViewMode.INSTRUMENT_SETTINGS
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                # Rotazione semplice: scorre ciclicamente la libreria strumenti
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.instrument_list_idx = (self.instrument_list_idx + delta) % len(self.instrument_library)
                self.instruments_by_slot[self.selected_slot] = self.instrument_library[self.instrument_list_idx]
                dirty.add(DirtyZone.TOP_BAR)
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.INSTRUMENT_LIST:
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                self.instrument_list_idx = (self.instrument_list_idx + delta) % len(self.instrument_library)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                # 1 Click: seleziona lo strumento per lo slot corrente e torna a HOME
                self.instruments_by_slot[self.selected_slot] = self.instrument_library[self.instrument_list_idx]
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.TOP_BAR)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.DOUBLE_CLICK:
                # Doppio click: seleziona e apre subito le impostazioni
                self.instruments_by_slot[self.selected_slot] = self.instrument_library[self.instrument_list_idx]
                self.view_mode = ViewMode.INSTRUMENT_SETTINGS
                dirty.add(DirtyZone.TOP_BAR)
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.INSTRUMENT_SETTINGS:
            curr_inst = self.instruments_by_slot[self.selected_slot]
            params = self.instruments_data.get(curr_inst, [])
            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                if params:
                    self.instrument_param_idx = (self.instrument_param_idx + delta) % len(params)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.PRESS_ROTATE_CW, EventType.PRESS_ROTATE_CCW):
                # Press & Turn modifica parametro strumento
                delta = 5 if event.event_type == EventType.PRESS_ROTATE_CW else -5
                if params:
                    p = params[self.instrument_param_idx]
                    p["val"] = max(0, min(100, p["val"] + delta))
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.CENTER_AREA)

        return dirty

    def _handle_instruments_live(self, event: HardwareEvent) -> Set[DirtyZone]:
        dirty: Set[DirtyZone] = set()

        if self.view_mode == ViewMode.HOME:
            if event.event_type == EventType.SHORT_CLICK:
                # In LIVE: Click Encoder Strumenti apre direttamente la modifica delle impostazioni
                self.view_mode = ViewMode.INSTRUMENT_SETTINGS
                dirty.add(DirtyZone.CENTER_AREA)

        elif self.view_mode == ViewMode.INSTRUMENT_SETTINGS:
            curr_inst = self.instruments_by_slot[self.selected_slot]
            params = self.instruments_data.get(curr_inst, [])

            if event.event_type in (EventType.ROTATE_CW, EventType.ROTATE_CCW):
                # Gira = scorri parametri
                delta = 1 if event.event_type == EventType.ROTATE_CW else -1
                if params:
                    self.instrument_param_idx = (self.instrument_param_idx + delta) % len(params)
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type in (EventType.PRESS_ROTATE_CW, EventType.PRESS_ROTATE_CCW):
                # Premi e gira = modifica valore
                delta = 5 if event.event_type == EventType.PRESS_ROTATE_CW else -5
                if params:
                    p = params[self.instrument_param_idx]
                    p["val"] = max(0, min(100, p["val"] + delta))
                dirty.add(DirtyZone.CENTER_AREA)

            elif event.event_type == EventType.SHORT_CLICK:
                self.view_mode = ViewMode.HOME
                dirty.add(DirtyZone.CENTER_AREA)

        return dirty

    # =====================================================================
    # INTEGRAZIONE DESKTOP E AGGIORNAMENTI ESTERNI
    # =====================================================================

    def save_studio_to_live_scene(self, scene_idx: int) -> StateSnapshot:
        """Salva la configurazione attuale dello Studio assegnandola a una Scena LIVE (0..4)."""
        idx = max(0, min(scene_idx, len(self.live_scenes) - 1))
        self.live_scenes[idx] = {
            "name": f"SCENA {idx+1}: STUDIO SAVE",
            "instruments": list(self.instruments_by_slot),
            "effects_bypass": [not fx["active"] for fx in self.effects[:4]]
        }
        logger.info(f"Configurazione Studio salvata su Scena LIVE {idx+1}.")
        return self.get_snapshot({DirtyZone.CENTER_AREA, DirtyZone.TOP_BAR})

    def set_hub_connected(self, connected: bool) -> StateSnapshot:
        """Aggiorna lo stato di connessione con MiMO Hub su PC (Verde / Spento)."""
        if self.hub_connected != connected:
            self.hub_connected = connected
            return self.get_snapshot({DirtyZone.TOP_BAR})
        return self.get_snapshot({DirtyZone.NONE})

    def update_module_status(self, new_status: Dict[str, Dict[str, str]]) -> StateSnapshot:
        """Riceve l'aggiornamento diagnostico da @HARDWARE e aggiorna la Bottom Bar."""
        has_changed = False
        for k, v in new_status.items():
            if k in self.module_status:
                if self.module_status[k] != v:
                    self.module_status[k] = v
                    has_changed = True

        if has_changed:
            return self.get_snapshot({DirtyZone.BOTTOM_BAR})
        return self.get_snapshot({DirtyZone.NONE})
