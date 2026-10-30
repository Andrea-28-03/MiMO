#!/usr/bin/env python3
"""
MiMO - Test 04: Validazione Completa Moduli di Produzione
Testa l'integrazione tra:
  - boards/board_config.json (@DATA)
  - src/hardware_monitor.py (@HARDWARE)
  - src/state_machine.py (@LOGIC)
  - src/ui_pygame_spi.py (@GUI)
"""

import os
import sys
import time
import unittest

# Assicura importazione moduli da root di progetto
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from src.hardware_monitor import (
    EncoderId,
    EventType,
    HardwareEvent,
    HardwareMonitor,
    ModuleState,
    SingleEncoderTracker,
)
from src.state_machine import (
    DirtyZone,
    MiMOStateMachine,
    StateSnapshot,
    ViewMode,
    Workspace,
)


class TestHardwareMonitor(unittest.TestCase):
    """Testa la logica di tracciamento encoder e thread diagnostico I2C."""

    def test_encoder_quadrature_and_press_rotate(self):
        events = []
        tracker = SingleEncoderTracker(EncoderId.EFFECTS, lambda ev: events.append(ev))

        # Simula 4 step per scatto CW: 11 -> 01 -> 00 -> 10 -> 11
        tracker.update_pins(clk=0, dt=1, sw=1)
        tracker.update_pins(clk=0, dt=0, sw=1)
        tracker.update_pins(clk=1, dt=0, sw=1)
        tracker.update_pins(clk=1, dt=1, sw=1)

        self.assertTrue(len(events) >= 1)
        self.assertEqual(events[-1].event_type, EventType.ROTATE_CW)

        # Ora simula pressione SW (0 = premuto) e rotazione CCW: 11 -> 10 -> 00 -> 01 -> 11
        events.clear()
        tracker.update_pins(clk=1, dt=1, sw=0)  # Premi SW
        tracker.update_pins(clk=1, dt=0, sw=0)
        tracker.update_pins(clk=0, dt=0, sw=0)
        tracker.update_pins(clk=0, dt=1, sw=0)
        tracker.update_pins(clk=1, dt=1, sw=0)

        self.assertTrue(len(events) >= 1)
        self.assertEqual(events[-1].event_type, EventType.PRESS_ROTATE_CCW)

    def test_encoder_double_click_detection(self):
        events = []
        tracker = SingleEncoderTracker(EncoderId.INSTRUMENTS, lambda ev: events.append(ev))

        t0 = time.time()
        # Primo click: giù a t0, su a t0 + 0.05
        tracker.update_pins(clk=1, dt=1, sw=0, now=t0)
        tracker.update_pins(clk=1, dt=1, sw=1, now=t0 + 0.05)

        # Secondo click rapido a t0 + 0.15 (entro 280ms)
        tracker.update_pins(clk=1, dt=1, sw=0, now=t0 + 0.15)
        tracker.update_pins(clk=1, dt=1, sw=1, now=t0 + 0.20)

        # Deve emettere DOUBLE_CLICK
        double_clicks = [ev for ev in events if ev.event_type == EventType.DOUBLE_CLICK]
        self.assertEqual(len(double_clicks), 1)

    def test_i2c_diagnostics_thread_safe_status(self):
        hw = HardwareMonitor(config_path="boards/board_config.json")
        status = hw.get_module_status()
        self.assertIn("porta_1", status)
        self.assertIn("status", status["porta_1"])


class TestStateMachineLogic(unittest.TestCase):
    """Testa le regole di stato definite in 04_AGENT_LOGIC.md."""

    def setUp(self):
        self.sm = MiMOStateMachine()

    def test_workspace_security_lock(self):
        # Rotazione a vuoto ignorata quando bloccata
        snap = self.sm.process_event(HardwareEvent(EventType.ROTATE_CW, EncoderId.WORKSPACE.value))
        self.assertEqual(snap.workspace, Workspace.STUDIO)
        self.assertFalse(snap.workspace_unlocked)

        # Click singolo sblocca
        snap = self.sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.WORKSPACE.value))
        self.assertTrue(snap.workspace_unlocked)
        self.assertIn(DirtyZone.TOP_BAR, snap.dirty_zones)

        # Ora la rotazione cambia Workspace
        snap = self.sm.process_event(HardwareEvent(EventType.ROTATE_CW, EncoderId.WORKSPACE.value))
        self.assertEqual(snap.workspace, Workspace.LIVE)
        self.assertFalse(snap.workspace_unlocked)  # Si riblocca automaticamente
        self.assertIn(DirtyZone.ALL, snap.dirty_zones)

    def test_studio_instruments_navigation(self):
        self.assertEqual(self.sm.workspace, Workspace.STUDIO)
        self.assertEqual(self.sm.view_mode, ViewMode.HOME)

        # Click singolo Encoder Strumenti: apre lista strumenti nella Center Area
        snap = self.sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.INSTRUMENTS.value))
        self.assertEqual(snap.view_mode, ViewMode.INSTRUMENT_LIST)
        self.assertIn(DirtyZone.CENTER_AREA, snap.dirty_zones)
        self.assertNotIn(DirtyZone.TOP_BAR, snap.dirty_zones)

        # Doppio click su uno strumento: apre impostazioni
        snap = self.sm.process_event(HardwareEvent(EventType.DOUBLE_CLICK, EncoderId.INSTRUMENTS.value))
        self.assertEqual(snap.view_mode, ViewMode.INSTRUMENT_SETTINGS)
        self.assertIn(DirtyZone.CENTER_AREA, snap.dirty_zones)

    def test_press_and_turn_value_modification(self):
        # In impostazioni strumenti, Press & Turn modifica valore
        self.sm.view_mode = ViewMode.INSTRUMENT_SETTINGS
        init_val = self.sm.instruments_data[self.sm.instruments_by_slot[0]][0]["val"]

        snap = self.sm.process_event(HardwareEvent(EventType.PRESS_ROTATE_CW, EncoderId.INSTRUMENTS.value))
        new_val = snap.instrument_params[0]["val"]
        self.assertEqual(new_val, min(100, init_val + 5))

    def test_live_effects_bypass_toggle(self):
        # Passa a LIVE
        self.sm.workspace = Workspace.LIVE
        self.sm.view_mode = ViewMode.HOME

        # Click Encoder Effetti: sblocca lista effetti
        snap = self.sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.EFFECTS.value))
        self.assertEqual(snap.view_mode, ViewMode.EFFECT_LIST)

        # 1 Click su un effetto in LIVE: Bypass toggle immediato
        initial_active = snap.effects[0]["active"]
        snap = self.sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.EFFECTS.value))
        self.assertEqual(snap.effects[0]["active"], not initial_active)

    def test_pedal_sustain_scene_advance(self):
        self.sm.workspace = Workspace.LIVE
        initial_scene = self.sm.current_scene_idx

        snap = self.sm.process_event(HardwareEvent(EventType.PEDAL_TRIGGER, 0))
        self.assertEqual(snap.current_scene_idx, (initial_scene + 1) % 5)


if __name__ == "__main__":
    unittest.main()
