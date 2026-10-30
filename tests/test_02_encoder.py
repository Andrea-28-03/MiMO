#!/usr/bin/env python3
"""
MiMO - Test 02: Validazione Hardware Encoder Rotativo 1 (PATIKIL EC11)
Modulo: @HARDWARE / @TESTER

Pinout Rock Pi 4 Plus:
  - CLK: Pin 36 (GPIO4_A4 -> gpiochip4, linea 4)
  - DT:  Pin 38 (GPIO4_A6 -> gpiochip4, linea 6)
  - SW:  Pin 40 (GPIO4_A7 -> gpiochip4, linea 7)
  - GND: Pin 34 (Ground)

Supporta:
  1. Esecuzione su Rock Pi 4 con Linux (gpiod v1/v2 oppure python-periphery).
  2. Esecuzione su PC Desktop (Windows/Mac/Linux) con simulatore interattivo da tastiera.
"""

import importlib
import os
import platform
import sys
import time

CHIP_PATH = "/dev/gpiochip4"
LINE_CLK = 4
LINE_DT = 6
LINE_SW = 7

gpiod = None
periphery = None

def install_instructions():
    print("\n[!] Nessun backend GPIO hardware rilevato (gpiod o python-periphery).")
    print("    Se stai eseguendo il test sulla Rock Pi 4, installa le librerie con:")
    print("    sudo apt-get install -y python3-gpiod || sudo apt-get install -y python3-periphery\n")

# Caricamento dinamico dei backend hardware (evita errori del linter su PC/Windows)
BACKEND = None
try:
    gpiod = importlib.import_module("gpiod")
    BACKEND = "gpiod"
except ImportError:
    try:
        periphery = importlib.import_module("periphery")
        BACKEND = "periphery"
    except ImportError:
        BACKEND = None


class EncoderTester:
    def __init__(self):
        self.valore = 0
        self.sw_premuto = False
        self.ultimo_click_time = 0.0
        self.is_simulation = False

        if BACKEND == "periphery":
            self.init_periphery()
        elif BACKEND == "gpiod":
            self.init_gpiod()
        else:
            self.init_simulator()

    def init_periphery(self):
        print("[*] Inizializzazione tramite backend 'python-periphery'...")
        try:
            GPIO = periphery.GPIO
            self.gpio_clk = GPIO(CHIP_PATH, LINE_CLK, "in", bias="pull_up")
            self.gpio_dt = GPIO(CHIP_PATH, LINE_DT, "in", bias="pull_up")
            self.gpio_sw = GPIO(CHIP_PATH, LINE_SW, "in", bias="pull_up")
            self.last_state = (self.gpio_clk.read() << 1) | self.gpio_dt.read()
        except Exception as e:
            print(f"[!] Errore apertura GPIO periphery ({CHIP_PATH}): {e}")
            install_instructions()
            self.init_simulator()

    def init_gpiod(self):
        print(f"[*] Inizializzazione tramite backend 'gpiod' (v{getattr(gpiod, '__version__', '2+')})...")
        try:
            # Supporto gpiod v2 (standard su Debian Trixie / Armbian 6.x)
            if hasattr(gpiod, "request_lines"):
                line_mod = importlib.import_module("gpiod.line")
                Direction = line_mod.Direction
                Bias = line_mod.Bias
                self.request = gpiod.request_lines(
                    CHIP_PATH,
                    consumer="mimo_encoder1_test",
                    config={
                        LINE_CLK: gpiod.LineSettings(direction=Direction.INPUT, bias=Bias.PULL_UP),
                        LINE_DT:  gpiod.LineSettings(direction=Direction.INPUT, bias=Bias.PULL_UP),
                        LINE_SW:  gpiod.LineSettings(direction=Direction.INPUT, bias=Bias.PULL_UP),
                    }
                )
                vals = self.request.get_values()
                self.last_state = (vals[0] << 1) | vals[1]
            else:
                # Fallback legacy gpiod v1
                chip = gpiod.Chip("gpiochip4")
                self.lines = chip.get_lines([LINE_CLK, LINE_DT, LINE_SW])
                self.lines.request(consumer="mimo_encoder1_test", type=gpiod.LINE_REQ_DIR_IN, flags=gpiod.LINE_REQ_FLAG_BIAS_PULL_UP)
                vals = self.lines.get_values()
                self.last_state = (vals[0] << 1) | vals[1]
        except Exception as e:
            print(f"[!] Errore apertura GPIO gpiod ({CHIP_PATH}): {e}")
            install_instructions()
            self.init_simulator()

    def init_simulator(self):
        self.is_simulation = True
        self.last_state = 0
        print("[*] Modalità hardware GPIO non attiva su questo host.")
        print("[*] Attivazione MODALITÀ SIMULATORE DESKTOP (sviluppo/test logica).")

    def read_pins(self):
        if self.is_simulation:
            return 1, 1, 1

        if BACKEND == "periphery":
            clk = 1 if self.gpio_clk.read() else 0
            dt = 1 if self.gpio_dt.read() else 0
            sw = 1 if self.gpio_sw.read() else 0
            return clk, dt, sw
        else:
            if hasattr(self, "request"):
                vals = self.request.get_values()
                return vals[0].value if hasattr(vals[0], "value") else vals[0], \
                       vals[1].value if hasattr(vals[1], "value") else vals[1], \
                       vals[2].value if hasattr(vals[2], "value") else vals[2]
            else:
                vals = self.lines.get_values()
                return vals[0], vals[1], vals[2]

    def _simula_rotazione(self, delta):
        direction = "+1 ORARIO (CW) ->" if delta > 0 else "<- -1 ANTIORARIO (CCW)"
        self.valore += delta
        if self.sw_premuto:
            print(f"  [PUSH + TURN]  Valore: {self.valore:4d}  | Direzione: {direction}")
        else:
            print(f"  [ROTATION]     Valore: {self.valore:4d}  | Direzione: {direction}")

    def _simula_click(self):
        now = time.time()
        delta_click = now - self.ultimo_click_time
        if delta_click < 0.35:
            print("  >>> [EVENT: DOUBLE_CLICK] Doppio click rapido simulato! <<<")
        else:
            print("  >>> [EVENT: SHORT_CLICK]  Click simulato! <<<")
        self.ultimo_click_time = now

    def run_simulation(self):
        print("=" * 60)
        print("   MiMO HARDWARE DIAGNOSTICS - TEST 02: ENCODER 1 (SIMULATORE)   ")
        print("=" * 60)
        print("[*] Controlli da tastiera:")
        print("    - [Freccia Destra] o [D]  : Ruota Orario (+1)")
        print("    - [Freccia Sinistra] o [A]: Ruota Antiorario (-1)")
        print("    - [Invio] o [Spazio]      : Click Pulsante SW")
        print("    - [P]                     : Toggle PUSH (Simula Pressione continua)")
        print("    - [Q] o [Ctrl+C]          : Termina test\n")

        try:
            if platform.system() == "Windows":
                import msvcrt
                while True:
                    if msvcrt.kbhit():
                        ch = msvcrt.getch()
                        if ch in (b'\x00', b'\xe0'):
                            ch2 = msvcrt.getch()
                            if ch2 == b'M':  # Freccia Destra
                                self._simula_rotazione(1)
                            elif ch2 == b'K':  # Freccia Sinistra
                                self._simula_rotazione(-1)
                        elif ch in (b'd', b'D'):
                            self._simula_rotazione(1)
                        elif ch in (b'a', b'A'):
                            self._simula_rotazione(-1)
                        elif ch in (b' ', b'\r', b'\n'):
                            self._simula_click()
                        elif ch in (b'p', b'P'):
                            self.sw_premuto = not self.sw_premuto
                            stato = "PREMUTO" if self.sw_premuto else "RILASCIATO"
                            print(f"  [TOGGLE PUSH] Pulsante ora: {stato}")
                        elif ch in (b'q', b'Q', b'\x1b'):
                            break
                    time.sleep(0.01)
            else:
                import select
                import tty
                import termios
                fd = sys.stdin.fileno()
                old_settings = termios.tcgetattr(fd)
                try:
                    tty.setcbreak(fd)
                    while True:
                        rlist, _, _ = select.select([sys.stdin], [], [], 0.05)
                        if rlist:
                            char = sys.stdin.read(1)
                            if char == '\x1b':
                                rest = sys.stdin.read(2)
                                if rest == '[C':
                                    self._simula_rotazione(1)
                                elif rest == '[D':
                                    self._simula_rotazione(-1)
                                else:
                                    break
                            elif char in ('d', 'D'):
                                self._simula_rotazione(1)
                            elif char in ('a', 'A'):
                                self._simula_rotazione(-1)
                            elif char in (' ', '\n', '\r'):
                                self._simula_click()
                            elif char in ('p', 'P'):
                                self.sw_premuto = not self.sw_premuto
                                stato = "PREMUTO" if self.sw_premuto else "RILASCIATO"
                                print(f"  [TOGGLE PUSH] Pulsante ora: {stato}")
                            elif char in ('q', 'Q'):
                                break
                finally:
                    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        except KeyboardInterrupt:
            pass

        print("\n[*] Collaudo simulato Encoder 1 terminato.")
        print(f"[*] Valore finale accumulato: {self.valore}")
        print("=" * 60)

    def run(self):
        if self.is_simulation:
            self.run_simulation()
            return

        print("=" * 60)
        print("   MiMO HARDWARE DIAGNOSTICS - TEST 02: ENCODER 1 (PATIKIL)   ")
        print("=" * 60)
        print("[*] Mappatura: CLK=Pin 36 | DT=Pin 38 | SW=Pin 40 | GND=Pin 34")
        print("[*] Ruota la manopola, premi o fai doppio click...")
        print("[*] Premi Ctrl+C per terminare.\n")

        # Tabella di transizione Gray Code a 4 bit: [vecchio_stato(2bit) << 2 | nuovo_stato(2bit)]
        QUADRATURE_TABLE = [
             0, -1,  1,  0,
             1,  0,  0, -1,
            -1,  0,  0,  1,
             0,  1, -1,  0
        ]

        step_counter = 0

        try:
            while True:
                clk, dt, sw_val = self.read_pins()
                current_state = (clk << 1) | dt

                # 1. Rilevamento Quadratura Rotazione
                if current_state != self.last_state:
                    index = (self.last_state << 2) | current_state
                    delta = QUADRATURE_TABLE[index]
                    if delta != 0:
                        step_counter += delta
                        # La maggior parte degli encoder EC11 genera 4 transizioni per scatto (detent)
                        if abs(step_counter) >= 4:
                            direction = "+1 ORARIO (CW) ->" if step_counter > 0 else "<- -1 ANTIORARIO (CCW)"
                            self.valore += (1 if step_counter > 0 else -1)
                            step_counter = 0

                            if self.sw_premuto:
                                print(f"  [PUSH + TURN]  Valore: {self.valore:4d}  | Direzione: {direction}")
                            else:
                                print(f"  [ROTATION]     Valore: {self.valore:4d}  | Direzione: {direction}")

                    self.last_state = current_state

                # 2. Rilevamento Pressione Tasto (Attivo LOW, 0 = Premuto)
                is_pressed = (sw_val == 0)
                now = time.time()

                if is_pressed and not self.sw_premuto:
                    self.sw_premuto = True
                    time.sleep(0.02)  # Debounce rapido
                    delta_click = now - self.ultimo_click_time
                    if delta_click < 0.35:
                        print("  >>> [EVENT: DOUBLE_CLICK] Doppio click rapido rilevato! <<<")
                    else:
                        print("  >>> [EVENT: SHORT_CLICK]  Click rilevato! <<<")
                    self.ultimo_click_time = now

                elif not is_pressed and self.sw_premuto:
                    self.sw_premuto = False
                    time.sleep(0.02)

                # Ciclo di polling ad alta frequenza (1000 Hz) per zero lag
                time.sleep(0.001)

        except KeyboardInterrupt:
            print("\n[*] Collaudo Encoder 1 terminato.")
            print(f"[*] Valore finale accumulato: {self.valore}")
            print("=" * 60)


if __name__ == "__main__":
    tester = EncoderTester()
    tester.run()
