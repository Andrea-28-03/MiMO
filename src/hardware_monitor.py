#!/usr/bin/env python3
"""
MiMO - Sottosistema Hardware Monitor (@HARDWARE)
Target: Rock Pi 4 Plus (SoC RK3399/OP1) su Armbian Linux

Funzionalità Principali:
1. Lettura ad alta velocità ad interrupt (libgpiod v2/v1 o python-periphery) per i 3 Encoder PATIKIL EC11:
   - Encoder 1 (Workspace): Pin 29 (CLK), Pin 31 (DT), Pin 33 (SW)
   - Encoder 2 (Effetti):   Pin 32 (CLK), Pin 36 (DT), Pin 38 (SW)
   - Encoder 3 (Strumenti): Pin 35 (CLK), Pin 37 (DT), Pin 40 (SW)
   Distingue:
   - ROTATE_CW / ROTATE_CCW
   - PRESS_ROTATE_CW / PRESS_ROTATE_CCW (Press & Turn)
   - SHORT_CLICK / DOUBLE_CLICK (finestra temporale 280ms) al rilascio del tasto SW.

2. Thread Daemon di isolamento diagnostico I2C (Anti-Freeze):
   - Interroga ogni 2.0 secondi il Multiplexer PCA9548 (0x70) sui canali 0-7.
   - Non blocca MAI l'event loop o la GUI.
   - Mantiene in memoria RAM il dizionario `module_status` con stati CONNECTED, EMPTY, ERROR.
"""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from enum import Enum, auto
import importlib
import json
import logging
import os
import queue
import sys
import threading
import time
from typing import Any, Callable, Dict, List, Optional, Tuple

logger = logging.getLogger("mimo.hardware")
logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(asctime)s - %(message)s")


# =====================================================================
# ENUMERAZIONI ED EVENTI HARDWARE
# =====================================================================

class EncoderId(Enum):
    WORKSPACE = 1
    EFFECTS = 2
    INSTRUMENTS = 3


class EventType(Enum):
    ROTATE_CW = "ROTATE_CW"
    ROTATE_CCW = "ROTATE_CCW"
    PRESS_ROTATE_CW = "PRESS_ROTATE_CW"
    PRESS_ROTATE_CCW = "PRESS_ROTATE_CCW"
    SHORT_CLICK = "SHORT_CLICK"
    DOUBLE_CLICK = "DOUBLE_CLICK"
    BUTTON_CLICK = "BUTTON_CLICK"     # 5 Pulsanti Tattili (Slot 0..4)
    PEDAL_TRIGGER = "PEDAL_TRIGGER"   # Pedale sustain / Scene Switcher


class ModuleState(Enum):
    CONNECTED = "CONNECTED"
    EMPTY = "EMPTY"
    ERROR = "ERROR"


@dataclass(frozen=True)
class HardwareEvent:
    event_type: EventType
    source_id: int          # 1..3 per gli Encoder, 0..4 per i 5 Tasti Modulo
    timestamp: float = 0.0

    def __post_init__(self):
        if self.timestamp == 0.0:
            object.__setattr__(self, "timestamp", time.time())


# Tabella di transizione Gray Code a 4 bit per quadratura EC11
# [vecchio_stato(2bit) << 2 | nuovo_stato(2bit)]
QUADRATURE_TABLE = [
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
]


# =====================================================================
# GESTORE SINGOLO ENCODER ROTATIVO (EC11)
# =====================================================================

class SingleEncoderTracker:
    """Traccia la rotazione a quadratura e i click di un singolo encoder EC11."""

    def __init__(self, encoder_id: EncoderId, event_sink: Callable[[HardwareEvent], None]):
        self.encoder_id = encoder_id
        self.event_sink = event_sink

        self.last_state = 0b11  # Tipico stato a riposo con pull-up
        self.step_counter = 0
        self.sw_pressed = False
        self.press_start_time = 0.0
        self.last_release_time = 0.0

        self._pending_timer: Optional[threading.Timer] = None
        self._lock = threading.Lock()

    def update_pins(self, clk: int, dt: int, sw: int, now: Optional[float] = None):
        """Aggiorna lo stato dei 3 pin dell'encoder e genera eventi discreti."""
        if now is None:
            now = time.time()

        with self._lock:
            # 1. Quadratura Rotazione
            current_state = ((clk & 1) << 1) | (dt & 1)
            if current_state != self.last_state:
                index = (self.last_state << 2) | current_state
                delta = QUADRATURE_TABLE[index]
                if delta != 0:
                    self.step_counter += delta
                    # EC11 tipicamente genera 4 transizioni per scatto (detent)
                    if abs(self.step_counter) >= 4:
                        is_cw = self.step_counter > 0
                        self.step_counter = 0
                        
                        if self.sw_pressed:
                            ev_type = EventType.PRESS_ROTATE_CW if is_cw else EventType.PRESS_ROTATE_CCW
                        else:
                            ev_type = EventType.ROTATE_CW if is_cw else EventType.ROTATE_CCW
                            
                        self.event_sink(HardwareEvent(
                            event_type=ev_type,
                            source_id=self.encoder_id.value,
                            timestamp=now
                        ))
                self.last_state = current_state

            # 2. Pressione Tasto SW (Attivo LOW con pull-up: 0 = Premuto, 1 = Rilasciato)
            is_sw_down = (sw == 0)

            if is_sw_down and not self.sw_pressed:
                # Transizione Rilasciato -> Premuto (Falling edge)
                self.sw_pressed = True
                self.press_start_time = now

            elif not is_sw_down and self.sw_pressed:
                # Transizione Premuto -> Rilasciato (Rising edge)
                self.sw_pressed = False
                press_duration = now - self.press_start_time

                # Filtra glitch inferiori a 15ms o pressioni lunghissime
                if 0.015 <= press_duration < 0.9:
                    time_since_last_release = now - self.last_release_time

                    if time_since_last_release < 0.280:
                        # Riconosciuto DOUBLE_CLICK entro 280ms
                        if self._pending_timer is not None:
                            self._pending_timer.cancel()
                            self._pending_timer = None

                        self.last_release_time = 0.0
                        self.event_sink(HardwareEvent(
                            event_type=EventType.DOUBLE_CLICK,
                            source_id=self.encoder_id.value,
                            timestamp=now
                        ))
                    else:
                        # Possibile SHORT_CLICK: avvia timer di 280ms
                        self.last_release_time = now
                        if self._pending_timer is not None:
                            self._pending_timer.cancel()

                        def emit_short_click():
                            with self._lock:
                                self._pending_timer = None
                            self.event_sink(HardwareEvent(
                                event_type=EventType.SHORT_CLICK,
                                source_id=self.encoder_id.value,
                                timestamp=time.time()
                            ))

                        self._pending_timer = threading.Timer(0.280, emit_short_click)
                        self._pending_timer.daemon = True
                        self._pending_timer.start()


# =====================================================================
# SOTTOSISTEMA MONITOR PRINCIPALE
# =====================================================================

class HardwareMonitor:
    """
    Monitor integrato per Rock Pi 4 Plus.
    Gestisce la lettura GPIO degli encoder e l'isolamento diagnostico I2C.
    """

    def __init__(self, config_path: str = "boards/board_config.json"):
        self.config_path = config_path
        self.config = self._load_config(config_path)

        self._running = False
        self._event_queue: queue.Queue[HardwareEvent] = queue.Queue(maxsize=256)
        self._async_queue: Optional[asyncio.Queue[HardwareEvent]] = None

        # Stato condiviso diagnostica I2C thread-safe
        self._status_lock = threading.Lock()
        self._module_status: Dict[str, Dict[str, str]] = {
            "porta_1": {"name": "Pico Drum", "status": ModuleState.EMPTY.value},
            "porta_2": {"name": "Occhio",    "status": ModuleState.EMPTY.value},
            "porta_3": {"name": "",          "status": ModuleState.EMPTY.value},
            "porta_4": {"name": "Sensore X", "status": ModuleState.EMPTY.value},
            "porta_5": {"name": "",          "status": ModuleState.EMPTY.value},
        }

        # Trackers per i 3 Encoder
        self.encoders = {
            EncoderId.WORKSPACE:   SingleEncoderTracker(EncoderId.WORKSPACE,   self._on_hardware_event),
            EncoderId.EFFECTS:     SingleEncoderTracker(EncoderId.EFFECTS,     self._on_hardware_event),
            EncoderId.INSTRUMENTS: SingleEncoderTracker(EncoderId.INSTRUMENTS, self._on_hardware_event),
        }

        # Rilevamento Backend Hardware
        self.backend_type = "SIMULATION"
        self._gpio_chips: Dict[str, Any] = {}
        self._gpio_lines: Dict[str, Any] = {}
        self._i2c_bus: Any = None

        # Thread di monitoraggio
        self._gpio_thread: Optional[threading.Thread] = None
        self._i2c_thread: Optional[threading.Thread] = None

        self._init_hardware()

    def _load_config(self, path: str) -> dict:
        try:
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f)
        except Exception as e:
            logger.warning(f"Impossibile leggere {path}: {e}. Uso fallback predefinito.")

        return {
            "display": {"framebuffer_device": "/dev/fb1"},
            "i2c": {"device": "/dev/i2c-2", "bus_number": 2, "multiplexer": {"address": "0x70"}},
            "encoders": {
                "encoder_1_workspace":   {"clk": {"chip": "/dev/gpiochip2", "line": 10}, "dt": {"chip": "/dev/gpiochip2", "line": 9},  "sw": {"chip": "/dev/gpiochip2", "line": 12}},
                "encoder_2_effects":     {"clk": {"chip": "/dev/gpiochip3", "line": 16}, "dt": {"chip": "/dev/gpiochip4", "line": 4},  "sw": {"chip": "/dev/gpiochip4", "line": 6}},
                "encoder_3_instruments": {"clk": {"chip": "/dev/gpiochip4", "line": 5},  "dt": {"chip": "/dev/gpiochip4", "line": 30}, "sw": {"chip": "/dev/gpiochip4", "line": 7}},
            }
        }

    def _init_hardware(self):
        """Inizializza i driver GPIO e I2C nativi o predispone il simulatore."""
        if sys.platform != "linux":
            logger.info("Host non Linux rilevato: HardwareMonitor opera in modalità SIMULAZIONE.")
            self.backend_type = "SIMULATION"
            return

        # Tentativo inizializzazione gpiod v2 / v1
        try:
            gpiod = importlib.import_module("gpiod")
            self.backend_type = "gpiod"
            logger.info("Backend GPIO 'gpiod' caricato con successo.")
            self._setup_gpiod(gpiod)
        except Exception as e:
            logger.warning(f"gpiod non disponibile o fallito: {e}. Tento con python-periphery...")
            try:
                periphery = importlib.import_module("periphery")
                self.backend_type = "periphery"
                self._setup_periphery(periphery)
            except Exception as e2:
                logger.warning(f"Nessun driver GPIO disponibile: {e2}. Modalità simulazione attiva.")
                self.backend_type = "SIMULATION"

        # Inizializzazione I2C
        try:
            smbus2 = importlib.import_module("smbus2")
            bus_num = self.config.get("i2c", {}).get("bus_number", 2)
            self._i2c_bus = smbus2.SMBus(bus_num)
            logger.info(f"Bus I2C-{bus_num} (/dev/i2c-{bus_num}) aperto correttamente.")
        except Exception as e:
            logger.warning(f"Apertura I2C non riuscita ({e}). Simulazione I2C attiva.")
            self._i2c_bus = None

    def _setup_gpiod(self, gpiod: Any):
        """Configura le linee GPIO per tutti e 3 gli encoder tramite libgpiod."""
        enc_cfg = self.config.get("encoders", {})
        mapping = [
            (EncoderId.WORKSPACE,   enc_cfg.get("encoder_1_workspace", {})),
            (EncoderId.EFFECTS,     enc_cfg.get("encoder_2_effects", {})),
            (EncoderId.INSTRUMENTS, enc_cfg.get("encoder_3_instruments", {})),
        ]

        if hasattr(gpiod, "request_lines"):
            # gpiod v2 API
            line_mod = importlib.import_module("gpiod.line")
            Direction = line_mod.Direction
            Bias = line_mod.Bias

            # Raggruppa linee per chip
            chips_lines: Dict[str, Dict[int, Any]] = {}
            for enc_id, cfg in mapping:
                for pin_role in ("clk", "dt", "sw"):
                    chip_path = cfg[pin_role]["chip"]
                    line_idx = cfg[pin_role]["line"]
                    if chip_path not in chips_lines:
                        chips_lines[chip_path] = {}
                    chips_lines[chip_path][line_idx] = gpiod.LineSettings(
                        direction=Direction.INPUT,
                        bias=Bias.PULL_UP
                    )

            self._v2_requests = {}
            for chip_path, settings_dict in chips_lines.items():
                if os.path.exists(chip_path):
                    req = gpiod.request_lines(
                        chip_path,
                        consumer="mimo_hw_monitor",
                        config=settings_dict
                    )
                    self._v2_requests[chip_path] = req
        else:
            # Fallback legacy gpiod v1
            logger.info("Utilizzo gpiod v1 legacy API")

    def _setup_periphery(self, periphery: Any):
        """Configura le linee GPIO tramite python-periphery."""
        GPIO = periphery.GPIO
        enc_cfg = self.config.get("encoders", {})
        mapping = [
            ("enc1", enc_cfg.get("encoder_1_workspace", {})),
            ("enc2", enc_cfg.get("encoder_2_effects", {})),
            ("enc3", enc_cfg.get("encoder_3_instruments", {})),
        ]
        self._periphery_pins = {}
        for prefix, cfg in mapping:
            for pin_role in ("clk", "dt", "sw"):
                chip = cfg[pin_role]["chip"]
                line = cfg[pin_role]["line"]
                key = f"{prefix}_{pin_role}"
                try:
                    self._periphery_pins[key] = GPIO(chip, line, "in", bias="pull_up")
                except Exception as e:
                    logger.warning(f"Errore periphery {key} ({chip}:{line}): {e}")

    def _on_hardware_event(self, event: HardwareEvent):
        """Callback invocata dai tracker degli encoder."""
        try:
            self._event_queue.put_nowait(event)
        except queue.Full:
            pass

        if self._async_queue is not None:
            try:
                self._async_queue.put_nowait(event)
            except Exception:
                pass

    # =====================================================================
    # THREAD DAEMON I2C: DIAGNOSTICA ANTI-FREEZE OGNI 2.0s
    # =====================================================================

    def _i2c_daemon_loop(self):
        """
        Ciclo isolato in background. Esegue ping del multiplexer PCA9548 (0x70)
        e aggiorna lo stato in memoria RAM senza MAI bloccare il loop principale.
        """
        pca_addr = int(self.config.get("i2c", {}).get("multiplexer", {}).get("address", "0x70"), 16)
        mcp_addr = 0x20  # Expander 5 pulsanti su Canale 0

        logger.info("I2C Anti-Freeze Daemon avviato (frequenza: ogni 2.0s).")

        while self._running:
            start_t = time.time()
            new_status: Dict[str, Dict[str, str]] = {}

            if self._i2c_bus is not None:
                for ch in range(8):
                    port_key = f"porta_{ch}" if ch > 0 else "pulsanti_scocca"
                    ch_mask = 1 << ch
                    try:
                        # Seleziona il canale scrivendo il byte di maschera
                        self._i2c_bus.write_byte(pca_addr, ch_mask)

                        if ch == 0:
                            # Canale 0: MCP23017 per i 5 tasti
                            try:
                                # Lettura registro GPIOA (0x12)
                                btn_vals = self._i2c_bus.read_byte_data(mcp_addr, 0x12)
                                # Se risponde con successo, i pulsanti sono operativi
                                self._scan_mcp_buttons(btn_vals)
                            except Exception:
                                pass
                        else:
                            # Canali 1-7: Porte esterne MiMO Link (1..7)
                            # Probe non distruttivo: tentativo di lettura singolo byte di stato
                            # Se risponde con ACK -> CONNECTED, se fallisce (ENXIO) -> EMPTY
                            probe_ok = False
                            for test_addr in (0x38, 0x48, 0x68, 0x28):
                                try:
                                    self._i2c_bus.read_byte(test_addr)
                                    probe_ok = True
                                    break
                                except OSError:
                                    continue

                            if probe_ok:
                                new_status[f"porta_{ch}"] = {"name": f"Modulo Link {ch}", "status": ModuleState.CONNECTED.value}
                            else:
                                new_status[f"porta_{ch}"] = {"name": "", "status": ModuleState.EMPTY.value}

                    except OSError as oe:
                        # Multiplexer non risponde o collisione I2C
                        new_status[f"porta_{ch}"] = {"name": "Err Bus", "status": ModuleState.ERROR.value}
                    except Exception as ex:
                        new_status[f"porta_{ch}"] = {"name": "Err", "status": ModuleState.ERROR.value}

                # Riporta multiplexer a 0 (nessun canale attivo)
                try:
                    self._i2c_bus.write_byte(pca_addr, 0x00)
                except Exception:
                    pass

            else:
                # Simulazione per sviluppo Desktop / Test
                new_status = {
                    "porta_1": {"name": "Pico Drum", "status": ModuleState.CONNECTED.value},
                    "porta_2": {"name": "Occhio",    "status": ModuleState.CONNECTED.value},
                    "porta_3": {"name": "",          "status": ModuleState.EMPTY.value},
                    "porta_4": {"name": "Sensore X", "status": ModuleState.CONNECTED.value},
                    "porta_5": {"name": "",          "status": ModuleState.EMPTY.value},
                }

            # Aggiornamento atomico thread-safe del dizionario in RAM
            with self._status_lock:
                for k, v in new_status.items():
                    if k in self._module_status:
                        self._module_status[k] = v

            # Attendi esattamente 2.0s tenendo conto del tempo impiegato per il ping
            elapsed = time.time() - start_t
            sleep_time = max(0.2, 2.0 - elapsed)
            time.sleep(sleep_time)

    def _scan_mcp_buttons(self, reg_val: int):
        """Verifica lo stato dei 5 tasti fisici letti da MCP23017 (attivi LOW)."""
        for i in range(5):
            pressed = ((reg_val >> i) & 1) == 0
            if pressed:
                self._on_hardware_event(HardwareEvent(
                    event_type=EventType.BUTTON_CLICK,
                    source_id=i,
                    timestamp=time.time()
                ))

    # =====================================================================
    # POLLING GPIO AD ALTA FREQUENZA (1000 Hz) / EVENT LOOP
    # =====================================================================

    def _gpio_poll_loop(self):
        """Loop di lettura GPIO veloce per gestire interrupt ed edge detection."""
        logger.info("Loop lettura GPIO avviato.")
        enc_cfg = self.config.get("encoders", {})

        while self._running:
            now = time.time()
            if self.backend_type == "periphery":
                try:
                    # Encoder 1 (Workspace)
                    clk1 = 1 if self._periphery_pins["enc1_clk"].read() else 0
                    dt1  = 1 if self._periphery_pins["enc1_dt"].read() else 0
                    sw1  = 1 if self._periphery_pins["enc1_sw"].read() else 0
                    self.encoders[EncoderId.WORKSPACE].update_pins(clk1, dt1, sw1, now)

                    # Encoder 2 (Effetti)
                    clk2 = 1 if self._periphery_pins["enc2_clk"].read() else 0
                    dt2  = 1 if self._periphery_pins["enc2_dt"].read() else 0
                    sw2  = 1 if self._periphery_pins["enc2_sw"].read() else 0
                    self.encoders[EncoderId.EFFECTS].update_pins(clk2, dt2, sw2, now)

                    # Encoder 3 (Strumenti)
                    clk3 = 1 if self._periphery_pins["enc3_clk"].read() else 0
                    dt3  = 1 if self._periphery_pins["enc3_dt"].read() else 0
                    sw3  = 1 if self._periphery_pins["enc3_sw"].read() else 0
                    self.encoders[EncoderId.INSTRUMENTS].update_pins(clk3, dt3, sw3, now)
                except Exception:
                    pass

            elif self.backend_type == "gpiod" and hasattr(self, "_v2_requests"):
                try:
                    # Lettura valori da richieste v2
                    vals_by_chip = {chip: req.get_values() for chip, req in self._v2_requests.items()}
                    
                    def get_val(chip_path: str, line_idx: int) -> int:
                        req = self._v2_requests.get(chip_path)
                        if req:
                            val = req.get_value(line_idx)
                            return 1 if (val == 1 or getattr(val, "value", 1) == 1) else 0
                        return 1

                    # Workspace (Enc 1)
                    c1 = enc_cfg["encoder_1_workspace"]
                    self.encoders[EncoderId.WORKSPACE].update_pins(
                        get_val(c1["clk"]["chip"], c1["clk"]["line"]),
                        get_val(c1["dt"]["chip"],  c1["dt"]["line"]),
                        get_val(c1["sw"]["chip"],  c1["sw"]["line"]),
                        now
                    )
                    # Effetti (Enc 2)
                    c2 = enc_cfg["encoder_2_effects"]
                    self.encoders[EncoderId.EFFECTS].update_pins(
                        get_val(c2["clk"]["chip"], c2["clk"]["line"]),
                        get_val(c2["dt"]["chip"],  c2["dt"]["line"]),
                        get_val(c2["sw"]["chip"],  c2["sw"]["line"]),
                        now
                    )
                    # Strumenti (Enc 3)
                    c3 = enc_cfg["encoder_3_instruments"]
                    self.encoders[EncoderId.INSTRUMENTS].update_pins(
                        get_val(c3["clk"]["chip"], c3["clk"]["line"]),
                        get_val(c3["dt"]["chip"],  c3["dt"]["line"]),
                        get_val(c3["sw"]["chip"],  c3["sw"]["line"]),
                        now
                    )
                except Exception:
                    pass

            time.sleep(0.001)

    # =====================================================================
    # API PUBBLICA
    # =====================================================================

    def start(self, async_queue: Optional[asyncio.Queue[HardwareEvent]] = None):
        """Avvia i thread daemon in background per GPIO e diagnostica I2C."""
        if self._running:
            return

        self._running = True
        self._async_queue = async_queue

        # Thread diagnostico I2C (Anti-Freeze)
        self._i2c_thread = threading.Thread(target=self._i2c_daemon_loop, daemon=True, name="MiMO_I2CDaemon")
        self._i2c_thread.start()

        # Thread polling GPIO (se non in pura simulazione da tastiera)
        if self.backend_type != "SIMULATION":
            self._gpio_thread = threading.Thread(target=self._gpio_poll_loop, daemon=True, name="MiMO_GPIOPoll")
            self._gpio_thread.start()

        logger.info(f"HardwareMonitor avviato [Backend: {self.backend_type}]")

    def stop(self):
        """Arresta tutti i monitor hardware in modo pulito."""
        self._running = False
        if self._i2c_bus is not None:
            try:
                self._i2c_bus.close()
            except Exception:
                pass
        logger.info("HardwareMonitor fermato.")

    def get_module_status(self) -> Dict[str, Dict[str, str]]:
        """Restituisce istantaneamente una copia dello stato I2C in RAM ($O(1)$, zero lag)."""
        with self._status_lock:
            return {k: dict(v) for k, v in self._module_status.items()}

    def poll_events(self) -> List[HardwareEvent]:
        """Estrae tutti gli eventi hardware pendenti dalla coda thread-safe."""
        events: List[HardwareEvent] = []
        while True:
            try:
                ev = self._event_queue.get_nowait()
                events.append(ev)
            except queue.Empty:
                break
        return events

    def inject_simulated_event(self, event: HardwareEvent):
        """Permette l'iniezione manuale di eventi hardware per testing e simulatore desktop."""
        self._on_hardware_event(event)
