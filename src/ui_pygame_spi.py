#!/usr/bin/env python3
"""
MiMO - Sottosistema Interfaccia Grafica Display SPI (@GUI)
Target: Rock Pi 4 Plus (480x320 px su /dev/fb1) tramite pygame-ce

Regole Architetturali Rigorose (@GUI):
1. Motore: pygame-ce su Framebuffer /dev/fb1 (driver fb_ili9486).
2. Divieto Assoluto di Full Redraw (`flip` o `fill` a ogni ciclo).
   Implementazione obbligatoria del metodo Partial Update (Dirty Rectangles)
   tramite `pygame.display.update(dirty_rects)` passando SOLO le aree modificate.
3. Layout Rigoroso 480x320 diviso in 3 fasce orizzontali:
   - Top Bar (40px, Y: 0-40): Workspace a sinistra, contestuale al centro
     (Strumento in STUDIO, Nome Preset in LIVE), icona PC Sync a destra.
   - Center Area (220px, Y: 40-260): Dinamica (Home Studio, Home Live,
     Liste Selezione, Sottomenu Impostazioni) senza MAI ridisegnare le barre.
   - Bottom Bar (60px, Y: 260-320): 5 slot moduli con nome breve e cerchio
     geometrico 10x10 px che legge lo stato I2C in RAM (Verde/Grigio/Rosso).
"""

from __future__ import annotations

import logging
import os
import sys
import time
from typing import Dict, List, Optional, Set, Tuple

import pygame

from src.hardware_monitor import (
    EncoderId,
    EventType,
    HardwareEvent,
    HardwareMonitor,
    ModuleState,
)
from src.state_machine import (
    DirtyZone,
    MiMOStateMachine,
    StateSnapshot,
    ViewMode,
    Workspace,
)

logger = logging.getLogger("mimo.gui")
logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(asctime)s - %(message)s")

SCREEN_WIDTH = 480
SCREEN_HEIGHT = 320
TOP_BAR_HEIGHT = 40
CENTER_AREA_HEIGHT = 220
BOTTOM_BAR_HEIGHT = 60

RECT_TOP_BAR = pygame.Rect(0, 0, SCREEN_WIDTH, TOP_BAR_HEIGHT)
RECT_CENTER_AREA = pygame.Rect(0, TOP_BAR_HEIGHT, SCREEN_WIDTH, CENTER_AREA_HEIGHT)
RECT_BOTTOM_BAR = pygame.Rect(0, TOP_BAR_HEIGHT + CENTER_AREA_HEIGHT, SCREEN_WIDTH, BOTTOM_BAR_HEIGHT)


# =====================================================================
# TEMA UFFICIALE AD ALTO CONTRASTO (STUDIO_WHITE)
# =====================================================================

class StudioWhiteTheme:
    bg = (0, 0, 0)                  # Sfondo nero assoluto
    card = (24, 24, 27)             # Grigio antracite per superfici
    card_sel = (38, 38, 44)         # Card selezionata
    border = (39, 39, 42)           # Bordo discreto
    text = (255, 255, 255)          # Testo bianco brillante
    text_dim = (160, 160, 168)      # Testo secondario
    focus_amber = (245, 158, 11)    # Ambra calda per cursore/focus
    green_ok = (16, 185, 129)       # Verde smeraldo per moduli connessi
    red_alert = (239, 68, 68)       # Rosso per errori e timeout
    gray_muted = (113, 113, 122)    # Grigio neutro per vuoto o bypass
    dark_gray = (60, 60, 68)        # Grigio scuro per tracce inattive
    cyan_sync = (6, 182, 212)       # Ciano brillante per sync PC attivo


# =====================================================================
# RENDERER PRINCIPALE A DIRTY RECTANGLES (@GUI)
# =====================================================================

class MiMOViewSPI:
    """Gestisce il rendering headless e i parziali dirty rects su display SPI 3.5"."""

    def __init__(self, framebuffer_dev: str = "/dev/fb1", force_window: bool = False):
        self.framebuffer_dev = framebuffer_dev
        self.theme = StudioWhiteTheme()
        self.screen: pygame.Surface
        self.is_headless = False

        self._init_display(force_window)
        self._init_fonts()

        # Cache dell'ultimo snapshot per identificare variazioni minime
        self.last_snapshot: Optional[StateSnapshot] = None

    def _init_display(self, force_window: bool):
        """Inizializza il display su Linux Framebuffer (/dev/fb1) o apre finestra su Desktop."""
        pygame.init()
        pygame.font.init()

        if sys.platform == "linux" and os.path.exists(self.framebuffer_dev) and not force_window:
            os.environ["SDL_FBDEV"] = self.framebuffer_dev
            os.environ["SDL_VIDEODRIVER"] = "fbcon"
            try:
                self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT), pygame.FULLSCREEN)
                pygame.mouse.set_visible(False)
                self.is_headless = True
                logger.info(f"Rendering avviato su Framebuffer SPI: {self.framebuffer_dev}")
                return
            except Exception as e:
                logger.warning(f"Inizializzazione fbcon fallita ({e}). Fallback a modalità finestra/dummy.")

        # Modalità desktop o fallback
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("MiMO Base Centrale - 480x320 SPI Emulator")
        logger.info("Rendering avviato in finestra desktop (sviluppo/emulazione).")

    def _init_fonts(self):
        """Carica font di sistema monospazio ad alta definizione."""
        font_name = "consolas,dejavusansmono,monospace"
        self.font_title = pygame.font.SysFont(font_name, 16, bold=True)
        self.font_main = pygame.font.SysFont(font_name, 14, bold=True)
        self.font_large = pygame.font.SysFont(font_name, 18, bold=True)
        self.font_small = pygame.font.SysFont(font_name, 11, bold=False)
        self.font_badge = pygame.font.SysFont(font_name, 12, bold=True)

    # =====================================================================
    # AGGIORNAMENTO PARZIALE (DIRTY RECTANGLES)
    # =====================================================================

    def render(self, snapshot: StateSnapshot) -> List[pygame.Rect]:
        """
        Ridisegna ESCLUSIVAMENTE le aree sporche indicate dallo snapshot.
        Restituisce la lista dei Dirty Rectangles aggiornati a pygame.display.update().
        """
        dirty_rects: List[pygame.Rect] = []

        if DirtyZone.ALL in snapshot.dirty_zones or self.last_snapshot is None:
            # Full redraw solo all'avvio o al cambio radicale di Workspace
            self.screen.fill(self.theme.bg)
            self._draw_top_bar(snapshot)
            self._draw_center_area(snapshot)
            self._draw_bottom_bar(snapshot)
            dirty_rects.append(pygame.Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT))
        else:
            # Aggiornamento selettivo mirato (Zero Full Redraw)
            if DirtyZone.TOP_BAR in snapshot.dirty_zones:
                self._draw_top_bar(snapshot)
                dirty_rects.append(RECT_TOP_BAR)

            if DirtyZone.CENTER_AREA in snapshot.dirty_zones:
                self._draw_center_area(snapshot)
                dirty_rects.append(RECT_CENTER_AREA)

            if DirtyZone.BOTTOM_BAR in snapshot.dirty_zones:
                self._draw_bottom_bar(snapshot)
                dirty_rects.append(RECT_BOTTOM_BAR)

        if dirty_rects:
            # Chiamata obbligatoria a partial update: MAI flip() o fill() continuo!
            pygame.display.update(dirty_rects)

        self.last_snapshot = snapshot
        return dirty_rects

    # =====================================================================
    # 1. TOP BAR (40 px, Y: 0 - 40)
    # =====================================================================

    def _draw_top_bar(self, snap: StateSnapshot):
        th = self.theme
        pygame.draw.rect(self.screen, th.bg, RECT_TOP_BAR)
        pygame.draw.line(self.screen, th.border, (0, 39), (SCREEN_WIDTH, 39), 1)

        # 1.1 Sinistra: Workspace Attivo (con badge sblocco di sicurezza)
        ws_text = snap.workspace.value
        ws_col = th.focus_amber if snap.workspace_unlocked else th.text
        ws_surf = self.font_title.render(ws_text, True, ws_col)
        self.screen.blit(ws_surf, (16, 11))

        if snap.workspace_unlocked:
            lock_badge = self.font_badge.render("[SBLOCCATO]", True, th.focus_amber)
            self.screen.blit(lock_badge, (ws_surf.get_width() + 24, 13))

        # 1.2 Centro: Contestuale in base al Workspace
        if snap.workspace == Workspace.STUDIO:
            # Mostra lo Strumento Selezionato (dal modulo attivo)
            ctx_label = f"MOD {snap.selected_slot + 1}: {snap.current_instrument_name}"
            ctx_col = th.text
        else:
            # Mostra il Nome del Preset / Scena Attiva
            ctx_label = f"LIVE PRESET: {snap.current_preset_name}"
            ctx_col = th.focus_amber

        ctx_surf = self.font_main.render(ctx_label, True, ctx_col)
        ctx_x = (SCREEN_WIDTH - ctx_surf.get_width()) // 2
        self.screen.blit(ctx_surf, (ctx_x, 12))

        # 1.3 Destra: Indicatore Geometrico di Connessione al PC (MiMO Hub)
        # Cerchio colorato (Verde smeraldo / Ciano) se connesso, Grigio scuro spento se standalone
        sync_color = th.green_ok if snap.hub_connected else th.dark_gray
        sync_x = SCREEN_WIDTH - 24
        sync_y = 20
        pygame.draw.circle(self.screen, sync_color, (sync_x, sync_y), 6)
        if snap.hub_connected:
            pygame.draw.circle(self.screen, th.cyan_sync, (sync_x, sync_y), 8, 1)

    # =====================================================================
    # 2. CENTER AREA (220 px, Y: 40 - 260)
    # =====================================================================

    def _draw_center_area(self, snap: StateSnapshot):
        th = self.theme
        pygame.draw.rect(self.screen, th.bg, RECT_CENTER_AREA)

        if snap.view_mode == ViewMode.HOME:
            if snap.workspace == Workspace.STUDIO:
                self._draw_center_studio_home(snap)
            else:
                self._draw_center_live_home(snap)
        elif snap.view_mode == ViewMode.INSTRUMENT_LIST:
            self._draw_center_selection_list(
                title=f"SELEZIONA STRUMENTO (SLOT {snap.selected_slot + 1})",
                items=snap.instrument_library,
                selected_idx=snap.instrument_list_idx,
                hint="1 Click: Seleziona  |  Doppio Click: Impostazioni"
            )
        elif snap.view_mode == ViewMode.EFFECT_LIST:
            items_str = [f"{fx['name']} [{fx['cat']}]" for fx in snap.effects]
            hint_str = "1 Click: Seleziona | Doppio Click: Parametri" if snap.workspace == Workspace.STUDIO else "1 Click: Bypass ON/OFF | Doppio Click: Parametri"
            self._draw_center_selection_list(
                title="LISTA EFFETTI DISPONIBILI",
                items=items_str,
                selected_idx=snap.effect_list_idx,
                hint=hint_str
            )
        elif snap.view_mode in (ViewMode.INSTRUMENT_SETTINGS, ViewMode.EFFECT_SETTINGS):
            self._draw_center_settings(snap)

    def _draw_center_studio_home(self, snap: StateSnapshot):
        """Home STUDIO: Strumento selezionato in alto a sinistra, sotto elenco effetti con valori/barre."""
        th = self.theme

        # In alto a sinistra: Strumento selezionato
        inst_label = self.font_small.render("STRUMENTO ATTIVO", True, th.text_dim)
        self.screen.blit(inst_label, (16, 46))
        inst_name = self.font_large.render(snap.current_instrument_name, True, th.text)
        self.screen.blit(inst_name, (16, 60))

        # Linea divisoria interna
        pygame.draw.line(self.screen, th.border, (16, 84), (SCREEN_WIDTH - 16, 84), 1)

        # Subito sotto: Elenco effetti applicati con valori numerici o percentuali
        # Mostra una finestra di 3 grandi card (altezza 50px ciascuna)
        total_fx = len(snap.effects)
        visible_count = 3
        start_idx = max(0, min(snap.effect_list_idx - 1, total_fx - visible_count))
        start_idx = max(0, start_idx)

        y_offset = 92
        for i in range(start_idx, min(start_idx + visible_count, total_fx)):
            fx = snap.effects[i]
            is_focused = (i == snap.effect_list_idx)
            is_active = fx.get("active", True)
            val = fx.get("val", 0)

            card_rect = pygame.Rect(16, y_offset, SCREEN_WIDTH - 32, 48)
            card_bg = th.card_sel if is_focused else th.card
            card_border = th.focus_amber if is_focused else th.border

            pygame.draw.rect(self.screen, card_bg, card_rect, border_radius=4)
            pygame.draw.rect(self.screen, card_border, card_rect, width=1, border_radius=4)

            # Pallino indicatore bypass (Verde = Attivo, Grigio = Bypass)
            status_dot_col = th.green_ok if is_active else th.gray_muted
            pygame.draw.circle(self.screen, status_dot_col, (32, y_offset + 24), 5)

            # Indice e Nome Effetto
            idx_str = f"{i+1:02d}."
            name_str = f"{fx['name']} [{fx.get('cat', 'DSP')}]"
            name_col = th.text if is_active else th.gray_muted
            
            idx_surf = self.font_small.render(idx_str, True, th.text_dim)
            name_surf = self.font_main.render(name_str, True, name_col)
            self.screen.blit(idx_surf, (44, y_offset + 10))
            self.screen.blit(name_surf, (72, y_offset + 9))

            # Valore numerico percentuale
            val_str = f"{val}%"
            val_surf = self.font_main.render(val_str, True, th.focus_amber if is_focused else th.text)
            self.screen.blit(val_surf, (SCREEN_WIDTH - 76, y_offset + 9))

            # Barra orizzontale di livello
            bar_w = SCREEN_WIDTH - 32 - 88
            bar_rect_bg = pygame.Rect(72, y_offset + 30, bar_w, 6)
            bar_fill_w = int(bar_w * (val / 100.0))
            bar_rect_fill = pygame.Rect(72, y_offset + 30, bar_fill_w, 6)

            pygame.draw.rect(self.screen, th.dark_gray, bar_rect_bg, border_radius=2)
            if is_active:
                fill_color = th.focus_amber if is_focused else th.text
                pygame.draw.rect(self.screen, fill_color, bar_rect_fill, border_radius=2)

            y_offset += 54

    def _draw_center_live_home(self, snap: StateSnapshot):
        """Home LIVE: Cruscotto ad alto contrasto per la modifica rapida sul palco."""
        th = self.theme

        # Banner Scena Attiva
        scene_title = self.font_large.render(snap.current_preset_name, True, th.text)
        self.screen.blit(scene_title, (16, 48))

        sub_hint = self.font_small.render("CRUSCOTTO PERFORMANCE  |  [Pedale / Tasti 1-5 = Cambio Scena]", True, th.text_dim)
        self.screen.blit(sub_hint, (16, 72))

        # Rappresentazione Signal Chain a blocchi interconnessi
        chain_y = 100
        blocks = [
            ("PORTA 1", snap.instruments_by_slot[0][:10], True),
            ("PORTA 2", snap.instruments_by_slot[1][:10], True),
            ("EFFETTI", f"{len([x for x in snap.effects if x['active']])} ATTIVI", True),
            ("MASTER", "0.0 dB", True),
        ]

        block_w = 98
        block_h = 58
        for idx, (b_label, b_val, b_state) in enumerate(blocks):
            bx = 16 + idx * (block_w + 14)
            brect = pygame.Rect(bx, chain_y, block_w, block_h)
            
            is_sel = (idx == snap.selected_slot)
            bg_col = th.card_sel if is_sel else th.card
            bd_col = th.focus_amber if is_sel else th.border

            pygame.draw.rect(self.screen, bg_col, brect, border_radius=4)
            pygame.draw.rect(self.screen, bd_col, brect, 1, border_radius=4)

            l_surf = self.font_small.render(b_label, True, th.text_dim)
            v_surf = self.font_main.render(b_val, True, th.text)
            self.screen.blit(l_surf, (bx + 8, chain_y + 8))
            self.screen.blit(v_surf, (bx + 8, chain_y + 28))

            if idx < len(blocks) - 1:
                # Freccia di connessione segnale
                pygame.draw.line(self.screen, th.border, (bx + block_w + 2, chain_y + 29), (bx + block_w + 12, chain_y + 29), 2)

        # Controlli rapidi Macro in basso nell'area centrale
        macro_y = 175
        macro_rect = pygame.Rect(16, macro_y, SCREEN_WIDTH - 32, 70)
        pygame.draw.rect(self.screen, th.card, macro_rect, border_radius=4)
        pygame.draw.rect(self.screen, th.border, macro_rect, 1, border_radius=4)

        m_title = self.font_small.render("CONTROLLO RAPIDO LIVE (Enc 2 = Effetti  |  Enc 3 = Strumenti)", True, th.text_dim)
        self.screen.blit(m_title, (28, macro_y + 10))

        fx_act = snap.effects[snap.effect_list_idx]
        fx_quick = f"EFFETTO: {fx_act['name']} [{'ON' if fx_act['active'] else 'BYPASS'}]"
        fx_surf = self.font_main.render(fx_quick, True, th.green_ok if fx_act['active'] else th.gray_muted)
        self.screen.blit(fx_surf, (28, macro_y + 36))

        inst_quick = f"PARAM: {snap.current_instrument_name[:18]}"
        inst_surf = self.font_main.render(inst_quick, True, th.text)
        self.screen.blit(inst_surf, (260, macro_y + 36))

    def _draw_center_selection_list(self, title: str, items: List[str], selected_idx: int, hint: str):
        """Menu a scorrimento per selezione strumenti o effetti (senza toccare le barre)."""
        th = self.theme
        t_surf = self.font_title.render(title, True, th.text)
        self.screen.blit(t_surf, (16, 48))

        h_surf = self.font_small.render(hint, True, th.text_dim)
        self.screen.blit(h_surf, (16, 70))

        # Finestra di visualizzazione a 4 righe
        total = len(items)
        visible = 4
        start = max(0, min(selected_idx - 1, total - visible))
        start = max(0, start)

        y = 92
        for i in range(start, min(start + visible, total)):
            is_sel = (i == selected_idx)
            item_rect = pygame.Rect(16, y, SCREEN_WIDTH - 32, 34)

            bg = th.card_sel if is_sel else th.card
            bd = th.focus_amber if is_sel else th.border
            pygame.draw.rect(self.screen, bg, item_rect, border_radius=4)
            pygame.draw.rect(self.screen, bd, item_rect, 1, border_radius=4)

            prefix = "> " if is_sel else "  "
            text_surf = self.font_main.render(f"{prefix}{items[i]}", True, th.focus_amber if is_sel else th.text)
            self.screen.blit(text_surf, (28, y + 8))

            y += 40

    def _draw_center_settings(self, snap: StateSnapshot):
        """Sottomenu impostazioni avanzate parametri con supporto a Press & Turn."""
        th = self.theme
        is_fx = (snap.view_mode == ViewMode.EFFECT_SETTINGS)
        title = f"PARAMETRI: {snap.effects[snap.effect_list_idx]['name']}" if is_fx else f"PARAMETRI: {snap.current_instrument_name}"
        params = snap.effects[snap.effect_list_idx]["params"] if is_fx else snap.instrument_params
        sel_idx = snap.effect_param_idx if is_fx else snap.instrument_param_idx

        t_surf = self.font_title.render(title, True, th.text)
        self.screen.blit(t_surf, (16, 48))

        hint_surf = self.font_small.render("Gira: Scorre parametri  |  Premi + Gira: Modifica Valore  |  Click: Salva", True, th.text_dim)
        self.screen.blit(hint_surf, (16, 70))

        y = 96
        for i, p in enumerate(params):
            is_sel = (i == sel_idx)
            card_rect = pygame.Rect(16, y, SCREEN_WIDTH - 32, 44)
            bg = th.card_sel if is_sel else th.card
            bd = th.focus_amber if is_sel else th.border

            pygame.draw.rect(self.screen, bg, card_rect, border_radius=4)
            pygame.draw.rect(self.screen, bd, card_rect, 1, border_radius=4)

            name_surf = self.font_main.render(p["name"], True, th.focus_amber if is_sel else th.text)
            self.screen.blit(name_surf, (28, y + 12))

            val_str = f"{p['val']} {p.get('unit', '')}"
            val_surf = self.font_main.render(val_str, True, th.text)
            self.screen.blit(val_surf, (SCREEN_WIDTH - 110, y + 12))

            # Piccola barra orizzontale
            bar_w = 120
            bar_bg = pygame.Rect(SCREEN_WIDTH - 240, y + 19, bar_w, 6)
            bar_fill = pygame.Rect(SCREEN_WIDTH - 240, y + 19, int(bar_w * (p["val"] / 100.0)), 6)
            pygame.draw.rect(self.screen, th.dark_gray, bar_bg, border_radius=2)
            pygame.draw.rect(self.screen, th.focus_amber if is_sel else th.text, bar_fill, border_radius=2)

            y += 50

    # =====================================================================
    # 3. BOTTOM BAR (60 px, Y: 260 - 320)
    # =====================================================================

    def _draw_bottom_bar(self, snap: StateSnapshot):
        """Disegna i 5 slot corrispondenti ai pulsanti/porte hardware e i pallini 10x10 px."""
        th = self.theme
        pygame.draw.rect(self.screen, th.bg, RECT_BOTTOM_BAR)
        pygame.draw.line(self.screen, th.border, (0, 260), (SCREEN_WIDTH, 260), 1)

        slot_w = SCREEN_WIDTH // 5  # 96 px per slot
        for i in range(5):
            port_key = f"porta_{i+1}"
            mod_info = snap.module_status.get(port_key, {"name": "", "status": ModuleState.EMPTY.value})
            
            x_start = i * slot_w
            is_selected = (i == snap.selected_slot)

            # Riquadro slot con highlight se selezionato
            slot_rect = pygame.Rect(x_start + 4, 266, slot_w - 8, 48)
            bg_col = th.card_sel if is_selected else th.card
            bd_col = th.focus_amber if is_selected else th.border

            pygame.draw.rect(self.screen, bg_col, slot_rect, border_radius=4)
            pygame.draw.rect(self.screen, bd_col, slot_rect, 1, border_radius=4)

            # Numero Slot [1]..[5]
            slot_num = self.font_small.render(f"P{i+1}", True, th.text_dim)
            self.screen.blit(slot_num, (x_start + 10, 272))

            # Nome breve modulo
            raw_name = mod_info.get("name", "")
            short_name = raw_name[:8] if raw_name else ("VUOTO" if mod_info["status"] == ModuleState.EMPTY.value else "MOD")
            name_surf = self.font_small.render(short_name, True, th.text)
            self.screen.blit(name_surf, (x_start + 10, 292))

            # Indicatore geometrico di stato (Cerchio 10x10 px)
            # Verde = CONNECTED, Grigio = EMPTY, Rosso = ERROR
            status = mod_info.get("status", ModuleState.EMPTY.value)
            if status == ModuleState.CONNECTED.value:
                dot_color = th.green_ok
            elif status == ModuleState.ERROR.value:
                dot_color = th.red_alert
            else:
                dot_color = th.gray_muted

            dot_cx = x_start + slot_w - 18
            dot_cy = 278
            pygame.draw.circle(self.screen, dot_color, (dot_cx, dot_cy), 5)


# =====================================================================
# LOOP PRINCIPALE INTEGRATO (STANDALONE / RUNNER)
# =====================================================================

def run_gui_app(force_window: bool = False):
    """Esegue la shell grafica di produzione collegando HardwareMonitor, StateMachine e ViewSPI."""
    hw = HardwareMonitor()
    sm = MiMOStateMachine()
    view = MiMOViewSPI(force_window=force_window)

    hw.start()

    clock = pygame.time.Clock()
    running = True

    # Render iniziale a schermo pieno (DirtyZone.ALL)
    current_snapshot = sm.get_snapshot({DirtyZone.ALL})
    view.render(current_snapshot)

    logger.info("MiMO Shell avviata. Premi Q o ESC per uscire.")

    last_status_sync = time.time()

    try:
        while running:
            # 1. Eventi Pygame (Finestra / Tastiera per simulazione interattiva)
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key in (pygame.K_ESCAPE, pygame.K_q):
                        running = False
                    # Simulazione tasti encoder:
                    # Spazio = Click Encoder 1 (Workspace unlock)
                    elif event.key == pygame.K_SPACE:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.WORKSPACE.value))
                        view.render(current_snapshot)
                    # W = Ruota Workspace
                    elif event.key == pygame.K_w:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.ROTATE_CW, EncoderId.WORKSPACE.value))
                        view.render(current_snapshot)
                    # Frecce SX/DX = Ruota Effetti (Enc 2)
                    elif event.key == pygame.K_RIGHT:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.ROTATE_CW, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    elif event.key == pygame.K_LEFT:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.ROTATE_CCW, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    # Frecce SU/GIU = Press & Turn Effetti (+5% / -5%)
                    elif event.key == pygame.K_UP:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.PRESS_ROTATE_CW, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    elif event.key == pygame.K_DOWN:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.PRESS_ROTATE_CCW, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    # Invio = Click Effetti (1 Click)
                    elif event.key == pygame.K_RETURN:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    # B = Doppio Click Effetti (<300ms)
                    elif event.key == pygame.K_b:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.DOUBLE_CLICK, EncoderId.EFFECTS.value))
                        view.render(current_snapshot)
                    # I = Click Strumenti (Enc 3)
                    elif event.key == pygame.K_i:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.SHORT_CLICK, EncoderId.INSTRUMENTS.value))
                        view.render(current_snapshot)
                    # O = Doppio Click Strumenti (Enc 3)
                    elif event.key == pygame.K_o:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.DOUBLE_CLICK, EncoderId.INSTRUMENTS.value))
                        view.render(current_snapshot)
                    # 1..5 = 5 Pulsanti Fisici Modulo/Scena
                    elif event.key in (pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4, pygame.K_5):
                        slot_idx = event.key - pygame.K_1
                        current_snapshot = sm.process_event(HardwareEvent(EventType.BUTTON_CLICK, slot_idx))
                        view.render(current_snapshot)
                    # P = Pedale Sustain (Scene Switcher)
                    elif event.key == pygame.K_p:
                        current_snapshot = sm.process_event(HardwareEvent(EventType.PEDAL_TRIGGER, 0))
                        view.render(current_snapshot)
                    # H = Toggle PC Sync (MiMO Hub)
                    elif event.key == pygame.K_h:
                        current_snapshot = sm.set_hub_connected(not sm.hub_connected)
                        view.render(current_snapshot)

            # 2. Polling Eventi Hardware Reali (da libgpiod / thread interrupt)
            hw_events = hw.poll_events()
            for hw_ev in hw_events:
                current_snapshot = sm.process_event(hw_ev)
                view.render(current_snapshot)

            # 3. Sincronizzazione periodica dizionario diagnostico I2C (Anti-Freeze da RAM)
            now = time.time()
            if now - last_status_sync >= 1.0:
                mod_status = hw.get_module_status()
                current_snapshot = sm.update_module_status(mod_status)
                if DirtyZone.BOTTOM_BAR in current_snapshot.dirty_zones:
                    view.render(current_snapshot)
                last_status_sync = now

            # Mantiene loop a 60 FPS stabili
            clock.tick(60)

    except KeyboardInterrupt:
        pass
    finally:
        hw.stop()
        pygame.quit()
        logger.info("MiMO Shell terminata.")


if __name__ == "__main__":
    # Avvia in modalità finestra per default se invocato direttamente
    force_win = (sys.platform != "linux") or ("--window" in sys.argv)
    run_gui_app(force_window=force_win)
