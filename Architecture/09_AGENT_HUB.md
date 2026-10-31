# 09_AGENT_HUB: Architetto di MiMO Hub Desktop Nativo (C++20 / JUCE) & Mobile PWA

## Ruolo dell'Agente
Sei l'**Architetto Audio Software e Lead UX Designer di "MiMO Hub"** (`@HUB`), sotto il coordinamento di `@00_MIMO_MASTER`. La tua missione è orchestrare l'ecosistema digitale di MiMO creando un'applicazione desktop nativa in **C++20 con framework JUCE**, con driver a bassissima latenza (ASIO/CoreAudio/PipeWire), porte virtuali MIDI e gestione OSC, incarnando un'identità visiva unica, moderna ed elegante: l'estetica **"Modern Flat & Pastel Minimal"**.

---

## Visione Estetica Globale: "Modern Flat & Pastel Minimal"

> [!IMPORTANT]
> **Rottura con le DAW Tradizionali e con lo Scheumorfismo Retrò:**  
> MiMO Hub **non assomiglia a un software musicale convenzionale** (nessun rack virtuale grigio asettico, nessuna griglia rigida da foglio di calcolo, nessun effetto metallico finto). L'interfaccia adotta un **design flat contemporaneo, pulito e minimale**, caratterizzato da riempimenti a tinta piatta, linee vettoriali nitide e bordi arrotondati precisi, valorizzato da una **palette pastello morbida e armoniosa**.

### Elementi Visivi Fondamentali
1. **Il Canvas Moderno (Superficie di Lavoro):** Sfondo a tinta piatta neutra e riposante (tonalità crema/calce in modalità chiara o grafite profondo e pulito in modalità scura), privo di texture rumorose o granulosità fittizie.
2. **Linee Moderne e Vettoriali:** Contorni dei componenti, cavi di segnale e frecce direzionali sono tracciati con curve di Bézier fluide e precise, spessori geometrici costanti e antialiasing impeccabile.
3. **Riempimenti Piatti a Colori Pastello (Flat Pastel):** Schede dei moduli, badge di stato, indicatori di livello e cavi attivi utilizzano campiture piatte a tinta unita (o con trasparenze soft moderne), declinate nella palette pastello ufficiale:
   - **Ciano Pastello (`crayonCyan`):** Modulo Occhio, telemetria 3D e connessioni ad alta velocità.
   - **Salvia Pastello (`crayonSage`):** Moduli percussivi, drum kit e trigger attivi.
   - **Pesca Pastello (`crayonPeach`):** Nodi sintetizzatore e generatori armonici.
   - **Lilla Pastello (`crayonLilac`):** Modulatori, LFO e automazioni.
   - **Giallo Ocra (`crayonOchre`):** Avvisi, connettori e macro interattive.
   - **Rosso Ciliegia (`crayonCherry`):** Registrazione, mute e trigger prioritari.
4. **Dock di Navigazione Inferiore:** Dock fluttuante con angoli smussati, posizionato **in basso al centro** dello schermo con 3 pulsanti essenziali e icone geometriche piatte:
   - **Studio:** Canvas infinito per sound design, nodi e composizione modulare.
   - **Live:** Galleria Preset Card e i 5 slot Scena ad accesso istantaneo per la performance.
   - **Community:** Marketplace per il download di moduli 3D, preset e pacchetti `.mimo`.

---

## Pilastro 1: Workspace STUDIO (Canvas Infinito & Nodi Nested)

Il Workspace **STUDIO** è il centro operativo di MiMO Hub: un ambiente modulare flessibile e intuitivo.

```
+-----------------------------------------------------------------------------------------+
|                                     CANVAS INFINITO                                     |
|                               (Pan & Zoom Liberi - No Grid)                             |
|                                                                                         |
|               [ Modulo Batteria ]                                  +------------------+ |
|                        \                                           | [FOTOCAMERA] (o) | |
|               (Cavo)    \                                          +------------------+ |
|                          v                                         | STRUMENTI        | |
|                 +------------------+    (Cavo Vettoriale)          | - Percussioni    | |
|                 |   BASE CENTRALE  | ==========================>   | - Banchi SFZ     | |
|                 |       MiMO       |       [ FILTRO DSP ]          | - Synth Faust    | |
|                 +------------------+       (Effetto Inserito)      |------------------| |
|                          ^                                         | EFFETTI          | |
|               (Cavo)    /                                          | - Riverberi      | |
|                        /                                           | - Delay Spaziali | |
|                 [ Modulo Occhio ]                                  | - Saturazioni    | |
|                                                                    +------------------+ |
|                                                                    | SIDEBAR SCOMPARSA| |
|                                                                                         |
|                         [ (Studio)          (Live)         (Community) ]                |
|                                      (DOCK INFERIORE)                                   |
+-----------------------------------------------------------------------------------------+
```

### 1.1 Canvas Infinito Senza Griglia Vincolante
- **Navigazione Fluida:** Spazio di lavoro libero con pan fluido a 360° (anche diagonale per touchpad) e zoom continuo incentrato sulla posizione del cursore.
- **Console Centrale MiMO:** Nodo centrale che rappresenta la base hardware (Rock Pi 4), disegnato come una card moderna ad angoli arrotondati con indicatori LED piatti che riflettono lo stato in tempo reale.
- **Sidebar a Scomparsa (Destra):** Pannello laterale retraibile con schede piatte suddivise per categorie:
  - **Strumenti:** Banchi campionati (`sfizz`), modelli fisici, sintetizzatori DSP (`Faust`).
  - **Effetti:** Filtri, delay, riverberi, modulatori, saturazioni.
  - **Utility:** LFO, Envelope Follower, Scale Lock, Tape Looper.

### 1.2 Interazione Drag & Drop Intuitiva
- **Assegnazione Strumenti:** Trascina uno strumento dalla sidebar e rilascialo su uno dei moduli periferici nel canvas. Lo strumento si posiziona automaticamente alla destra del modulo facendogli spazio ordinato sul canvas, rimanendo visibile ed editabile.
- **Inserimento Effetti "Drop-on-Wire":** Trascina un effetto e rilascialo direttamente sul cavo di collegamento tra due blocchi: il cavo vettoriale si illumina e si divide automaticamente accogliendo la card del nuovo effetto inserito in serie.

### 1.3 Architettura a Nodi Nested (Stile Modern Graph Editor)
- **Macro Node (Vista Esterna Compatta):** All'esterno il nodo complesso appare come una card compatta con bordi arrotondati ed esponendo unicamente le manopole macro essenziali.
- **Drill-Down a Schermo Intero (Doppio Clic / TAB):** Il doppio clic o il tasto TAB sul blocco apre il canvas interno di personalizzazione per modificare la catena di blocchi base e salvare preset custom nella libreria globale.

---

## Pilastro 2: Sistema di Salvataggio "Preset Cards" & Workspace LIVE

La gestione dei preset sostituisce i vecchi menu testuali con un sistema visivo immediato a **Preset Cards**.

```
       +-------------------+
       |   /\    ____      |
       |  /__\  |    | (O) |  <- Icona Fotocamera / Snapshot (Sopra la Sidebar)
       +-------------------+

       +-------------------+
       | +---------------+ |
       | |               | |
       | | MINIATURA     | |
       | | CANVAS FLAT   | |  <- Snapshot Istantaneo della Scena Grafica
       | |               | |
       | +---------------+ |
       |                   |
       |  Batte-Funk 70    |  <- Etichetta Flat pulita e rinominabile
       +-------------------+
```

### 2.1 Lo Snapshot Istantaneo
Sopra la sidebar destra è presente un'icona a forma di fotocamera/snapshot moderna. Un clic su di essa cattura istantaneamente lo stato del canvas attuale generando una **Preset Card** in primo piano.

### 2.2 Interazioni Rapide con la Preset Card
1. **Auto-Salvataggio nell'Album (Timeout o Click Esterno):** Archiviazione automatica nella galleria preset con nome descrittivo automatico.
2. **Ridenominazione Immediata:** Digitando direttamente da tastiera, il titolo della card si aggiorna.
3. **Assegnazione Rapida ai 5 Slot (Drag & Drop o Tasto Hardware):** Trascinando la card su uno dei 5 slot Scena in basso, la configurazione attuale viene assegnata allo slot selezionato.

---

## Pilastro 3: Architettura del "Doppio Binario" (C++20 / JUCE Audio Core)

```
                           +------------------------+
                           |  BASE CENTRALE ROCK PI |
                           +------------------------+
                               |              |
                      (OSC / WebSocket)   (MIDI USB)
                               |              |
                               v              v
             +------------------------------------------------------+
             |                   MiMO HUB (C++20)                   |
             |                                                      |
             |    [INTERNAL ENGINE]             [PRO DAW BRIDGE]    |
             |    - sfizz (SFZ)                 - Virtual MIDI Port |
             |    - Faust DSP                   - MPE CC (16-18)    |
             |    - AudioDeviceManager          - Zero CPU DSP      |
             +------------------------------------------------------+
```

### 3.1 Modalità Internal Engine (Stand-Alone / Neofiti)
- **Latenza Ultra-Bassa:** `juce::AudioDeviceManager` interfacciato con ASIO (Windows), CoreAudio (macOS) e PipeWire/ALSA (Linux) con buffer a 64 campioni ($< 3\text{ ms}$).
- **Campionatore `sfizz` (C++):** Gestione kit percussivi multi-layer e campioni `.sfz`.
- **Sintesi ed Effetti `Faust` DSP (C++):** Processori C++ modulari isolati in `Source/Audio/Modules/`.

### 3.2 Modalità Pro DAW Bridge (Integrazione Studi Professionali)
- **Bypass DSP:** Risparmio totale di CPU disattivando la sintesi interna quando si usa una DAW esterna (Ableton Live, Bitwig, Logic, Reaper).
- **Virtual MIDI & OSC Routing:** `juce::MidiOutput` crea un bus Virtual MIDI standard e telemetria continua con la console Rock Pi.

---

## Pilastro 4: Modulo "Occhio" (MiMO Eye) & Hand Tracking 3D

Il Modulo Occhio trasforma i movimenti della mano e delle dita in modulazioni continue e trigger sonori istantanei, basandosi sull'elaborazione AI open-source.

```
       +-----------------------------------------------------------------+
       |                       MODULO OCCHIO (CANVAS)                    |
       |  [ (O) MiMO Eye ]   [ Badge: Phone Live 60 FPS ]    [ (Config) ]|
       |                                                                 |
       |        +-- [POPOVER DI COLLEGAMENTO / DRAWER] ----------+       |
       |        | [Smartphone (Consigliato)] | [Webcam PC]       |       |
       |        |                                                |       |
       |        |   +-------------+    Hand Tracking Live:       |       |
       |        |   | QR CODE     |    21 Landmark Vettoriali    |       |
       |        |   | VETTORIALE  |    Pinch Indice: ON (vel 95) |       |
       |        |   +-------------+    X: 0.62 | Y: 0.81 | Z:0.4 |       |
       |        |   +-------------+                              |       |
       |        +------------------------------------------------+       |
       +-----------------------------------------------------------------+
```

- **QR Code Vettoriale Integrato:** Punta a `http://<IP_LOCALE>:8088/eye` per aprire la PWA mobile senza installazioni da app store.
- **MediaPipe Hands a 60 FPS:** 21 landmark 3D (falangi, nocche, polso e dita) con pinch indipendenti per dito e streaming WebSocket/OSC a bassa latenza.

---

## Pilastro 5: Struttura Modulare Rigorosa della Codebase C++20 / JUCE

> [!CAUTION]
> **REGOLA ARCHITETTURALE ANTI-MONOLITE (Obbligo di Decomposizione Modulare):**  
> `MainComponent.cpp` non deve superare **800-1.000 righe di codice**.  
> È vietato aggiungere direttamente in `MainComponent.cpp` logiche di calcolo coordinate complesse, algoritmi grafici di disegno nodi o routine di tracking hardware. Tutte le funzionalità devono risiedere nei rispettivi moduli dedicati.

### 5.1 Albero delle Directory di MiMO Hub (`mimo_hub_juce/Source/`)

```
Source/
├── Audio/                          [PILASTRO 1: IL SUONO]
│   ├── AudioEngine.h / .cpp        Matrice di routing audio realtime (zero allocazioni)
│   ├── NodeAudioProcessor.h        Interfaccia base pura polimorfa per tutti i processori DSP
│   ├── DSPUtils.h                  Anti-denormal, ADSR analogica, saturazione soft-clip
│   ├── AudioDSP.h                  Umbrella header che include tutti i moduli
│   └── Modules/                    Moduli DSP isolati (1 header per classe, compilazione autonoma):
│       ├── Instruments/            AnalogDrumVoice.h, MelodicSynthVoice.h, Theremin3DVoice.h, KarplusPluckVoice.h
│       ├── Effects/                SVFFilter.h, SoftSaturator.h, StereoTapeDelay.h, LushReverb.h, BBDChorus.h, MoogLadderFilter.h
│       └── Utilities/              MultiWaveLFO.h, EnvelopeFollower.h, ScaleLock.h, TapeLooperProcessor.h
│
├── Devices/                        [PILASTRO 2: L'HARDWARE DEL LAPTOP & SENSORI]
│   ├── HardwareWorkers.h           Thread ad alta priorità dedicati (0 ms blocco sul thread UI)
│   └── HardwareWorkers.cpp         MicWorker (WASAPI input/synth) + CameraWorker (Webcam & MediaPipe)
│
├── GUI/
│   ├── CanvasGraph.h / .cpp        [PILASTRO 3: DATI & TOPOLOGIA DEL GRAFO]
│   │                               Strutture pure (CanvasNode, CanvasCable, KnobParam, CatalogItem, PinHit)
│   │                               Matematica di proiezione (screenToWorld, snap pin, hit-testing cavi)
│   │
│   ├── MiMOEyeDrawer.h / .cpp      [PILASTRO 4: VISIONE IA & COMPUTER VISION]
│   │                               Rendering scheletro 21 falangi MediaPipe, drawer laterale calibrazione,
│   │                               generazione codice QR vettoriale per smartphone PWA.
│   │
│   ├── MiMOLookAndFeel.h / .cpp    Stile visivo ufficiale "Modern Flat Pastel" (font, bottoni, knob, cavi)
│   │
│   ├── CanvasRenderer.h / .cpp     Rendering specializzato del canvas: disegno nodi, schede opache, manopole,
│   │                               cavi pastello dinamici e marquee selection.
│   │
│   ├── SidebarComponent.h / .cpp   Pannello laterale a scomparsa: categorie catalogo, hover, drag-and-drop.
│   │
│   ├── MainComponent.h             [IL COORDINATORE GRAFICO LEAN]
│   └── MainComponent.cpp           Unicamente assemblatore e router di eventi ad alto livello (< 800 righe).
│                                   Inizializza audio, workers e sub-componenti grafici.
│
├── Networking/
│   ├── MiMOEyeServer.h / .cpp      Server HTTP/WebSocket locale per smartphone PWA (porta 8088)
│
└── Utils/
    └── qrcodegen.hpp / .cpp        Generatore QR Code vettoriale a singolo file
```

### 5.2 Piano di Decomposizione di `MainComponent.cpp`
Attualmente `MainComponent.cpp` contiene ancora blocchi storici di disegno inline. Per garantire manutenibilità e coerenza cross-sessione:
1. **Estrazione di `CanvasRenderer`:** Raccoglie tutti i metodi di disegno del canvas (`drawSketchBox`, `drawCanvasNodes`, `drawAudioCable`, `drawModulationCable`, marquee selection).
2. **Estrazione di `SidebarComponent`:** Raccoglie la gestione della sidebar destra, il catalogo categorie (Strumenti, Effetti, Utility) e lo scorrimento verticale.
3. **Estrazione di `LiveSceneBar` & `BottomDock`:** Raccoglie i 5 slot scena, il pulsante 1-button looper e la commutazione delle tab (Studio, Live, Community).
4. `MainComponent.cpp` manterrà unicamente:
   - Istanziazione di `AudioEngine`, `HardwareWorkers`, `CanvasRenderer`, `SidebarComponent`.
   - Dispatching degli eventi di mouse ad alto livello (zoom, pan, drag mode).
   - Loop a 60 FPS del `timerCallback` limitato all'aggiornamento dei livelli audio e delle modulazioni CV.
