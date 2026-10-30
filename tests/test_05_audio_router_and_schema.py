#!/usr/bin/env python3
"""
MiMO - Test 05: Validazione Schema Pacchetto .mimo e Router Audio Desktop
Testa:
  - schemas/mimo_package_schema.json (@DATA)
  - hub_backend/audio_router.py (@AUDIO / @HUB / @COMMS)
"""

import json
import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from hub_backend.audio_router import (
    DrumTriggerEvent,
    EyeGesture3D,
    HubOperatingMode,
    MiMOAudioRouter,
)


class TestMimoPackageSchema(unittest.TestCase):
    """Verifica la validità dello schema JSON per i pacchetti .mimo."""

    def setUp(self):
        schema_path = os.path.join(os.path.dirname(__file__), "..", "schemas", "mimo_package_schema.json")
        with open(schema_path, "r", encoding="utf-8") as f:
            self.schema = json.load(f)

    def test_schema_structure(self):
        self.assertEqual(self.schema["title"], "MiMO Package Manifest Schema")
        self.assertIn("engine_type", self.schema["required"])
        self.assertIn("live_scenes", self.schema["required"])
        self.assertIn("sfz", self.schema["properties"]["engine_type"]["enum"])
        self.assertIn("faust", self.schema["properties"]["engine_type"]["enum"])
        self.assertIn("daw_bridge", self.schema["properties"]["engine_type"]["enum"])

    def test_manifest_example_validation(self):
        sample_manifest = {
            "mimo_version": "1.0.0",
            "name": "Cinematic 3D Gestures",
            "author": "MiMO Lab",
            "description": "Banco orchestrale ibrido e synth 3D",
            "engine_type": "hybrid",
            "engine_config": {
                "sfz_config": {
                    "sfz_file": "audio_assets/strings.sfz",
                    "samples_dir": "audio_assets/samples/",
                    "polyphony": 32
                },
                "faust_config": {
                    "dsp_file": "audio_assets/synth.dsp",
                    "polyphony": 8
                }
            },
            "live_scenes": {
                "scene_1": {"name": "Intro", "instruments": {"port_1": "Strings"}},
                "scene_2": {"name": "Verse", "instruments": {"port_1": "Strings"}},
                "scene_3": {"name": "Chorus", "instruments": {"port_1": "Strings"}},
                "scene_4": {"name": "Bridge", "instruments": {"port_1": "Strings"}},
                "scene_5": {"name": "Outro", "instruments": {"port_1": "Strings"}}
            }
        }
        # Verifica campi minimi obbligatori
        for req in self.schema["required"]:
            self.assertIn(req, sample_manifest)


class TestAudioRouterDoppioBinario(unittest.TestCase):
    """Testa il selettore di modalità operativa e l'instradamento audio/MIDI/OSC."""

    def setUp(self):
        self.router = MiMOAudioRouter(mode=HubOperatingMode.INTERNAL_ENGINE)

    def tearDown(self):
        self.router.pro_bridge.close()

    def test_internal_engine_mode_routing(self):
        self.assertEqual(self.router.mode, HubOperatingMode.INTERNAL_ENGINE)
        self.assertTrue(self.router.internal_engine.active)
        self.assertFalse(self.router.pro_bridge.active)

        # Invio coordinate occhio (60Hz)
        coords = EyeGesture3D(x=0.5, y=0.8, z=0.3)
        self.router.route_eye_gesture(coords)
        # Verifica che il filtro del synth Faust abbia reagito a Y
        self.assertAlmostEqual(self.router.internal_engine.synth_params["space_wet"], 0.3 * 0.95)

        # Invio colpo batteria al campionatore sfizz
        hit = DrumTriggerEvent(pad_id=0, velocity=115)
        self.router.route_drum_hit(hit)

        # Salvataggio da Studio a Scena Live (slot 0 = Scena 1)
        self.router.save_studio_to_live_scene(target_slot=0)
        self.assertIsNotNone(self.router.live_scenes[0]["data"])
        self.assertEqual(self.router.live_scenes[0]["data"]["name"], "SCENA 1 (STUDIO PRESET)")

    def test_pro_daw_bridge_mode_routing(self):
        # Commuta a Binario 2 (Professionisti)
        self.router.set_operating_mode(HubOperatingMode.PRO_DAW_BRIDGE)
        self.assertEqual(self.router.mode, HubOperatingMode.PRO_DAW_BRIDGE)
        self.assertFalse(self.router.internal_engine.active)  # DSP fermato
        self.assertTrue(self.router.pro_bridge.active)        # Bridge attivo

        # Invio coordinate occhio -> devono generare pacchetti senza errori
        coords = EyeGesture3D(x=0.25, y=0.5, z=0.75)
        self.router.route_eye_gesture(coords)

        # Invio colpo batteria -> MIDI Note su canale 10
        hit = DrumTriggerEvent(pad_id=1, velocity=100)
        self.router.route_drum_hit(hit)

        # Evento cambio scena
        self.router.on_scene_change(scene_index=2)

        # Evento encoder Rock Pi
        self.router.on_rockpi_encoder_event("EFFECTS", "PRESS_ROTATE_CW", delta=1)


if __name__ == "__main__":
    unittest.main()
