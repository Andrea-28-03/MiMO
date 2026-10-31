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

## Sistema di Routing tramite Tag e Collaborazione Cross-Agent
Quando l'utente inserisce uno o più Tag nel prompt, adotta le direttive del file corrispondente:
*   `@HARDWARE` -> `01_AGENT_HARDWARE.md` (GPIO, Interrupts, I2C, Multiplexer, HAL su SBC).
*   `@GUI` -> `02_AGENT_GUI.md` (Rendering Headless su Framebuffer SPI 3.5" della SBC).
*   `@COMMS` -> `03_AGENT_COMMS.md` (Seriale USB, Protocollo Universale Modulo Occhio, Sync tra SBC, Smartphone e PC).
*   `@LOGIC` -> `04_AGENT_LOGIC.md` (Macchina a Stati sulla SBC: STUDIO, LIVE e Workspace Custom).
*   `@DATA` -> `05_AGENT_DATA.md` (JSON, Atomic Writes, configurazioni HAL e pacchetti `.mimo`).
*   `@TESTER` -> `06_AGENT_TESTER.md` (Script diagnostici isolati per collaudo hardware e rete).
*   `@DEBUG` -> `07_AGENT_DEBUG.md` (Analisi traceback, logging, debug elettronico e software).
*   `@AUDIO` -> `08_AGENT_AUDIO.md` (Motore DSP Sonoro su PC/Mac dentro MiMO Hub, vincolato a `_DSP_REFERENCES/00_DSP_KNOWLEDGE_BASE.md`).
*   `@HUB` -> `09_AGENT_HUB.md` (Interfaccia Micro-DAW, Web App per Smartphone "MiMO Eye" e Community Marketplace).
*   `@DEVOPS` -> `10_AGENT_DEVOPS.md` (Versioning Git, Backup modulari per Core, Hub, Tests e Architettura).
*   `@STANDARDS` -> `11_CODEBASE_STRUCTURE_STANDARDS.md` (Regole vincolanti sulla struttura dei 4 Pilastri e prevenzione monolite).

> [!NOTE]
> **Modalità Operativa Flessibile per l'Utente:**  
> I tag sopra elencati sono pienamente operativi e possono essere usati quando si vuole indirizzare una richiesta a un dominio specifico.  
> Tuttavia, **l'utente NON è obbligato a ricordarli o a scriverli nel prompt**: in assenza di tag espliciti, l'assistente agisce come coordinatore unico e applica autonomamente tutte le regole architetturali in base al contesto.

**Collaborazione Cross-Agent:** Se un prompt invoca più agenti (es. `@LOGIC @HUB`), mantieni separati il codice Python che gira sulla Rock Pi e il codice dell'applicazione Desktop, mostrando chiaramente il protocollo di scambio dati attraverso `@COMMS`.

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
