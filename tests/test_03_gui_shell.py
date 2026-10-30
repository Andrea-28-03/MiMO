#!/usr/bin/env python3
"""
MiMO - Test 03: Prototipo Shell Grafica Ufficiale (480x320) & Simulatore Eventi
Modulo: @GUI / @LOGIC / @TESTER

Descrizione:
Implementa il layout grafico ufficiale definito in `Architecture/02_AGENT_GUI.md`:
  1. BARRA SUPERIORE (40 px): Workspace (STUDIO / LIVE), Target/Preset, PC Sync (MiMO Hub).
  2. AREA CENTRALE (220 px):
     - Vista HOME STUDIO: Strumento selezionato + Tabella Effetti a colonne compatte.
     - Vista HOME LIVE: Signal Chain a blocchi interconnessi + Controlli rapidi.
  3. BARRA INFERIORE (60 px): 5 Colonne Moduli Hardware con indicatori 10x10 px
     gestiti da un Thread Daemon Anti-Freeze in background (aggiornamento ogni 2.0s).

Funziona sia su Rock Pi 4 (Framebuffer SPI /dev/fb1 o /dev/fb0 anche via SSH tramite mmap RGB565)
sia su PC Desktop (Windows/macOS/Linux) in finestra 480x320 per sviluppo rapido.

Controlli Simulati (Tastiera in finestra o via console SSH):
  - [Spazio] / [W] : Cambia Workspace (STUDIO <-> LIVE) [Simula Encoder 3]
  - [Frecce SX/DX] : Scorri elemento selezionato [Simula Rotazione Encoder]
  - [Frecce SU/GIU]: Modifica valore parametro (+5% / -5%) [Simula Push + Turn]
  - [Invio] / [B]  : Attiva/Disattiva (Bypass) effetto o conferma [Simula Click Encoder]
  - [1..5]         : Seleziona Porta Modulo (in STUDIO) o Scena (in LIVE) [5 Pulsanti Fisici]
  - [H]            : Simula connessione/disconnessione PC "MiMO Hub" (Pallino Verde/Grigio)
  - [E]            : Simula errore/ripristino sul Modulo 4 (Test Anti-Freeze Rosso/Verde)
  - [Q] / [ESC]    : Chiudi e pulisci il display
"""

import glob
import mmap
import os
import platform
import struct
import sys
import threading
import time

# Dimensioni fisiche display CUQI 3.5" SPI
SCREEN_WIDTH = 480
SCREEN_HEIGHT = 320
DEFAULT_FB = "/dev/fb1"


# =====================================================================
# MOTORE TEMI PERSONALIZZABILI (@GUI)
# Chiunque può aggiungere o modificare i colori di MiMO in questa classe!
# =====================================================================

class Theme:
    """Rappresenta una palette colori completa per l'interfaccia di MiMO."""

    def __init__(
        self,
        name: str,
        active_item=(255, 255, 255),      # BIANCO PURO per filtri ed elementi attivi!
        focus_accent=(245, 158, 11),     # Ambra per il focus dell'encoder
        live_accent=(255, 255, 255),      # Bianco per la modalità Live
        bg=(0, 0, 0),                     # Sfondo nero assoluto per display OLED/IPS
        card=(24, 24, 27),                # Grigio antracite per riquadri
        card_sel=(38, 38, 44),            # Card evidenziata con focus
        border=(39, 39, 42),              # Bordo discreto
        text=(255, 255, 255),             # Testo bianco brillante
        text_dim=(160, 160, 168),         # Testo secondario grigio
        green_ok=(16, 185, 129),          # Verde per moduli e switch OK
        red_alert=(239, 68, 68),          # Rosso per errori e alert
        gray_muted=(113, 113, 122),       # Grigio per elementi in Bypass
        dark_gray=(60, 60, 68),           # Grigio scuro per parti spente
    ):
        self.name = name
        self.active_item = active_item
        self.focus_accent = focus_accent
        self.live_accent = live_accent
        self.bg = bg
        self.card = card
        self.card_sel = card_sel
        self.border = border
        self.text = text
        self.text_dim = text_dim
        self.green_ok = green_ok
        self.red_alert = red_alert
        self.gray_muted = gray_muted
        self.dark_gray = dark_gray


# Lista Temi Ufficiali e Accessibili a Tutti (Ciclabili con il tasto 'T')
THEMES = [
    # 1. IL NUOVO DEFAULT: Filtri attivi in BIANCO PURO, Focus in Ambra calda
    Theme(
        name="STUDIO_WHITE",
        active_item=(255, 255, 255),
        focus_accent=(245, 158, 11),
        live_accent=(255, 255, 255),
    ),
    # 2. MONOCROMATICO PURO (Stile sintetizzatori hardware analogici)
    Theme(
        name="PURE_MONO",
        active_item=(255, 255, 255),
        focus_accent=(215, 215, 225),
        live_accent=(255, 255, 255),
    ),
    # 3. AMBRA CALDA / VALVOLARE
    Theme(
        name="WARM_AMBER",
        active_item=(245, 158, 11),
        focus_accent=(255, 255, 255),
        live_accent=(245, 158, 11),
    ),
    # 4. CYBER CIANO CLASSICO
    Theme(
        name="CYBER_CYAN",
        active_item=(6, 182, 212),
        focus_accent=(245, 158, 11),
        live_accent=(6, 182, 212),
    ),
    # 5. VERDE SMERALDO VINTAGE
    Theme(
        name="EMERALD_MINT",
        active_item=(16, 185, 129),
        focus_accent=(255, 255, 255),
        live_accent=(16, 185, 129),
    ),
    # 6. RETRO VAPOR PURPLE
    Theme(
        name="RETRO_PURPLE",
        active_item=(192, 132, 252),
        focus_accent=(245, 158, 11),
        live_accent=(192, 132, 252),
    ),
]


# Retrocompatibilità per costanti base
COL_BG = THEMES[0].bg
COL_CARD = THEMES[0].card
COL_CARD_SEL = THEMES[0].card_sel
COL_BORDER = THEMES[0].border
COL_GREEN = THEMES[0].green_ok
COL_WHITE = THEMES[0].active_item
COL_AMBER = THEMES[0].focus_accent
COL_RED = THEMES[0].red_alert
COL_GRAY = THEMES[0].gray_muted
COL_DARK_GRAY = THEMES[0].dark_gray
COL_TEXT = THEMES[0].text
COL_TEXT_DIM = THEMES[0].text_dim


# =====================================================================
# 1. STATO CONDIVISO & THREAD DAEMON ANTI-FREEZE (@HARDWARE -> @GUI)
# =====================================================================

class SharedSystemState:
    """Contiene lo stato globale letto istantaneamente da Pygame (Zero-Freeze)."""

    def __init__(self):
        self.lock = threading.Lock()
        self.running = True

        # Workspace attivo: "STUDIO" oppure "LIVE"
        self.workspace = "STUDIO"
        self.hub_connected = False  # True = Pallino Verde, False = Grigio

        # Modulo/Scena selezionato (0..4 corrispondenti ai 5 pulsanti fisici)
        self.selected_slot = 0

        # Strumenti assegnati ai moduli
        self.instruments = [
            "JUNO-60 POLY SYNTH",
            "PICO DRUM KIT 808",
            "THEREMIN OCCHIO AI",
            "SUB BASS MOOG",
            "GRANULAR PAD",
        ]

        # Scene per la modalità LIVE
        self.live_scenes = [
            "SCENA 1: INTRO AMBIENT",
            "SCENA 2: VERSE GROOVE",
            "SCENA 3: LEAD SOLO_01",
            "SCENA 4: DROP BASS",
            "SCENA 5: OUTRO DELAY",
        ]

        # Lista completa ed espansa degli Effetti (16 Moduli DSP da Studio)
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

        # Inizializza i parametri dettagliati (3 controlli) per ciascun effetto DSP
        for fx in self.effects:
            fx["params"] = [
                {"name": "WET / DRY MIX", "val": fx["val"], "unit": "%"},
                {"name": "TIME / RATE",   "val": 50,         "unit": "ms"},
                {"name": "TONE / DAMP",   "val": 70,         "unit": "%"},
            ]

        # Finestra di visualizzazione a colonna singola (indice del primo elemento visibile)
        self.window_start = 0

        # Indice dell'effetto/blocco attualmente sotto il cursore dell'encoder
        self.focused_index = 0
        # Flag per ridisegnare SOLO quando c'è una reale modifica di stato
        self.needs_redraw = True
        # Regione orizzontale sporca (y_min, y_max) da aggiornare (Dirty Rectangles)
        self.dirty_region = (0, SCREEN_HEIGHT)

        # Libreria Strumenti per la rotazione di Encoder 1 (Frecce DX/SX)
        self.instrument_library = [
            "JUNO-60 POLY SYNTH",
            "PICO DRUM KIT 808",
            "MINIMOOG SUB BASS",
            "THEREMIN OCCHIO AI",
            "GRANULAR PAD VOX",
            "DX7 ELECTRIC PIANO",
            "MELLOTRON FLUTE",
            "TB-303 ACID LINE",
        ]

        # Modal/Sottomenu attivo su Doppio Click (None, "INSTRUMENT", "EFFECT")
        self.modal_mode = None
        self.modal_param_idx = 0
        self.last_click_enc1 = 0.0
        self.last_click_enc2 = 0.0

        # Parametri sonori dettagliati per ciascuno strumento (modificabili in modale)
        self.instrument_params = {
            "JUNO-60 POLY SYNTH": [
                {"name": "FILTER CUTOFF", "val": 75, "unit": "%"},
                {"name": "ATTACK TIME",   "val": 20, "unit": "ms"},
                {"name": "RESONANCE",     "val": 45, "unit": "%"},
            ],
            "PICO DRUM KIT 808": [
                {"name": "PITCH TUNE",    "val": 50, "unit": "%"},
                {"name": "DECAY TIME",    "val": 65, "unit": "ms"},
                {"name": "SNAP / CLAP",   "val": 80, "unit": "%"},
            ],
            "MINIMOOG SUB BASS": [
                {"name": "SUB OSC LEVEL", "val": 85, "unit": "%"},
                {"name": "FILTER GLIDE",  "val": 30, "unit": "ms"},
                {"name": "OVERDRIVE",     "val": 40, "unit": "%"},
            ],
            "THEREMIN OCCHIO AI": [
                {"name": "SENSITIVITY",   "val": 80, "unit": "%"},
                {"name": "SMOOTHING",     "val": 40, "unit": "ms"},
                {"name": "PITCH QUANT",   "val": 100, "unit": "%"},
            ],
            "GRANULAR PAD VOX": [
                {"name": "GRAIN SIZE",    "val": 55, "unit": "ms"},
                {"name": "DENSITY",       "val": 70, "unit": "%"},
                {"name": "SHIMMER REVERB","val": 60, "unit": "%"},
            ],
            "DX7 ELECTRIC PIANO": [
                {"name": "FM BRIGHTNESS", "val": 65, "unit": "%"},
                {"name": "VELOCITY SENS", "val": 80, "unit": "%"},
                {"name": "CHORUS DETUNE", "val": 35, "unit": "%"},
            ],
            "MELLOTRON FLUTE": [
                {"name": "TAPE WARBLE",   "val": 45, "unit": "%"},
                {"name": "MECH NOISE",    "val": 25, "unit": "%"},
                {"name": "TONE WARMTH",   "val": 75, "unit": "%"},
            ],
            "TB-303 ACID LINE": [
                {"name": "CUTOFF FREQ",   "val": 85, "unit": "%"},
                {"name": "RESONANCE ACID","val": 90, "unit": "%"},
                {"name": "ENV MOD DEPTH", "val": 70, "unit": "%"},
            ],
        }

        # Dizionario Stato Moduli (aggiornato dal Daemon Background ogni 2.0s)
        self.stato_moduli = {
            "porta_1": {"nome": "Pico Drum", "stato": "OK"},      # Verde
            "porta_2": {"nome": "Occhio AI", "stato": "OK"},      # Verde
            "porta_3": {"nome": "",          "stato": "VUOTA"},   # Grigio
            "porta_4": {"nome": "Sensore X", "stato": "ERRORE"},  # Rosso
            "porta_5": {"nome": "",          "stato": "VUOTA"},   # Grigio
        }

        # Tema cromatico attivo (indice in THEMES, ciclabile col tasto T)
        self.theme_idx = 0

    def get_instrument_params(self, inst_name: str):
        if inst_name not in self.instrument_params:
            self.instrument_params[inst_name] = [
                {"name": "FILTER CUTOFF", "val": 60, "unit": "%"},
                {"name": "ATTACK TIME",   "val": 30, "unit": "ms"},
                {"name": "RESONANCE",     "val": 50, "unit": "%"},
            ]
        return self.instrument_params[inst_name]

    @property
    def theme(self) -> Theme:
        return THEMES[self.theme_idx % len(THEMES)]


def hardware_health_daemon(state: SharedSystemState):
    """
    Thread Daemon in background che simula il ping ogni 2.0 secondi
    alle porte del multiplexer I2C (PCA9548) senza mai bloccare la GUI.
    """
    while state.running:
        # In produzione qui avverrà la scansione I2C non bloccante per la GUI
        time.sleep(2.0)


# =====================================================================
# 2. GESTIONE FRAMEBUFFER MMAP RGB565 (PER ROCK PI HEADLESS / SSH)
# =====================================================================

class FramebufferWriter:
    """
    Scrive nel display SPI in blocchi controllati (chunk di 20 righe = 19.200 byte)
    con micro-pause di 2ms. Questo azzera i buffer overrun del controller ILI9486,
    elimina il glitch e rende il passaggio tra STUDIO e LIVE fluido e stabile.
    """

    def __init__(self, fb_path: str, width: int = 480, height: int = 320):
        self.fb_path = fb_path
        self.width = width
        self.height = height
        self.fb_size = width * height * 2  # 307200 byte (RGB565)
        self.chunk_size = width * 2 * 20   # 20 righe per blocco = 19.200 byte
        self.fd = os.open(fb_path, os.O_RDWR)

    def update_full(self, surface):
        self.update_region(surface, 0, self.height)

    def update_region(self, surface, y_start: int, y_end: int):
        """
        Aggiorna ESCLUSIVAMENTE la fascia orizzontale compresa tra y_start e y_end
        in un'UNICA scrittura atomica (Line-Aligned al byte).
        In questo modo il driver fbtft imposta la finestra hardware X/Y (set_addr_win)
        UNA SOLA VOLTA e scarica i pixel via DMA continuo senza perdere la sincronizzazione.
        """
        bytes_per_line = self.width * 2  # 960 byte
        y_start = max(0, min(y_start, self.height))
        y_end = max(y_start, min(y_end, self.height))
        if y_start >= y_end:
            return

        start_offset = y_start * bytes_per_line
        end_offset = y_end * bytes_per_line
        raw_bytes = surface.get_buffer().raw[start_offset:end_offset]

        os.lseek(self.fd, start_offset, os.SEEK_SET)
        os.write(self.fd, raw_bytes)

    def clear(self):
        """Pulisce completamente il display a blocchi fino al nero assoluto."""
        black = b"\x00" * self.chunk_size
        os.lseek(self.fd, 0, os.SEEK_SET)
        for offset in range(0, self.fb_size, self.chunk_size):
            end = min(offset + self.chunk_size, self.fb_size)
            os.write(self.fd, black[:end - offset])
            time.sleep(0.002)

    def close(self):
        try:
            self.clear()
            time.sleep(0.05)
            os.close(self.fd)
        except Exception:
            pass


# =====================================================================
# 3. MOTORE DI RENDERING GRAFICO (@GUI - 480x320 px)
# =====================================================================

class MiMOGuiRenderer:
    """Disegna le 3 fasce rigide della GUI MiMO sulla Surface 480x320."""

    def __init__(self, pygame_mod, surface):
        self.pg = pygame_mod
        self.surf = surface

        self.pg.font.init()
        # Font ad alta leggibilità con anti-aliasing per display 3.5"
        self.font_badge = self.pg.font.SysFont("consolas,monospace,dejavusansmono", 15, bold=True)
        self.font_title = self.pg.font.SysFont("consolas,monospace,dejavusansmono", 16, bold=True)
        self.font_effect_large = self.pg.font.SysFont("consolas,monospace,dejavusansmono", 19, bold=True)
        self.font_main = self.pg.font.SysFont("consolas,monospace,dejavusansmono", 13, bold=True)
        self.font_small = self.pg.font.SysFont("consolas,monospace,dejavusansmono", 11, bold=False)

    def render(self, state: SharedSystemState, y_start: int = 0, y_end: int = 320):
        th = state.theme
        with state.lock:
            if y_start == 0 and y_end >= 320:
                self.surf.fill(th.bg)
                self._draw_top_bar(state)
                if state.workspace == "STUDIO":
                    self._draw_center_studio(state)
                else:
                    self._draw_center_live(state)
                self._draw_bottom_bar(state)
            else:
                # Aggiornamento mirato Dirty Rectangles: ridisegna solo le fasce interessate!
                if y_start < 40:
                    self._draw_top_bar(state)
                if y_start < 260 and y_end > 40:
                    self.pg.draw.rect(self.surf, th.bg, (0, 40, SCREEN_WIDTH, 220))
                    if state.workspace == "STUDIO":
                        self._draw_center_studio(state)
                    else:
                        self._draw_center_live(state)
                if y_end > 260:
                    self._draw_bottom_bar(state)

            # Se è aperto un sottomenu modale (Doppio Click su Encoder 1 o Encoder 2)
            if state.modal_mode:
                self._draw_modal(state)

    # -----------------------------------------------------------------
    # FASCIA 1: BARRA SUPERIORE (Altezza: 40 px, Y: 0..39)
    # -----------------------------------------------------------------
    def _draw_top_bar(self, state: SharedSystemState):
        pg = self.pg
        th = state.theme
        # Sfondo barra superiore e linea divisoria
        pg.draw.rect(self.surf, th.card, (0, 0, SCREEN_WIDTH, 40))
        pg.draw.line(self.surf, th.border, (0, 39), (SCREEN_WIDTH, 39), 2)

        # 1. Sinistra: Indicatore Workspace (STUDIO = Focus Accent / Ambra, LIVE = Live Accent / Bianco)
        ws_color = th.focus_accent if state.workspace == "STUDIO" else th.live_accent
        pg.draw.rect(self.surf, ws_color, (8, 7, 82, 26), border_radius=4)
        ws_txt = self.font_badge.render(state.workspace, True, th.bg)
        self.surf.blit(
            ws_txt,
            (8 + (82 - ws_txt.get_width()) // 2, 7 + (26 - ws_txt.get_height()) // 2),
        )

        # 2. Centro: Contestuale (Strumento in STUDIO, Preset in LIVE)
        if state.workspace == "STUDIO":
            center_label = f"MOD {state.selected_slot + 1}: {state.instruments[state.selected_slot]}"
        else:
            center_label = state.live_scenes[state.selected_slot]

        c_txt = self.font_title.render(center_label, True, th.text)
        self.surf.blit(c_txt, ((SCREEN_WIDTH - c_txt.get_width()) // 2, 11))

        # 3. Destra: Indicatore Connessione PC (MiMO Hub) + Tema attivo
        hub_label = self.font_small.render("HUB", True, th.text if state.hub_connected else th.gray_muted)
        self.surf.blit(hub_label, (SCREEN_WIDTH - 58, 14))

        dot_color = th.green_ok if state.hub_connected else th.dark_gray
        pg.draw.circle(self.surf, dot_color, (SCREEN_WIDTH - 18, 20), 6)
        pg.draw.circle(self.surf, th.border, (SCREEN_WIDTH - 18, 20), 6, 1)

    # -----------------------------------------------------------------
    # FASCIA 2A: AREA CENTRALE - HOME STUDIO (Colonna Singola a Scorrimento Discreto)
    # -----------------------------------------------------------------
    def _draw_center_studio(self, state: SharedSystemState):
        pg = self.pg
        th = state.theme

        visible_rows = 3
        total_effects = len(state.effects)

        # Calcolo finestra di scorrimento a scatto (Slot windowing)
        if state.focused_index < state.window_start:
            state.window_start = state.focused_index
        elif state.focused_index >= state.window_start + visible_rows:
            state.window_start = state.focused_index - visible_rows + 1

        max_start = max(0, total_effects - visible_rows)
        state.window_start = max(0, min(state.window_start, max_start))

        # Layout Colonna Singola Larga (444 px su 480 totali)
        start_x = 14
        col_w = 444
        row_h = 62
        start_y = 48
        gap_y = 7

        # Disegna esattamente le 3 righe visibili che scalano a scatto
        for slot in range(visible_rows):
            eff_idx = state.window_start + slot
            if eff_idx >= total_effects:
                break

            fx = state.effects[eff_idx]
            y = start_y + slot * (row_h + gap_y)
            is_focused = (eff_idx == state.focused_index)

            # Sfondo e bordo
            bg_col = th.card_sel if is_focused else th.card
            # Se attivo: BIANCO PURO (th.active_item); se con focus: AMBRA (th.focus_accent); se bypass: bordo discreto
            border_col = th.focus_accent if is_focused else (th.active_item if fx["active"] else th.border)
            border_w = 2 if is_focused else 1

            pg.draw.rect(self.surf, bg_col, (start_x, y, col_w, row_h), border_radius=6)
            pg.draw.rect(self.surf, border_col, (start_x, y, col_w, row_h), border_w, border_radius=6)

            # Basetta laterale accesa per l'elemento con focus
            if is_focused:
                pg.draw.rect(self.surf, th.focus_accent, (start_x, y, 4, row_h), border_top_left_radius=6, border_bottom_left_radius=6)
                arrow = self.font_main.render("▶", True, th.focus_accent)
                self.surf.blit(arrow, (start_x + 8, y + 10))

            # Pallino di stato (ON verde / BYPASS grigio scuro)
            dot_x = start_x + (26 if is_focused else 16)
            status_col = th.green_ok if fx["active"] else th.dark_gray
            pg.draw.circle(self.surf, status_col, (dot_x, y + 18), 5)

            # Indice progressivo (es. 01., 02., 16.)
            idx_str = f"{eff_idx + 1:02d}."
            idx_txt = self.font_main.render(idx_str, True, th.text_dim)
            self.surf.blit(idx_txt, (dot_x + 10, y + 11))

            # Nome Effetto PIÙ IN GRANDE (19pt bold) in BIANCO PURO (th.active_item)
            name_x = dot_x + 40
            name_col = th.active_item if fx["active"] else th.gray_muted
            n_txt = self.font_effect_large.render(fx["name"], True, name_col)
            self.surf.blit(n_txt, (name_x, y + 6))

            # Badge Categoria a destra (pulito, senza percentuali)
            cat_txt = self.font_main.render(f"[{fx['cat']}]", True, th.active_item if fx["active"] else th.text_dim)
            self.surf.blit(cat_txt, (start_x + col_w - cat_txt.get_width() - 14, y + 9))

            # Grande Barra di Progresso Orizzontale a piena larghezza (14 px altezza) SENZA percentuali
            bar_x = start_x + 16
            bar_y = y + 36
            bar_w = col_w - 32
            bar_h = 14
            pg.draw.rect(self.surf, th.bg, (bar_x, bar_y, bar_w, bar_h), border_radius=4)
            fill_w = int((bar_w * fx["val"]) / 100)
            bar_color = th.focus_accent if is_focused else (th.active_item if fx["active"] else th.dark_gray)
            if fill_w > 0:
                pg.draw.rect(self.surf, bar_color, (bar_x, bar_y, fill_w, bar_h), border_radius=4)
            pg.draw.rect(self.surf, th.border, (bar_x, bar_y, bar_w, bar_h), 1, border_radius=4)

        # Scrollbar laterale discreta sulla destra dello schermo (466 px)
        sb_x = 466
        sb_y = 48
        sb_w = 4
        sb_h = 200
        pg.draw.rect(self.surf, (28, 28, 32), (sb_x, sb_y, sb_w, sb_h), border_radius=2)
        thumb_h = max(24, int(sb_h * visible_rows / total_effects))
        thumb_y = sb_y + int((sb_h - thumb_h) * state.focused_index / (total_effects - 1))
        pg.draw.rect(self.surf, th.focus_accent, (sb_x, thumb_y, sb_w, thumb_h), border_radius=2)

    # -----------------------------------------------------------------
    # FASCIA 2B: AREA CENTRALE - HOME LIVE (Altezza: 220 px, Y: 40..259)
    # -----------------------------------------------------------------
    def _draw_center_live(self, state: SharedSystemState):
        pg = self.pg
        th = state.theme

        title_txt = self.font_main.render("SIGNAL CHAIN LIVE (PERFORMANCE MODE)", True, th.live_accent)
        self.surf.blit(title_txt, (12, 48))

        # Catena a 4 blocchi orizzontali: [Occhio AI] -> [Pico Drum] -> [FX Chain] -> [Master]
        chain_blocks = [
            {"title": "OCCHIO AI",  "sub": "60 FPS OSC", "active": True, "color": th.green_ok},
            {"title": "PICO DRUM",  "sub": state.instruments[state.selected_slot][:10], "active": True, "color": th.focus_accent},
            {
                "title": state.effects[state.focused_index]["name"][:10],
                "sub": "ACTIVE" if state.effects[state.focused_index]["active"] else "BYPASS",
                "active": state.effects[state.focused_index]["active"],
                "color": th.active_item,
            },
            {"title": "MASTER OUT", "sub": "0.0 dB", "active": True, "color": th.green_ok},
        ]

        box_w = 98
        box_h = 68
        start_x = 12
        y_box = 78
        gap = 21

        for i, blk in enumerate(chain_blocks):
            bx = start_x + i * (box_w + gap)

            # Linea di connessione al blocco successivo
            if i < len(chain_blocks) - 1:
                line_x1 = bx + box_w
                line_x2 = bx + box_w + gap
                line_y = y_box + box_h // 2
                pg.draw.line(self.surf, th.live_accent if blk["active"] else th.dark_gray, (line_x1, line_y), (line_x2, line_y), 3)

            # Sfondo e bordo del blocco
            is_fx_block = (i == 2)
            bg_c = (28, 28, 34) if (blk["active"] and is_fx_block) else (th.card_sel if blk["active"] else th.card)
            b_col = blk["color"] if blk["active"] else th.dark_gray
            pg.draw.rect(self.surf, bg_c, (bx, y_box, box_w, box_h), border_radius=6)
            pg.draw.rect(self.surf, b_col, (bx, y_box, box_w, box_h), 2 if blk["active"] else 1, border_radius=6)

            t1 = self.font_main.render(blk["title"], True, th.text if blk["active"] else th.gray_muted)
            self.surf.blit(t1, (bx + (box_w - t1.get_width()) // 2, y_box + 14))

            t2 = self.font_small.render(blk["sub"] if blk["active"] else "BYPASSED", True, b_col if blk["active"] else th.gray_muted)
            self.surf.blit(t2, (bx + (box_w - t2.get_width()) // 2, y_box + 38))

        # Riquadro inferiore: Controllo Rapido Effetti in Live (Focus Encoder 2)
        panel_y = 162
        pg.draw.rect(self.surf, th.card, (12, panel_y, SCREEN_WIDTH - 24, 86), border_radius=6)
        pg.draw.rect(self.surf, th.border, (12, panel_y, SCREEN_WIDTH - 24, 86), 1, border_radius=6)

        fx = state.effects[state.focused_index]
        lbl = self.font_main.render(
            f"FOCUS LIVE FX [{state.focused_index + 1}/{len(state.effects)}]: {fx['name']}",
            True,
            th.active_item if fx["active"] else th.gray_muted,
        )
        self.surf.blit(lbl, (24, panel_y + 10))

        state_str = "ACTIVE (ON)" if fx["active"] else "BYPASSED (OFF)"
        st_lbl = self.font_main.render(state_str, True, th.green_ok if fx["active"] else th.red_alert)
        self.surf.blit(st_lbl, (SCREEN_WIDTH - st_lbl.get_width() - 24, panel_y + 10))

        # Grande Slider per Push + Turn in Live (senza numeri di percentuale)
        bar_x, bar_y, bar_w, bar_h = 24, panel_y + 38, SCREEN_WIDTH - 48, 18
        pg.draw.rect(self.surf, th.bg, (bar_x, bar_y, bar_w, bar_h), border_radius=4)
        fill_w = int((bar_w * fx["val"]) / 100)
        if fill_w > 0:
            pg.draw.rect(self.surf, th.active_item if fx["active"] else th.dark_gray, (bar_x, bar_y, fill_w, bar_h), border_radius=4)
        pg.draw.rect(self.surf, th.border, (bar_x, bar_y, bar_w, bar_h), 1, border_radius=4)

        val_txt = self.font_small.render(
            "PARAM LEVEL (W / S per regolare | B per Bypass | [T] Cambia Tema)",
            True,
            th.text_dim,
        )
        self.surf.blit(val_txt, (24, panel_y + 62))

    # -----------------------------------------------------------------
    # FASCIA 3: BARRA INFERIORE - 5 MODULI HARDWARE (Altezza: 60 px, Y: 260..319)
    # -----------------------------------------------------------------
    def _draw_bottom_bar(self, state: SharedSystemState):
        pg = self.pg
        th = state.theme
        y_top = 260
        col_w = SCREEN_WIDTH // 5  # 96 px per ciascuna delle 5 porte

        pg.draw.rect(self.surf, th.card, (0, y_top, SCREEN_WIDTH, 60))
        pg.draw.line(self.surf, th.border, (0, y_top), (SCREEN_WIDTH, y_top), 2)

        keys = ["porta_1", "porta_2", "porta_3", "porta_4", "porta_5"]
        for i, key in enumerate(keys):
            x_left = i * col_w
            mod_info = state.stato_moduli[key]
            nome = mod_info["nome"]
            stato = mod_info["stato"]

            # Evidenzia la colonna se corrisponde allo slot selezionato (1..5)
            if i == state.selected_slot:
                pg.draw.rect(self.surf, th.card_sel, (x_left + 2, y_top + 3, col_w - 4, 54), border_radius=4)
                pg.draw.rect(self.surf, th.focus_accent if state.workspace == "STUDIO" else th.live_accent, (x_left + 2, y_top + 3, col_w - 4, 54), 1, border_radius=4)

            # Separatore verticale
            if i > 0:
                pg.draw.line(self.surf, th.border, (x_left, y_top + 6), (x_left, 314), 1)

            # Colore del pallino (10x10 pixel -> raggio 5)
            if stato == "OK":
                dot_col = th.green_ok
            elif stato == "ERRORE":
                dot_col = th.red_alert
            else:
                dot_col = th.dark_gray

            # Disegna Cerchio indicatore (diametro 10px = raggio 5px) e numero porta
            dot_x = x_left + 16
            dot_y = y_top + 18
            pg.draw.circle(self.surf, dot_col, (dot_x, dot_y), 5)

            p_txt = self.font_small.render(f"P{i + 1}", True, th.text_dim)
            self.surf.blit(p_txt, (dot_x + 10, y_top + 11))

            # Etichetta nome modulo (se porta vuota, lascia pulito/nero come da specifica)
            if nome:
                n_txt = self.font_small.render(nome[:11], True, th.text if stato == "OK" else th.red_alert)
                self.surf.blit(n_txt, (x_left + 8, y_top + 34))

    # -----------------------------------------------------------------
    # FASCIA POPUP / MODALE SOTTOMENU (Attivata da Doppio Click)
    # -----------------------------------------------------------------
    def _draw_modal(self, state: SharedSystemState):
        pg = self.pg
        th = state.theme
        mx, my, mw, mh = 30, 46, 420, 212

        if state.modal_mode == "INSTRUMENT":
            # Modale Impostazioni Strumento (Doppio Click su Encoder 1)
            inst_name = state.instruments[state.selected_slot]
            params = state.get_instrument_params(inst_name)

            pg.draw.rect(self.surf, (15, 15, 18), (mx, my, mw, mh), border_radius=8)
            pg.draw.rect(self.surf, th.focus_accent, (mx, my, mw, mh), 2, border_radius=8)

            t = self.font_title.render("⚙ DETTAGLIO STRUMENTO (ENC 1)", True, th.focus_accent)
            self.surf.blit(t, (mx + (mw - t.get_width()) // 2, my + 10))

            name_lbl = self.font_main.render(inst_name, True, th.text)
            self.surf.blit(name_lbl, (mx + (mw - name_lbl.get_width()) // 2, my + 30))
            pg.draw.line(self.surf, th.border, (mx + 12, my + 48), (mx + mw - 12, my + 48), 1)

            for pi, p in enumerate(params):
                py = my + 54 + pi * 44
                is_selected = (pi == state.modal_param_idx)

                # Riquadro di selezione per il parametro attivo
                if is_selected:
                    pg.draw.rect(self.surf, th.card_sel, (mx + 12, py - 2, mw - 24, 38), border_radius=5)
                    pg.draw.rect(self.surf, th.focus_accent, (mx + 12, py - 2, mw - 24, 38), 1, border_radius=5)
                    prefix = "► "
                    text_color = th.focus_accent
                    val_color = th.focus_accent
                    bar_color = th.focus_accent
                else:
                    prefix = "  "
                    text_color = th.text
                    val_color = th.text_dim
                    bar_color = th.border

                pname_txt = prefix + p["name"]
                unit = p.get("unit", "%")
                pval_str = f"{p['val']} {unit}"

                self.surf.blit(self.font_main.render(pname_txt, True, text_color), (mx + 20, py + 1))
                self.surf.blit(self.font_main.render(pval_str, True, val_color), (mx + mw - 85, py + 1))

                # Barra di riempimento parametro
                pg.draw.rect(self.surf, th.bg, (mx + 20, py + 20, mw - 40, 10), border_radius=3)
                fill_w = int((mw - 40) * (p["val"] / 100.0))
                if fill_w > 0:
                    pg.draw.rect(self.surf, bar_color, (mx + 20, py + 20, fill_w, 10), border_radius=3)

            hint = self.font_small.render("[SU/GIÙ o A/D] Cursore  |  [SX/DX o W/S] Regola (+/-)  |  [ESC/Doppio Click] Chiudi", True, th.text_dim)
            self.surf.blit(hint, (mx + (mw - hint.get_width()) // 2, my + mh - 18))

        elif state.modal_mode == "EFFECT":
            # Modale Impostazioni Effetto (Doppio Click su Encoder 2)
            fx = state.effects[state.focused_index]
            params = fx.get("params", [])

            pg.draw.rect(self.surf, (15, 15, 18), (mx, my, mw, mh), border_radius=8)
            pg.draw.rect(self.surf, th.active_item, (mx, my, mw, mh), 2, border_radius=8)

            t = self.font_title.render("⚙ DETTAGLIO EFFETTO (ENC 2)", True, th.active_item)
            self.surf.blit(t, (mx + (mw - t.get_width()) // 2, my + 10))

            status_str = "ATTIVO" if fx.get("active", True) else "BYPASS"
            status_color = (34, 197, 94) if fx.get("active", True) else (140, 140, 140)
            name_lbl = self.font_main.render(f"{fx['name']} [{fx['cat']}]  ({status_str})", True, status_color)
            self.surf.blit(name_lbl, (mx + (mw - name_lbl.get_width()) // 2, my + 30))
            pg.draw.line(self.surf, th.border, (mx + 12, my + 48), (mx + mw - 12, my + 48), 1)

            for pi, p in enumerate(params):
                py = my + 54 + pi * 44
                is_selected = (pi == state.modal_param_idx)

                if is_selected:
                    pg.draw.rect(self.surf, th.card_sel, (mx + 12, py - 2, mw - 24, 38), border_radius=5)
                    pg.draw.rect(self.surf, th.active_item, (mx + 12, py - 2, mw - 24, 38), 1, border_radius=5)
                    prefix = "► "
                    text_color = th.active_item
                    val_color = th.active_item
                    bar_color = th.active_item
                else:
                    prefix = "  "
                    text_color = th.text
                    val_color = th.text_dim
                    bar_color = th.border

                pname_txt = prefix + p["name"]
                unit = p.get("unit", "%")
                pval_str = f"{p['val']} {unit}"

                self.surf.blit(self.font_main.render(pname_txt, True, text_color), (mx + 20, py + 1))
                self.surf.blit(self.font_main.render(pval_str, True, val_color), (mx + mw - 85, py + 1))

                pg.draw.rect(self.surf, th.bg, (mx + 20, py + 20, mw - 40, 10), border_radius=3)
                fill_w = int((mw - 40) * (p["val"] / 100.0))
                if fill_w > 0:
                    pg.draw.rect(self.surf, bar_color, (mx + 20, py + 20, fill_w, 10), border_radius=3)

            hint = self.font_small.render("[SU/GIÙ o A/D] Cursore  |  [SX/DX o W/S] Regola (+/-)  |  [B] Bypass  |  [ESC] Chiudi", True, th.text_dim)
            self.surf.blit(hint, (mx + (mw - hint.get_width()) // 2, my + mh - 18))


# =====================================================================
# 4. GESTIONE INPUT SIMULATI E AVVIO MULTI-PIATTAFORMA
# =====================================================================

def handle_key_Action(key_name: str, state: SharedSystemState):
    """Applica un comando simulato allo stato condiviso con Dirty Region mirata."""
    now = time.time()
    with state.lock:
        # =============================================================
        # 1. GESTIONE FOCUS MODALE ATTIVO (INSTRUMENT o EFFECT)
        # Se una finestra modale è aperta, TUTTI i controlli agiscono
        # sulla finestra modale senza alterare la schermata sottostante!
        # =============================================================
        if state.modal_mode in ("INSTRUMENT", "EFFECT"):
            # A) Chiusura immediata della modale (ESC o Backspace)
            if key_name in ("escape", "esc", "backspace"):
                state.modal_mode = None
                state.dirty_region = (0, SCREEN_HEIGHT)
                state.needs_redraw = True
                return

            # Parametri associati alla modale
            if state.modal_mode == "INSTRUMENT":
                inst_name = state.instruments[state.selected_slot]
                params = state.get_instrument_params(inst_name)
            else:
                fx = state.effects[state.focused_index]
                params = fx.get("params", [])

            num_params = len(params)

            # B) Gestione Tasto INVIO (Encoder 1) nella modale Strumento
            if key_name == "return" and state.modal_mode == "INSTRUMENT":
                if (now - state.last_click_enc1) < 0.35:
                    # Doppio click: Chiude la modale
                    state.modal_mode = None
                    state.last_click_enc1 = 0.0
                    state.dirty_region = (0, SCREEN_HEIGHT)
                    state.needs_redraw = True
                    return
                state.last_click_enc1 = now
                # Click singolo: passa al parametro successivo
                if num_params > 0:
                    state.modal_param_idx = (state.modal_param_idx + 1) % num_params
                    state.dirty_region = (40, 260)
                    state.needs_redraw = True
                return

            # C) Gestione Tasto B (Encoder 2) nella modale Effetto
            elif key_name in ("b", "f") and state.modal_mode == "EFFECT":
                if (now - state.last_click_enc2) < 0.35:
                    # Doppio click: Chiude la modale
                    state.modal_mode = None
                    state.last_click_enc2 = 0.0
                    state.dirty_region = (0, SCREEN_HEIGHT)
                    state.needs_redraw = True
                    return
                state.last_click_enc2 = now
                # Click singolo: Toggle Bypass dell'effetto corrente
                fx = state.effects[state.focused_index]
                fx["active"] = not fx["active"]
                state.dirty_region = (40, 260)
                state.needs_redraw = True
                return

            # D) Spostamento Cursore tra i parametri (Navigazione Su / Giù):
            # Supporta frecce SU / GIÙ e tasti A / D
            if key_name in ("up", "a"):
                if num_params > 0:
                    state.modal_param_idx = (state.modal_param_idx - 1) % num_params
                state.dirty_region = (40, 260)
                state.needs_redraw = True
                return

            elif key_name in ("down", "d"):
                if num_params > 0:
                    state.modal_param_idx = (state.modal_param_idx + 1) % num_params
                state.dirty_region = (40, 260)
                state.needs_redraw = True
                return

            # E) Modifica Valore Parametro Selezionato (+5% / -5%):
            # Supporta frecce DESTRA / SINISTRA, tasti W / S e '+' / '-'
            elif key_name in ("right", "w", "+", "="):
                if num_params > 0:
                    p = params[state.modal_param_idx]
                    p["val"] = min(100, p["val"] + 5)
                    # Sincronizza il valore principale se è il mix dell'effetto
                    if state.modal_mode == "EFFECT" and state.modal_param_idx == 0:
                        state.effects[state.focused_index]["val"] = p["val"]
                state.dirty_region = (40, 260)
                state.needs_redraw = True
                return

            elif key_name in ("left", "s", "-", "_"):
                if num_params > 0:
                    p = params[state.modal_param_idx]
                    p["val"] = max(0, p["val"] - 5)
                    if state.modal_mode == "EFFECT" and state.modal_param_idx == 0:
                        state.effects[state.focused_index]["val"] = p["val"]
                state.dirty_region = (40, 260)
                state.needs_redraw = True
                return

            # F) Tasto Q in modale: chiude la modale senza uscire dall'applicazione
            elif key_name == "q":
                state.modal_mode = None
                state.dirty_region = (0, SCREEN_HEIGHT)
                state.needs_redraw = True
                return

            # G) Tasti Scocca 1..5 o Spazio/Tab: chiudono la modale prima di cambiare slot/workspace
            elif key_name in ("space", "tab"):
                state.modal_mode = None
                state.workspace = "LIVE" if state.workspace == "STUDIO" else "STUDIO"
                state.dirty_region = (0, SCREEN_HEIGHT)
                state.needs_redraw = True
                return

            elif key_name in ("1", "2", "3", "4", "5"):
                state.modal_mode = None
                state.selected_slot = int(key_name) - 1
                state.dirty_region = (0, SCREEN_HEIGHT)
                state.needs_redraw = True
                return

            # Tutti gli altri tasti vengono assorbiti dalla modale attiva
            return

        # =============================================================
        # 2. GESTIONE CONTROLLI SCHERMATA PRINCIPALE (Nessuna modale attiva)
        # =============================================================

        # 1. ROTELLA WORKSPACE (ENCODER 3): Spazio / Tab
        if key_name in ("space", "tab"):
            state.workspace = "LIVE" if state.workspace == "STUDIO" else "STUDIO"
            state.modal_mode = None
            state.dirty_region = (0, SCREEN_HEIGHT)

        # 2. ROTELLA STRUMENTI (ENCODER 1): FRECCE DELLA TASTIERA
        elif key_name == "right":
            # Ruota la manopola strumenti in senso ORARIO (+1)
            curr = state.instruments[state.selected_slot]
            if curr in state.instrument_library:
                idx = state.instrument_library.index(curr)
                state.instruments[state.selected_slot] = state.instrument_library[(idx + 1) % len(state.instrument_library)]
            else:
                state.instruments[state.selected_slot] = state.instrument_library[0]
            state.dirty_region = (0, SCREEN_HEIGHT)

        elif key_name == "left":
            # Ruota la manopola strumenti in senso ANTIORARIO (-1)
            curr = state.instruments[state.selected_slot]
            if curr in state.instrument_library:
                idx = state.instrument_library.index(curr)
                state.instruments[state.selected_slot] = state.instrument_library[(idx - 1) % len(state.instrument_library)]
            else:
                state.instruments[state.selected_slot] = state.instrument_library[0]
            state.dirty_region = (0, SCREEN_HEIGHT)

        elif key_name == "return":
            # Click su Encoder 1 (Strumenti) con rilevazione DOPPIO CLICK (<350ms)
            if (now - state.last_click_enc1) < 0.35:
                # DOPPIO CLICK: Apre Sottomenu Dettaglio Strumento!
                state.modal_mode = "INSTRUMENT"
                state.modal_param_idx = 0
                state.last_click_enc1 = 0.0
                state.dirty_region = (0, SCREEN_HEIGHT)
            else:
                state.last_click_enc1 = now
            state.dirty_region = (0, SCREEN_HEIGHT)

        # 3. ROTELLA EFFETTI (ENCODER 2): WASD + TASTO B/E
        elif key_name == "d":
            # Ruota la manopola effetti in senso ORARIO: scorre l'effetto successivo
            state.focused_index = (state.focused_index + 1) % len(state.effects)
            state.dirty_region = (40, 260)

        elif key_name == "a":
            # Ruota la manopola effetti in senso ANTIORARIO: scorre l'effetto precedente
            state.focused_index = (state.focused_index - 1) % len(state.effects)
            state.dirty_region = (40, 260)

        elif key_name == "w":
            # Push + Turn su Encoder 2: Incrementa il Valore / Mix dell'effetto (+5%)
            fx = state.effects[state.focused_index]
            fx["val"] = min(100, fx["val"] + 5)
            if "params" in fx and len(fx["params"]) > 0:
                fx["params"][0]["val"] = fx["val"]
            state.dirty_region = (40, 260)

        elif key_name == "s":
            # Push + Turn su Encoder 2: Decrementa il Valore / Mix dell'effetto (-5%)
            fx = state.effects[state.focused_index]
            fx["val"] = max(0, fx["val"] - 5)
            if "params" in fx and len(fx["params"]) > 0:
                fx["params"][0]["val"] = fx["val"]
            state.dirty_region = (40, 260)

        elif key_name in ("b", "e", "f"):
            # Click su Encoder 2 (Effetti) con rilevazione DOPPIO CLICK (<350ms)
            if (now - state.last_click_enc2) < 0.35:
                # DOPPIO CLICK: Apre Sottomenu Dettaglio Avanzato Effetto!
                state.modal_mode = "EFFECT"
                state.modal_param_idx = 0
                state.last_click_enc2 = 0.0
                state.dirty_region = (0, SCREEN_HEIGHT)
            else:
                # Click Singolo: Toggle Bypass Effetto (ON/OFF)
                fx = state.effects[state.focused_index]
                fx["active"] = not fx["active"]
                state.dirty_region = (40, 260)
                state.last_click_enc2 = now

        # 4. ALTRE FUNZIONI
        elif key_name == "t":
            state.theme_idx = (state.theme_idx + 1) % len(THEMES)
            print(f"[*] Tema cambiato: {state.theme.name}")
            state.dirty_region = (0, SCREEN_HEIGHT)
        elif key_name == "h":
            state.hub_connected = not state.hub_connected
            state.dirty_region = (0, 40)
        elif key_name == "e":
            cur = state.stato_moduli["porta_4"]["stato"]
            state.stato_moduli["porta_4"]["stato"] = "OK" if cur == "ERRORE" else "ERRORE"
            state.dirty_region = (260, 320)
        elif key_name in ("1", "2", "3", "4", "5"):
            state.selected_slot = int(key_name) - 1
            state.modal_mode = None
            state.dirty_region = (0, SCREEN_HEIGHT)
        elif key_name in ("escape", "esc", "q"):
            state.running = False

        state.needs_redraw = True


def find_framebuffer():
    fbs = sorted(glob.glob("/dev/fb*"))
    if DEFAULT_FB in fbs:
        return DEFAULT_FB
    return fbs[0] if fbs else None


def configure_linux_console(enable_cursor: bool = False):
    """Disabilita/abilita il cursore lampeggiante della console Linux che interferisce con il framebuffer."""
    if platform.system() != "Linux":
        return
    try:
        val = "1\n" if enable_cursor else "0\n"
        for p in ("/sys/class/graphics/fbcon/cursor_blink", "/sys/devices/virtual/graphics/fbcon/cursor_blink"):
            if os.path.exists(p):
                with open(p, "w") as f:
                    f.write(val)
    except Exception:
        pass
    try:
        flag = "on" if enable_cursor else "off"
        os.system(f"setterm -cursor {flag} -blank 0 -powerdown 0 2>/dev/null")
    except Exception:
        pass


def main():
    print("=" * 65)
    print("   MiMO GUI SHELL (480x320) - STRUMENTI & EFFETTI INDIPENDENTI   ")
    print("=" * 65)
    print("Controlli Rotelle Encoder & Tasti:")
    print("  [FRECCE SX/DX]    -> ROTELLA STRUMENTI (Encoder 1): Cambia Strumento")
    print("  [INVIO]           -> Click: Seleziona | DOPPIO CLICK: Dettaglio Strumento")
    print("  [A / D]           -> ROTELLA EFFETTI (Encoder 2): Scorre tra gli Effetti")
    print("  [W / S]           -> Valore / Mix Effetto (+5% / -5%)")
    print("  [B]               -> Click: Bypass ON/OFF | DOPPIO CLICK: Dettaglio Effetto")
    print("  --- QUANDO SI APRE UNA FINESTRA IMPOSTAZIONI (MODALE) ---")
    print("  [FRECCE SU/GIÙ o A/D] -> Sposta il cursore tra i parametri")
    print("  [FRECCE SX/DX o W/S]  -> Modifica il valore del parametro selezionato (+/- 5%)")
    print("  [ESC o BACKSPACE]     -> Chiudi Impostazioni e torna alla schermata principale")
    print("  ---------------------------------------------------------")
    print("  [T]               -> Cambia TEMA Colore (Studio White, Pure Mono, Amber, etc.)")
    print("  [Spazio] o [Tab]  -> ROTELLA WORKSPACE (Encoder 3): STUDIO <-> LIVE")
    print("  [Tasti 1..5]      -> 5 Pulsanti Scocca: Seleziona Modulo / Scena")
    print("  [H]               -> Simula Connessione PC 'MiMO Hub' (Verde/Grigio)")
    print("  [Q]               -> Esci (e pulisci display al nero)\n")

    state = SharedSystemState()

    # Avvia il Thread Daemon Anti-Freeze in background
    daemon_thread = threading.Thread(target=hardware_health_daemon, args=(state,), daemon=True)
    daemon_thread.start()

    fb_dev = find_framebuffer() if platform.system() == "Linux" else None
    use_mmap_fb = False
    fb_writer = None

    import pygame

    if fb_dev and os.path.exists(fb_dev):
        print(f"[*] Rilevato Display Hardware Framebuffer sulla Rock Pi: {fb_dev}")
        print("[*] Attivazione Scrittura Sincrona RGB565 (16-bit) su Framebuffer...")
        os.environ["SDL_VIDEODRIVER"] = "dummy"
        pygame.init()
        # Superficie nativa a 16-bit RGB565 (Maschere R:0xF800, G:0x07E0, B:0x001F)
        screen = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT), 0, 16, (0xF800, 0x07E0, 0x001F, 0))
        fb_writer = FramebufferWriter(fb_dev, SCREEN_WIDTH, SCREEN_HEIGHT)
        use_mmap_fb = True
    else:
        print("[*] Avvio in modalità Finestra Simulatore Desktop (480x320)...")
        pygame.init()
        screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("MiMO 3.5\" GUI Shell (480x320) - [Spazio: STUDIO/LIVE | Frecce: Encoder]")

    renderer = MiMOGuiRenderer(pygame, screen)
    clock = pygame.time.Clock()

    # Se siamo in modalità headless SSH su Rock Pi, leggiamo anche i tasti da terminale SSH in modo robusto!
    if use_mmap_fb and sys.stdin.isatty():
        import select
        import termios
        import tty

        def ssh_keyboard_listener():
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            try:
                # setraw disabilita l'ECHO dei caratteri sulla console Linux (zero interferenze col display!)
                tty.setraw(fd)
                while state.running:
                    rlist, _, _ = select.select([sys.stdin], [], [], 0.05)
                    if not rlist:
                        continue

                    ch = sys.stdin.read(1)
                    if not ch:
                        break

                    # Ctrl+C in raw mode produce \x03
                    if ch in ("\x03", "q", "Q"):
                        handle_key_Action("q", state)
                        break

                    if ch == "\x1b":  # Sequenza di escape ANSI (Frecce)
                        r2, _, _ = select.select([sys.stdin], [], [], 0.2)
                        if r2:
                            seq = sys.stdin.read(1)
                            if seq in ("[", "O"):
                                r3, _, _ = select.select([sys.stdin], [], [], 0.2)
                                if r3:
                                    code = sys.stdin.read(1)
                                    if code == "A":
                                        handle_key_Action("up", state)
                                    elif code == "B":
                                        handle_key_Action("down", state)
                                    elif code == "C":
                                        handle_key_Action("right", state)
                                    elif code == "D":
                                        handle_key_Action("left", state)

                                    while True:
                                        r_drain, _, _ = select.select([sys.stdin], [], [], 0.02)
                                        if r_drain:
                                            sys.stdin.read(1)
                                        else:
                                            break
                        continue
                    elif ch in ("\r", "\n"):
                        handle_key_Action("return", state)
                    elif ch == "\t":
                        handle_key_Action("tab", state)
                    elif ch == " ":
                        handle_key_Action("space", state)
                    else:
                        handle_key_Action(ch.lower(), state)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

        kb_thread = threading.Thread(target=ssh_keyboard_listener, daemon=True)
        kb_thread.start()
        print("[OK] Listener tastiera SSH attivo (RAW mode, zero echo a video).")

    # Disattiva interferenze del cursore Linux
    configure_linux_console(enable_cursor=False)

    try:
        while state.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    state.running = False
                elif event.type == pygame.KEYDOWN:
                    key_map = {
                        pygame.K_SPACE: "space",
                        pygame.K_TAB: "tab",
                        pygame.K_w: "w",
                        pygame.K_s: "s",
                        pygame.K_a: "a",
                        pygame.K_d: "d",
                        pygame.K_t: "t",
                        pygame.K_RIGHT: "right",
                        pygame.K_LEFT: "left",
                        pygame.K_UP: "up",
                        pygame.K_DOWN: "down",
                        pygame.K_RETURN: "return",
                        pygame.K_b: "b",
                        pygame.K_h: "h",
                        pygame.K_e: "e",
                        pygame.K_1: "1",
                        pygame.K_2: "2",
                        pygame.K_3: "3",
                        pygame.K_4: "4",
                        pygame.K_5: "5",
                        pygame.K_q: "q",
                        pygame.K_ESCAPE: "escape",
                        pygame.K_BACKSPACE: "backspace",
                        pygame.K_PLUS: "+",
                        pygame.K_KP_PLUS: "+",
                        pygame.K_MINUS: "-",
                        pygame.K_KP_MINUS: "-",
                    }
                    if event.key in key_map:
                        handle_key_Action(key_map[event.key], state)

            # Ridisegna e invia al display SPI SOLO quando lo stato è cambiato
            with state.lock:
                should_draw = state.needs_redraw
                dirty_y = state.dirty_region
                state.needs_redraw = False
                state.dirty_region = (0, SCREEN_HEIGHT)

            if should_draw:
                y0, y1 = dirty_y
                renderer.render(state, y0, y1)
                if use_mmap_fb and fb_writer:
                    # Invia ESCLUSIVAMENTE la fascia che è cambiata (Dirty Rectangles!)
                    fb_writer.update_region(screen, y0, y1)
                else:
                    pygame.display.flip()

            clock.tick(30)

    except KeyboardInterrupt:
        print("\n[*] Interruzione manuale rilevata.")
    finally:
        state.running = False
        configure_linux_console(enable_cursor=True)
        if fb_writer:
            fb_writer.close()
        pygame.quit()
        print("[OK] Display pulito e applicazione terminata correttamente.")


if __name__ == "__main__":
    main()
