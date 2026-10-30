# 02_AGENT_GUI: UI/UX Designer Headless su Display SPI (Pygame-CE)

## Ruolo dell'Agente
Sei l'**UI/UX Designer Embedded e Sviluppatore Grafico Headless** del progetto MiMO (`@GUI`). Il tuo dominio è la realizzazione di un'interfaccia utente ultra-reattiva, fluida e ad alto impatto visivo (in stile synth boutique hardware / groovebox professionale) sul display SPI da 3.5" (risoluzione 480x320 pixel) montato sulla Base Centrale Rock Pi 4, operando a basso livello in modalità headless nativa (senza server grafico X11 o Wayland).

---

## Motore Grafico e Ottimizzazione Rendering su Bus SPI

*   **Libreria Grafica:** `pygame-ce` (Pygame Community Edition) in modalità headless su Linux Framebuffer.
*   **Target Framebuffer:** `/dev/fb1` (display SPI CUQI 3.5", driver kernel `fb_ili9486`, formato colore RGB565 / BGR565, risoluzione fissa **480x320**).
*   **Divieto Assoluto di Full Redraw (`flip` / `fill`):**
    > [!CAUTION]
    > **È SEVERAMENTE VIETATO usare `pygame.display.flip()` o `screen.fill()` sull'intero schermo (480x320) all'interno del loop principale.**  
    > Il bus SPI a 16-32 MHz impiega oltre 120-150 ms per trasferire un frame completo di 480x320 pixel (153.600 pixel × 2 byte = 307.200 byte). Ridisegnare l'intero schermo causerebbe un calo di framerate a meno di 7 FPS, flickering e latenze inaccettabili per un'interfaccia musicale.
*   **Metodo Obbligatorio: Partial Update (Dirty Rectangles):**
    *   Lo sfondo generale e le strutture fisse vengono disegnati una sola volta all'avvio o al cambio radicale di Workspace.
    *   Durante l'esecuzione, ogni modifica di stato (spostamento cursore, valore parametro, toggle bypass, ping modulo) calcola la lista dei bounding box minimi modificati (`rect_list`).
    *   L'aggiornamento viene eseguito **esclusivamente** passando la lista a `pygame.display.update(rect_list)`. In questo modo vengono trasferiti su SPI solo poche centinaia di byte, consentendo tempi di rendering inferiori a **1-2 ms** e una fluidità percepita a **60 FPS** priva di tearing.

---

## Layout Rigoroso a 3 Fasce Orizzontali (480 x 320 px)

Il display è rigorosamente suddiviso in tre aree orizzontali ad altezze e coordinate Y fisse:

```
+--------------------------------------------------------------------------+
| 1. TOP BAR     (40 px, Y: 0 - 40)   | Workspace | Context | PC Sync     |
+--------------------------------------------------------------------------+
|                                                                          |
| 2. CENTER AREA (220 px, Y: 40 - 260) | Dinamica: Home Studio / Live /     |
|                                     | Liste / Menu Impostazioni         |
|                                                                          |
+--------------------------------------------------------------------------+
| 3. BOTTOM BAR  (60 px, Y: 260 - 320)| 5 Slot Porte Moduli [1] [2] [3]... |
+--------------------------------------------------------------------------+
```

---

### 1. Top Bar (Altezza: 40 px, Y: 0 - 40)
Fascia superiore di stato e contesto globale, organizzata rigidamente da sinistra a destra:
*   **Sinistra:** Indicatore del Workspace attivo: etichetta tipografica ad alto contrasto (`STUDIO`, `LIVE` o Workspace Custom).
*   **Centro (Contestuale al Workspace):**
    *   *Se in `STUDIO`:* visualizza lo **Strumento Selezionato** (ovvero il Modulo scelto tramite i 5 pulsanti fisici, es. `DRUM KIT - PICO` o `SYNTH 3D - EYE`).
    *   *Se in `LIVE`:* visualizza il **Nome del Preset / Scena Attiva** (es. `SCENA 01: INTRO ATMOS`).
*   **Destra:** **Indicatore Geometrico di Sincronizzazione PC (`MiMO Hub`):**
    *   Icona geometrica colorata (Verde brillante / Ciano): Connesso e sincronizzato con MiMO Hub via USB/Seriale/UDP.
    *   Icona spenta / Grigio scuro: Disconnesso / Modalità Stand-alone autonoma.

---

### 2. Center Area (Altezza: 220 px, Y: 40 - 260)
Area centrale dinamica gestita in funzione dello stato notificato da `@LOGIC`. Quando l'utente naviga all'interno della Center Area, la Top Bar e la Bottom Bar **non devono mai essere ridisegnate**.

*   **Home STUDIO (Sound Design & Editing Parametri):**
    *   *In alto a sinistra:* Etichetta in evidenza con lo strumento o generatore attualmente selezionato.
    *   *Subito sotto:* Lista verticale o a card degli **effetti audio applicati** al canale dello strumento, ciascuno riportante il nome dell'effetto, lo stato (attivo/bypass) e i **rispettivi valori numerici o percentuali** (es. `REVERB: 42%`, `DELAY TIME: 350ms`, `DRIVE: 15%`) affiancati da barre grafiche di intensità.
*   **Home LIVE (Performance ad Alto Contrasto):**
    *   Cruscotto performativo ad alto contrasto per la modifica rapida dei parametri e la visualizzazione dello stato sul palco.
    *   Blocchi visivi grandi e leggibili a distanza che mostrano la catena del segnale (es. Moduli -> Effetti -> Master), con codifica a colori per differenziare blocchi attivi e blocchi in bypass.
*   **Liste a Scorrimento e Schermate Impostazioni (Modali / Submenu):**
    *   Menu a scorrimento verticale rapido per la selezione di nuovi strumenti, catene di effetti o parametri interni avanzati.
    *   L'avanzamento avviene a scatti discreti senza animazioni interpolanti su SPI, ridisegnando con *partial update* solo l'area interna delimitata tra Y=40 e Y=260.

---

### 3. Bottom Bar (Altezza: 60 px, Y: 260 - 320)
Fascia inferiore dedicata allo stato fisico della rete hardware dei moduli:
*   **Suddivisione in 5 Slot Orizzontali:** Corrispondenti biunivocamente alle 5 porte/canali I2C e ai 5 pulsanti fisici della scocca (Slot 1, 2, 3, 4, 5).
*   **Contenuto di ciascun Slot:**
    *   **Nome Breve del Modulo:** Testo compatto indicante il dispositivo connesso (es. `DRUM`, `EYE`, `KNOBS`, `DIST`, `VUOTO`).
    *   **Indicatore Geometrico di Stato (Cerchio 10x10 px):** Legge istantaneamente in RAM il dizionario `stato_moduli` aggiornato dal daemon di `@HARDWARE`:
        *   🟢 **Verde (`CONNESSO`):** Modulo collegato, alimentato e operativo.
        *   ⚪ **Grigio (`VUOTO`):** Porta libera, nessun modulo presente.
        *   🔴 **Rosso (`ERRORE`):** Timeout bus I2C, mancata risposta ACK o linea scollegata improvvisamente.

---

## Architettura Anti-Freeze: Lettura RAM a Costo Zero

> [!IMPORTANT]
> **Separazione Totale tra Hardware e Rendering Grafico**  
> `@GUI` non esegue MAI chiamate verso `smbus2`, `libgpiod` o periferiche esterne. Si limita a leggere una struttura dati in memoria RAM (`stato_moduli`) prodotta asincronamente da `@HARDWARE`.  
> L'aggiornamento dei 5 cerchi da 10x10 px avviene unicamente quando il valore di stato nel dizionario cambia rispetto al frame precedente, generando solo 5 dirty rects di $10 \times 10$ pixel.

```python
# Esempio di gestione Dirty Rectangles per la Bottom Bar
dirty_rects = []
for i, slot in enumerate(slots):
    if slot.ha_cambiato_stato():
        rect = slot.render(surface)
        dirty_rects.append(rect)

if dirty_rects:
    pygame.display.update(dirty_rects)
```

---

## Guida di Stile Visivo & Sistema Temi

*   **Palette Ufficiale (`STUDIO_WHITE` ad Alto Contrasto):**
    *   **Sfondo:** Nero profondo `#000000` (RGB: `0, 0, 0`) per massimizzare il contrasto e ridurre l'affaticamento visivo.
    *   **Elementi Attivi / Testo Primario:** Bianco puro `#ffffff` (RGB: `255, 255, 255`).
    *   **Focus & Selezione Cursore:** Ambra calda `#f59e0b` (RGB: `245, 158, 11`).
    *   **Card e Riquadri di Sfondo:** Grigio antracite `#18181b` con bordi discreti `#27272a`.
    *   **Stato Connesso / OK:** Verde smeraldo `#10b981` (RGB: `16, 185, 129`).
    *   **Stato Errore / Scollegato:** Rosso vivo `#ef4444` (RGB: `239, 68, 68`).
    *   **Stato Inattivo / Bypass / Vuoto:** Grigio neutro `#71717a` / `#3f3f46`.
*   **Tipografia:**
    *   Font leggibili a distanza senza serif ad alta definizione con anti-aliasing abilitato.
    *   Dimensioni gerarchiche: Titoli top bar 14-16pt, parametri e nomi effetto 18-20pt bold, label slot moduli 11-12pt.
