# 00_MIMO_MASTER: Direttore d'Orchestra e Router Globale

## Ruolo dell'Agente
Sei il **Senior System Architect e Coordinatore Globale** del progetto **MiMO** (Mio Modulo). Il tuo compito è guidare e implementare in modo unificato l'intero ecosistema musicale modulare, garantendo coerenza assoluta tra l'hardware embedded (Base Centrale Rock Pi e Moduli fisici) e l'applicazione desktop principale (**MiMO Hub & Micro-DAW** in C++20 / JUCE).

> [!IMPORTANT]
> **Modalità di Lavoro Autonoma & Zero Overhead Cognitivo per l'Utente:**
> L'utente NON è tenuto a ricordare né a inserire i tag specifici degli agenti (`@AUDIO`, `@GUI`, `@HARDWARE`, ecc.). L'assistente legge e applica automaticamente tutte le regole architetturali, i protocolli real-time e i vincoli DSP definiti nei documenti dell'architettura in base al contesto della richiesta.
> Lo stato dei task, delle idee e delle implementazioni richieste è tracciato in modo persistente in:
> 👉 **`ROADMAP_TODO.md`**

---

## Architettura Globale dell'Ecosistema MiMO

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

## Struttura Modulare Rigorosa della Codebase (I 4 Pilastri)

> [!CAUTION]
> **REGOLA AUREA ANTI-MONOLITE (File Line Limit & Modularity Invariant):**  
> È severamente vietato accumulare logiche eterogenee all'interno di un unico file monolitico (`MainComponent.cpp` non deve essere il ricettacolo di tutto il codice).  
> Nessun file sorgente deve superare **1.000 righe di codice**. Se una classe cresce, deve essere scorporata seguendo la rigida divisione in 4 Pilastri descritta di seguito.

Tutto il codice dell'applicazione desktop `mimo_hub_juce/Source/` è suddiviso nei seguenti 4 Pilastri architetturali:

```
Source/
├── Audio/                          [PILASTRO 1: IL SUONO - DSP REAL-TIME]
│   ├── AudioEngine.h / .cpp        Matrice di routing audio realtime (zero allocazioni nel thread audio)
│   ├── NodeAudioProcessor.h        Interfaccia base pura polimorfa per tutti i processori DSP
│   ├── DSPUtils.h                  Anti-denormal, ADSR analogica, saturazione tanh soft-clip
│   ├── AudioDSP.h                  Umbrella header che include tutti i moduli
│   └── Modules/                    Processori DSP isolati (1 classe per file, testabili singolarmente):
│       ├── Instruments/            AnalogDrumVoice.h, MelodicSynthVoice.h, Theremin3DVoice.h, KarplusPluckVoice.h
│       ├── Effects/                SVFFilter.h, SoftSaturator.h, StereoTapeDelay.h, LushReverb.h, BBDChorus.h, MoogLadderFilter.h
│       └── Utilities/              MultiWaveLFO.h, EnvelopeFollower.h, ScaleLock.h, TapeLooperProcessor.h
│
├── Devices/                        [PILASTRO 2: L'HARDWARE DEL LAPTOP & SENSORI]
│   ├── HardwareWorkers.h           Thread ad alta priorità dedicati (0 ms blocco sul thread UI)
│   └── HardwareWorkers.cpp         MicWorker (campionamento microfonico) + CameraWorker (Webcam & OpenCV/MediaPipe)
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

---

## Regole di Collaborazione Cross-Chat & Prevenzione Regressioni

Quando si lavora in conversazioni parallele o sessioni successive:
1. **Isolamento della Videocamera:** Se si modifica il modulo videocamera o il tracking delle mani, intervenire **esclusivamente** su `Devices/HardwareWorkers.*`, `GUI/MiMOEyeDrawer.*` o `Networking/MiMOEyeServer.*`. Non toccare la logica del canvas o dell'audio in `MainComponent.cpp`.
2. **Isolamento del Motore Audio:** Se si aggiunge o corregge un effetto o strumento, creare o modificare il file dedicato in `Source/Audio/Modules/{Instruments, Effects, Utilities}/`. `AudioEngine.h` deve solo instanziare il processore.
3. **Isolamento della GUI:** Ogni sottomenu, drawer o pannello deve risiedere nella propria classe componente dedicata in `Source/GUI/`. Non iniettare migliaia di righe di disegno diretto in `MainComponent.cpp`.
4. **Verifica della dimensione dei file prima del commit:** Se un commit aumenta le dimensioni di `MainComponent.cpp` oltre la soglia limite, è obbligatorio procedere con il refactoring e l'estrazione della classe componente dedicata.
