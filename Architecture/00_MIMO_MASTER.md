# 00_MIMO_MASTER: Direttore d'Orchestra e Router Globale

## Ruolo dell'Agente
Sei il **Senior System Architect e Router Principale** del progetto **MiMO** (Mio Modulo). Il tuo compito è coordinare lo sviluppo di un ecosistema musicale modulare e open-source, garantendo la coerenza tra l'hardware embedded (la Base Centrale e i Moduli) e il software su computer (**MiMO Hub & Micro-DAW**).

---

## Architettura Globale dell'Ecosistema MiMO
Per garantire prestazioni estreme, costi ridotti e latenza zero, il carico computazionale è distribuito strategicamente:

1.  **Base Centrale (Controller & Router Embedded):**  
    Basata su una SBC **Rock Pi 4 Plus v1.73** (SoC Rockchip RK3399) con Linux (Armbian Minimal CLI). **NON processa audio né visione artificiale**. Gestisce l'interfaccia fisica (display SPI da 3.5", 3 encoder rotativi, 5 pulsanti tattili, pedale sustain), raccoglie i dati dai moduli cablati e permette di modificare in tempo reale gli strumenti, gli effetti e le impostazioni assegnate a ciascun modulo (incluso il Modulo Occhio).
2.  **Moduli Periferici (Architettura Ibrida e Agnostica):**
    *   *Modulo "Batteria" (Percussivo):* Microfoni piezoelettrici DIY preamplificati + **Raspberry Pi Pico** collegato via USB Seriale alla Base Centrale.
    *   *Modulo "Occhio" (Computer Vision Agnostica):* Progettato per sfruttare in Fase 1 la potenza NPU/GPU di uno **Smartphone** (tramite Web App locale/PWA ospitata da MiMO Hub o app OSC) che calcola il tracciamento delle mani a 60 FPS e invia le coordinate direttamente al PC via Wi-Fi/USB, ma compatibile al 100% in Fase 2 con un modulo hardware custom dedicato (es. SBC/Microcontrollore + Camera USB/CSI ad alto framerate) grazie a un protocollo OSC/WebSocket standardizzato.
    *   *Porta "MiMO Link" (Daisy Chain I2C):* Espansione esterna gestita tramite multiplexer PCA9548 per collegare sensori secondari in cascata.
3.  **MiMO Hub & Micro-DAW (Software Desktop su PC/Mac/Linux):**  
    Programma gratuito tutto-in-uno sul computer (C++20 / JUCE). Riceve i flussi ad alta velocità (dalla Base Centrale e dallo Smartphone/Modulo Occhio), genera i suoni tramite il motore DSP integrato, offre un'interfaccia intuitiva per neofiti e ospita il **Community Marketplace** per scaricare/acquistare moduli hardware (file 3D, BOM) e scambiare Preset.

```
+-------------------------------------------------------------------------------+
|                             ECOSISTEMA MODULARE MIMO                          |
+-------------------------------------------------------------------------------+
                                        |
       +--------------------------------+--------------------------------+
       |                                                                 |
       v                                                                 v
+-----------------------------+                         +-----------------------------+
|    BASE CENTRALE & MODULI   |                         |          MIMO HUB           |
| (Embedded Rock Pi 4 / Pico) |                         |  (JUCE Desktop DAW C++20)   |
+-----------------------------+                         +-----------------------------+
| - Rock Pi 4 Plus (Armbian)  |                         | - Canvas V2 Sketch Grafico  |
| - Display SPI 3.5" + Tasti  |       USB Seriale       | - AudioEngine Disaccoppiato |
| - Modulo Batteria (Piezo)   | ----------------------> | - DSP Nativo Real-Time      |
| - MiMO Eye (Computer Vision)|       UDP / OSC         | - Routing Audio + CV + Trig |
| - Bus I2C Daisy-Chain       |                         | - Master Limiter & Export   |
+-----------------------------+                         +-----------------------------+
```

---

## Regole Auree Globali (Inviolabili per tutti gli Agenti)
1.  **Hardware & Source Agnosticism (HAL):** Il codice della Base Centrale deve girare su qualsiasi SBC Linux (Rock Pi, Raspberry Pi, Orange Pi) leggendo i pin da `board_config.json`. Allo stesso modo, i moduli di rete (come il Modulo Occhio) devono comunicare tramite payload standardizzati: al sistema non deve importare se le coordinate della mano arrivano da uno smartphone o da una telecamera hardware dedicata.
2.  **Zero Vendor Lock-in:** Vietato usare librerie proprietarie o esclusive per Raspberry Pi (es. **vietato `RPi.GPIO`**, **vietato `gpiozero`**). Usare solo standard del kernel Linux (`/dev`, `/sys`, Character Devices).
3.  **Asincronia Globale:** Il software sulla SBC gira su un Event Loop asincrono (`asyncio`) per non bloccare mai la lettura degli input o il rendering dello schermo SPI.
4.  **Flessibilità dello Stack Tecnologico:** Ogni agente può proporre librerie più moderne o performanti rispetto a quelle consigliate, purché open-source, asincrone e compatibili cross-platform.
5.  **Real-Time Audio Safety:** Nessuna allocazione dinamica di memoria (`new`, `malloc`, `.push_back()`), deallocazione né lock di mutex all'interno del callback audio (`audioDeviceIOCallbackWithContext`).
6.  **Regola Anti-Monolite & Modularity Invariant:** Nessun file sorgente (`.cpp` o `.h`) deve superare le **800 - 1.000 righe di codice**. `MainComponent.cpp` non deve essere il ricettacolo di logiche eterogenee; le funzionalità devono risiedere nei rispettivi moduli dedicati.

---

## Sistema di Routing Autonomo e Collaborazione Cross-Agent

### 1. Auto-Dispatcher degli Agenti per Intento (Zero Tag Overhead)
L'ecosistema MiMO adotta un sistema di **Auto-Routing Autonomo per Intento**:
* **Nessun obbligo di tag manuali:** Né l'utente né l'assistente devono forzatamente inserire `@TAG` manuali nei prompt per far collaborare le parti del sistema.
* **Attivazione Contestuale Automatica:** In base al contenuto della richiesta o al task preso in carico da `ROADMAP_TODO.md`, l'assistente identifica autonomamente i domini coinvolti, consulta i documenti architetturali di competenza (`01` - `11`), ne rispetta i vincoli inviolabili e fa interagire i componenti con la massima sincronia.

### 2. Mappa di Specializzazione dei Domini (Agenti di Riferimento)
*   `@HARDWARE` (`01_AGENT_HARDWARE.md`): GPIO, Interrupts, I2C, Multiplexer PCA9548, HAL su SBC Linux, sensori piezo su Pico.
*   `@GUI` (`02_AGENT_GUI.md`): Rendering headless e Dirty Rectangles sul display SPI 3.5" (480x320) della Rock Pi.
*   `@COMMS` (`03_AGENT_COMMS.md`): Comunicazione seriale USB, protocolli OSC/WebSocket per Modulo Occhio, sync Rock Pi <-> PC.
*   `@LOGIC` (`04_AGENT_LOGIC.md`): Macchina a stati, logiche di commutazione Studio/Live, gestione scene e routing comandi.
*   `@DATA` (`05_AGENT_DATA.md`): Gestione formati `.mimo`, Preset Cards, file JSON atomici e mapping hardware `board_config.json`.
*   `@TESTER` (`06_AGENT_TESTER.md`): Script di collaudo hardware, unit test e test di sincronizzazione telemetrica.
*   `@DEBUG` (`07_AGENT_DEBUG.md`): Analisi traceback, isolamento glitch e profiling delle prestazioni.
*   `@AUDIO` (`08_AGENT_AUDIO.md`): Motore DSP C++20 real-time in MiMO Hub, vincolato a `_DSP_REFERENCES/00_DSP_KNOWLEDGE_BASE.md`.
*   `@HUB` (`09_AGENT_HUB.md`): Interfaccia grafica desktop in JUCE (Canvas V2, palette pastello, drag-and-drop, nodi).
*   `@DEVOPS` (`10_AGENT_DEVOPS.md`): Gestione versionamento Git, build CMake/MSBuild e integrità dell'ambiente.
*   `@STANDARDS` (`11_CODEBASE_STRUCTURE_STANDARDS.md`): Regole vincolanti sulla struttura dei 4 Pilastri e prevenzione monolite (< 1000 righe).

### 3. Matrice Operativa di Collaborazione Cross-Agent
Quando una funzionalità attraversa più livelli tecnologici, gli agenti collaborano secondo precise interfacce:
1. **Hardware Embedded <-> Desktop GUI/DSP:** Il codice Python su Rock Pi interagisce con l'applicazione desktop C++/JUCE unicamente tramite i payload standardizzati definiti da `@COMMS` (OSC su UDP o pacchetti seriali JSON/MIDI). Non esistono dipendenze dirette tra il runtime Linux della SBC e il framework JUCE sul PC.
2. **Audio DSP <-> Canvas GUI:** I processori in `Source/Audio/Modules/` (`@AUDIO`) sono disaccoppiati dalla GUI (`@HUB`). La GUI legge i peak level e invia valori normalizzati (0.0 - 1.0) tramite i metodi thread-safe di `AudioEngine` senza mai bloccare il thread audio real-time.
3. **Computer Vision / Sensori <-> Strumenti/Parametri:** I worker dedicati in `Source/Devices/` (`@HARDWARE`) acquisiscono webcam/microfono e trasmettono dati normalizzati via atomici. Il canvas (`@HUB`) li mappa graficamente su cavi pastello e `AudioEngine` (`@AUDIO`) li applica ai processori.

---

## Struttura Modulare Rigorosa di MiMO Hub Desktop (I 4 Pilastri)

👉 Riferimento normativo dettagliato: [**`Architecture/11_CODEBASE_STRUCTURE_STANDARDS.md`**](file:///h:/Il%20mio%20Drive/5.%20Programmazione%20MIMO/MiMO_Project/Architecture/11_CODEBASE_STRUCTURE_STANDARDS.md)  
👉 Taskboard operativo di avanzamento: [**`ROADMAP_TODO.md`**](file:///h:/Il%20mio%20Drive/5.%20Programmazione%20MIMO/MiMO_Project/ROADMAP_TODO.md)

Tutto il codice dell'applicazione desktop `mimo_hub_juce/Source/` è suddiviso nei seguenti 4 Pilastri architetturali:

```
Source/
├── Audio/                          [PILASTRO 1: IL SUONO - DSP REAL-TIME]
│   ├── AudioEngine.h / .cpp        Matrice di routing audio realtime (zero allocazioni nel thread audio)
│   ├── NodeAudioProcessor.h        Interfaccia base pura polimorfa per tutti i processori DSP
│   ├── DSPUtils.h                  Anti-denormal, ADSR analogica, saturazione tanh soft-clip
│   ├── AudioDSP.h                  Umbrella header che include tutti i moduli
│   └── Modules/                    Processori DSP isolati (1 header per classe, compilazione autonoma):
│       ├── Instruments/            AnalogDrumVoice.h, MelodicSynthVoice.h, Theremin3DVoice.h, KarplusPluckVoice.h
│       ├── Effects/                SVFFilter.h, SoftSaturator.h, StereoTapeDelay.h, LushReverb.h, BBDChorus.h, MoogLadderFilter.h
│       └── Utilities/              MultiWaveLFO.h, EnvelopeFollower.h, ScaleLock.h, TapeLooperProcessor.h
│
├── Devices/                        [PILASTRO 2: L'HARDWARE DEL LAPTOP & SENSORI]
│   ├── HardwareWorkers.h           Thread ad alta priorità dedicati (0 ms blocco sul thread UI)
│   └── HardwareWorkers.cpp         MicWorker (WASAPI input/synth) + CameraWorker (Webcam & OpenCV/MediaPipe)
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
│                                   Inizializza audio, workers e sub-componenti grafici; non contiene algoritmi grafici raw.
│
├── Networking/
│   ├── MiMOEyeServer.h / .cpp      Server HTTP/WebSocket locale per smartphone PWA (porta 8088)
│
└── Utils/
    └── qrcodegen.hpp / .cpp        Generatore QR Code vettoriale a singolo file
```
