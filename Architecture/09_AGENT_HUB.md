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
- **Navigazione Fluida:** Spazio di lavoro libero con pan fluido (trascinamento / spazio+mouse) e zoom continuo incentrato sulla posizione del cursore.
- **Console Centrale MiMO:** Nodo centrale che rappresenta la base hardware (Rock Pi 4), disegnato come una card moderna ad angoli arrotondati con indicatori LED piatti che riflettono lo stato in tempo reale.
- **Sidebar a Scomparsa (Destra):** Pannello laterale retraibile con schede piatte suddivise per categorie:
  - **Strumenti:** Banchi campionati (`sfizz`), modelli fisici, sintetizzatori DSP (`Faust`).
  - **Effetti:** Filtri, delay, riverberi, modulatori, saturazioni.

### 1.2 Interazione Drag & Drop Intuitiva
- **Assegnazione Strumenti:** Trascina uno strumento dalla sidebar e rilascialo su uno dei moduli periferici nel canvas (es. Modulo Batteria P1 o Modulo Occhio P2). La card del modulo si illumina con la campitura pastello del colore corrispondente.
- **Inserimento Effetti "Snap-on-Wire":** Trascina un effetto e rilascialo direttamente sul cavo di collegamento tra due blocchi: il cavo vettoriale si divide automaticamente accogliendo la card del nuovo effetto.

### 1.3 Architettura a Nodi Nested (Stile Modern Graph Editor)
- **Macro Node (Vista Esterna Compatta):** All'esterno il nodo complesso appare come una card compatta con bordi arrotondati ed esponendo unicamente le manopole macro essenziali (slider o manopole rotative piatte).
- **Drill-Down a Schermo Intero (Doppio Clic):** Il doppio clic sul blocco esegue uno zoom/espansione nell'architettura interna del nodo (routing a blocchi Faust DSP o catene interne avanzate).
- **Esposizione Dinamica dei Parametri:** I parametri collegati alle porte di uscita/ingresso del nodo vengono esposti automaticamente sulla card genitore nel canvas principale.

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
1. **Auto-Salvataggio nell'Album (Timeout o Click Esterno):**  
   Archiviazione automatica nella galleria preset con nome generato automaticamente (es. *"Cassa Piezo + Space Delay"*).
2. **Ridenominazione Immediata:**  
   Digitando direttamente da tastiera, il titolo della card si aggiorna con un font geometrico moderno e pulito.
3. **Assegnazione Rapida ai 5 Slot (Drag & Drop o Tasto Hardware):**  
   Trascinando la card su uno dei 5 slot Scena in basso (o premendo il tasto hardware corrispondente sulla console Rock Pi), la configurazione attuale viene assegnata allo slot selezionato.

### 2.3 Workspace LIVE (Performance & Preset Gallery)
- **Preset Gallery:** Griglia ordinata e sfogliabile di tutte le Preset Cards salvate.
- **5 Slot Scena a Commutazione Istantanea:** Grandi pulsanti piatti associati ai 5 tasti hardware I2C della Rock Pi.
- **Seamless Spillover:** Commutazione tra le scene in $< 1\text{ ms}$, preservando le code di riverbero e delay senza tagli audio.
- **1-Button Looper Integrato:** Visualizzato a fondo schermo con indicatori geometrici piatti a colori pastello (Rosso per Record, Verde per Play, Giallo ocra per Overdub).

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
- **Sintesi ed Effetti `Faust` DSP (C++):** Compilati in `juce::AudioProcessorGraph` ed esposti tramite macro parametriche flat.

### 3.2 Modalità Pro DAW Bridge (Integrazione Studi Professionali)
- **Bypass DSP:** Risparmio totale di CPU disattivando la sintesi interna quando si usa una DAW esterna (Ableton Live, Bitwig, Logic, Reaper).
- **Virtual MIDI & OSC Routing:**
  - `juce::MidiOutput` crea un bus Virtual MIDI standard (note percussive su Canale 10, assi 3D dell'Occhio su MPE CC 16-18).
  - Telemetria continua con la console Rock Pi per aggiornare lo schermo da 3.5".

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
       |        +------------------------------------------------+       |
       +-----------------------------------------------------------------+
```

### 4.1 Architettura Zero-Install (Smartphone PWA con Google MediaPipe)
- **QR Code Vettoriale Integrato:** Generato istantaneamente in C++ (tramite libreria single-header snella come `qrcodegen`), punta all'URL locale della macchina `http://<IP_LOCALE>:8088/eye`. Inquadrandolo con la fotocamera dello smartphone, il browser mobile apre la PWA locale senza alcuna installazione da app store.
- **MediaPipe Hands a 60 FPS:** Lo smartphone elabora il flusso video tramite WebGL sulla propria GPU, rilevando i **21 landmark 3D** della mano (falangi, nocche, polso e punta di ogni dito).
- **Streaming Dati a Bassa Latenza:** I dati normalizzati (0.0 - 1.0) vengono trasmessi direttamente a MiMO Hub su PC via **WebSocket o UDP/OSC** (tramite modulo `juce_osc`).
- **Webcam Locale di Fallback:** Possibilità di selezionare la webcam del laptop o una telecamera USB tramite `juce::CameraDevice`.

### 4.2 Rappresentazione sul Canvas (Semplificata e Flat)
- **Card del Modulo Compatta:** Sul canvas infinito, il Modulo Occhio occupa una card minimale ad angoli arrotondati con:
  - Icona geometrica stilizzata dell'occhio.
  - Badge a pillola con lo stato di connessione e framerate ("Disconnesso", "Webcam Locale", "Smartphone 60 FPS").
  - Mini indicatore vettoriale pastello ciano che visualizza la posizione della mano nello spazio 3D.
  - Pulsante a icona per aprire il **Popover / Drawer di configurazione** con il QR Code e la calibrazione.
- **Preview Vettoriale delle Mani:** All'interno del popover di configurazione, le mani sono renderizzate come scheletri geometrici moderni (21 punti collegati da linee pulite in colori pastello, es. ciano per mano destra e pesca per mano sinistra).

### 4.3 Mappatura Musicale & Controllo Indipendente per Singolo Dito
- **Controllo Indipendente delle Dita (Pinch Trigger):**
  - Calcolo in tempo reale della distanza euclidea tra la punta del pollice e le rispettive punte di Indice, Medio, Anulare e Mignolo.
  - Ciascun pinch costituisce un trigger percussivo/nota distinto, con velocità dinamica calcolata dalla rapidità di chiusura del gesto.
- **Modulazione Continua 3D:**
  - **Asse X:** Pan stereofonico, spazializzazione o morfologia spettrale.
  - **Asse Y:** Cutoff del filtro, frequenza base o inviluppo.
  - **Asse Z (Distanza dalla camera):** Quantità mandata riverbero / delay o bilanciamento wet/dry.
  - **Apertura Palmo / Flessione complessiva:** Modulatore globale assegnabile alle macro degli strumenti o degli effetti.

---

## Pilastro 5: Stack Tecnologico & Implementazione Grafica JUCE

### 5.1 Motore Grafico e Standard UI C++20
*   **Linguaggio:** C++20.
*   **Framework:** JUCE 7 / 8 (`juce_core`, `juce_events`, `juce_graphics`, `juce_data_structures`, `juce_gui_basics`, `juce_gui_extra`, `juce_audio_basics`, `juce_audio_devices`, `juce_audio_processors`, `juce_osc`, `juce_video`).
*   **Tecnica di Rendering Flat Moderno:**
    - Tracciati geometrici precisi con `juce::Graphics::drawRoundedRectangle`, `fillRoundedRectangle` e curve di Bézier nitide.
    - Nessun jitter casuale, nessuna texture di carta o rumore frattale: campiture a tinta piatta e gradienti lineari soft.
    - Cavi di connessione disegnati con spline vettoriali antialiased ad alto contrasto.
    - Controlli rotativi moderni (flat arc knobs con indicatore ad arco color pastello).
    - Gestione dello spazio di lavoro con `juce::Viewport` e matrice di trasformazione affine per pan e zoom fluidi.

### 5.2 Roadmap Modulare di Sviluppo
1.  **Fase 1 (Completata):** Scheletro standalone C++ JUCE con CMake, LookAndFeel base e finestra nativa.
2.  **Fase 2 (In Corso):** Implementazione del Canvas Infinito in stile Modern Flat & Pastel Minimal, Dock inferiore (Studio, Live, Community) e sidebar a scomparsa.
3.  **Fase 3 (Modulo Occhio & Rete):** Integrazione modulo `juce_osc`, server locale con QR Code e Web App PWA MediaPipe Hands a 21 landmark.
4.  **Fase 4 (Nodi & Snapshot Cards):** Architettura a nodi nested con drill-down a schermo intero e sistema di salvataggio Preset Cards.
5.  **Fase 5 (Audio Core & Virtual MIDI):** Integrazione DSP Faust, campionatore `sfizz`, driver ASIO/WASAPI a bassa latenza e porte Virtual MIDI.
