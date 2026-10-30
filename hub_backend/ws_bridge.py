#!/usr/bin/env python3
"""
MiMO Hub - WebSocket & OSC Streaming Bridge Server (@COMMS / @HUB)
Collega in tempo reale l'applicazione Desktop (React/Tauri) con l'Audio Router Python/DSP.
"""

import asyncio
import json
import logging
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from hub_backend.audio_router import (
    MiMOAudioRouter,
    HubOperatingMode,
    EyeGesture3D,
    DrumTriggerEvent,
)

logger = logging.getLogger("mimo.ws_bridge")
logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(asctime)s - %(message)s")

HOST = "0.0.0.0"
PORT = 8888


class MimoWebSocketBridge:
    def __init__(self):
        self.router = MiMOAudioRouter(mode=HubOperatingMode.INTERNAL_ENGINE)
        self.connected_clients = set()
        self.running = False

    async def register(self, websocket):
        self.connected_clients.add(websocket)
        logger.info(f"[COMMS] Nuovo client MiMO Hub Desktop connesso ({websocket.remote_address})")
        # Invia stato iniziale
        init_payload = {
            "type": "INITIAL_SYNC",
            "mode": self.router.mode.value,
            "scenes": [s["name"] for s in self.router.live_scenes],
            "connected": True
        }
        await websocket.send(json.dumps(init_payload))

    async def unregister(self, websocket):
        self.connected_clients.discard(websocket)
        logger.info("[COMMS] Client MiMO Hub Desktop disconnesso.")

    async def handle_client_message(self, websocket, message_str: str):
        try:
            data = json.loads(message_str)
            msg_type = data.get("type")

            if msg_type == "SET_OPERATING_MODE":
                new_mode_str = data.get("mode")
                mode = HubOperatingMode[new_mode_str] if new_mode_str in HubOperatingMode.__members__ else HubOperatingMode.INTERNAL_ENGINE
                self.router.set_operating_mode(mode)
                await self.broadcast({"type": "MODE_CHANGED", "mode": self.router.mode.value})

            elif msg_type == "CHANGE_SCENE":
                scene_idx = data.get("scene_index", 0)
                self.router.on_scene_change(scene_idx)
                await self.broadcast({"type": "SCENE_CHANGED", "scene_index": scene_idx})

            elif msg_type == "SAVE_STUDIO_TO_LIVE":
                target_slot = data.get("target_slot", 0)
                self.router.save_studio_to_live_scene(target_slot)
                await self.broadcast({"type": "STUDIO_SAVED", "target_slot": target_slot})

            elif msg_type == "EYE_COORDINATES":
                coords = EyeGesture3D(x=data.get("x", 0.5), y=data.get("y", 0.5), z=data.get("z", 0.5))
                self.router.route_eye_gesture(coords)

        except Exception as e:
            logger.error(f"[COMMS Error] {e}")

    async def broadcast(self, data: dict):
        if not self.connected_clients:
            return
        msg = json.dumps(data)
        await asyncio.gather(*[client.send(msg) for client in self.connected_clients], return_exceptions=True)

    async def run_server(self):
        try:
            import websockets
        except ImportError:
            logger.warning("[!] Libreria 'websockets' non installata. Installa con: pip install websockets")
            return

        self.running = True
        logger.info(f"[*] MiMO WebSocket Bridge Server in ascolto su ws://{HOST}:{PORT}")

        async with websockets.serve(self._ws_handler, HOST, PORT):
            while self.running:
                await asyncio.sleep(1)

    async def _ws_handler(self, websocket, path=None):
        await self.register(websocket)
        try:
            async for message in websocket:
                await self.handle_client_message(websocket, message)
        except Exception:
            pass
        finally:
            await self.unregister(websocket)


if __name__ == "__main__":
    bridge = MimoWebSocketBridge()
    try:
        asyncio.run(bridge.run_server())
    except KeyboardInterrupt:
        logger.info("Server arrestato.")
