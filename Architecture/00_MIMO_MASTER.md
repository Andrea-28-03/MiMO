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

### 1. Base Centrale (Controller & Router Embedded)
* Basata su SBC **Rock Pi 4 Plus v1.73** (SoC Rockchip RK3399) con Linux (Armbian Minimal CLI).
* **Ruolo:** Controller fisico dedicato con display SPI 3.5", encoder rotativi e tasti per la performance. Comunica via USB Seriale / OSC con MiMO Hub senza eseguire DSP audio pesante on-board.

### 2. Moduli Periferici
* **Modulo Batteria / Percussioni:** Fino a 5 sensori piezoelettrici preamplificati con rilevamento di dinamica e trigger veloci gestiti da Raspberry Pi Pico.
* **Modulo Occhio (Computer Vision):** Rilevamento gestuale 3D a 60 FPS (MediaPipe Hands / Camera) con pinch su ciascun dito e coordinate spaziali continue ($X, Y, Z$, Tilt, Fist).
* **Modulo Microfono:** Ingresso audio per trigger su soglia dinamica interattiva, inviluppo (envelope follower) e intonazione vocale (pitch detection).

### 3. MiMO Hub Desktop (C++20 & JUCE)
* **Grafica Pastello & Canvas Dinamico:** Foglio da disegno a nodi liberamente navigabile, selezione multipla, auto-inserimento su cavo (drop-on-wire) e zoom fluido.
* **Motore DSP Disaccoppiato (`AudioEngine`):**
  * Thread-safe, privo di allocazioni dinamiche o lock nel thread audio real-time (`audioDeviceIOCallbackWithContext`).
  * Processori sonori modulari derivati da `NodeAudioProcessor`:
    * *Strumenti:* AnalogDrumKit, MelodicSynthVoice (VA & Felt Piano), Theremin3DVoice, KarplusPluckVoice.
    * *Effetti:* SVFFilter, SoftSaturator, StereoTapeDelay, LushReverb, BBDChorus, MoogLadderFilter.
    * *Utility & CV:* MultiWaveLFO (CV continuo + Clock Trigger Out), EnvelopeFollower, ScaleLock, TapeLooperProcessor.
  * **Topologia a 3 Livelli di Segnale:**
    1. **Audio:** Cavi scuri ad inchiostro stilografico per il flusso sonoro stereo.
    2. **CV Modulation:** Cavi pastello colorati per modulare in tempo reale qualsiasi manopola.
    3. **Trigger Impulses:** Cavi arancione fiamma a rombo per clock, trig percussivi e pinch delle dita.

---

## Regole Auree di Sviluppo
1. **Real-time Safety:** Nessuna allocazione di memoria (`new`, `malloc`, resize di `std::vector`) né mutex bloccanti nel thread audio ad alta priorità.
2. **Modularità Rigorosa:** Ogni strumento, effetto o utility deve risiedere nel proprio file header dedicato in `Source/Audio/Modules/` per mantenere il codice leggibile, testabile ed estendibile.
3. **Persistenza & Sincronizzazione:** Qualsiasi nuova funzionalità o decisione di design deve essere registrata in `ROADMAP_TODO.md` e nei documenti architetturali pertinenti.
