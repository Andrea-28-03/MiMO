# 11_CODEBASE_STRUCTURE_STANDARDS: Standard Architetturale dei 4 Pilastri & Regola Anti-Monolite

## Scopo del Documento
Questo standard definisce le regole architetturali vincolanti per la struttura della codebase C++20 / JUCE di **MiMO Hub** (`mimo_hub_juce/Source/`).
Tutti gli sviluppatori e gli assistenti AI in qualsiasi sessione di lavoro **devono conformarsi rigidamente a questo schema** per impedire la ricrescita di file monolitici e prevenire conflitti durante modifiche simultanee (es. modulo videocamera, DSP o canvas).

---

## I 4 Pilastri Architetturali di MiMO Hub

L'albero dei sorgenti in `mimo_hub_juce/Source/` è suddiviso rigorosamente secondo i 4 Pilastri funzionali:

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
│                                   Inizializza audio, workers e sub-componenti grafici; non contiene algoritmi grafici raw.
│
├── Networking/
│   ├── MiMOEyeServer.h / .cpp      Server HTTP/WebSocket locale per smartphone PWA (porta 8088)
│
└── Utils/
    └── qrcodegen.hpp / .cpp        Generatore QR Code vettoriale a singolo file
```

---

## Regole Inviolabili per Qualsiasi Modifica di Codice

### 1. Limite Dimensionale (Regola Anti-Monolite)
* Nessun file sorgente (`.cpp` o `.h`) deve superare le **800 - 1.000 righe di codice**.
* `MainComponent.cpp` ha storicamente accumulato logiche eterogenee raggiungendo ~8900 righe. **È vietato aggiungere nuove funzionalità direttamente in `MainComponent.cpp`**.
* Nuove funzionalità devono essere implementate estraendo o espandendo i componenti modulari specifici (`CanvasRenderer`, `SidebarComponent`, `MiMOEyeDrawer`, ecc.).

### 2. Confini di Modifica per Dominio
* **Modulo Videocamera & Computer Vision:**
  - Qualsiasi intervento su MediaPipe, streaming webcam, scheletro delle mani o pinch gesture deve risiedere in `Devices/HardwareWorkers.*`, `GUI/MiMOEyeDrawer.*` o `Networking/MiMOEyeServer.*`.
  - Non inserire codice video, calcoli su falangi o matrici di landmark dentro `MainComponent.cpp`.
* **Motore Sonoro & DSP:**
  - Ogni strumento, effetto o utility deve avere il proprio file header dedicato in `Source/Audio/Modules/{Instruments, Effects, Utilities}/`.
  - `AudioEngine.h` funge unicamente da host/router e non deve implementare algoritmi di sintesi o filtraggio inline.
  - Zero allocazioni di memoria dinamica (`new`, `malloc`, `.push_back()`) e zero lock all'interno del callback `audioDeviceIOCallbackWithContext`.
* **Canvas, Cavi & Interazione:**
  - La topologia, le strutture dati e la matematica delle coordinate devono risiedere in `GUI/CanvasGraph.*`.
  - Lo stile e le palette pastello risiedono in `GUI/MiMOLookAndFeel.*`.

### 3. Procedura Operativa Obbligatoria (Cross-Chat Safety)
1. **Verifica Pre-Modifica:** Eseguire sempre `git status` e `git diff` prima di iniziare qualsiasi lavoro per rilevare eventuali modifiche parallele apportate da altre sessioni (es. sul modulo videocamera).
2. **Modularità Nativa:** Creare sempre il modulo o componente isolato prima di collegarlo al coordinatore.
3. **Verifica Post-Modifica:** Ricompilare sempre in configurazione `Release` con CMake verificando codice di uscita 0 (`MSBuild`).
4. **Tracciamento:** Aggiornare lo stato del task in `ROADMAP_TODO.md` e committare su Git con un messaggio semantico chiaro.
