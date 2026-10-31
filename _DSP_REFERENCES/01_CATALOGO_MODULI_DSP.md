# 01_CATALOGO_MODULI_DSP: Registro & Roadmap Ufficiale dei Moduli MiMO Hub (V2 - Grafo Dinamico)

Questo documento costituisce il **Registro Ufficiale dei Moduli (Strumenti, Effetti, Modulatori e Utility)** dell'ecosistema **MiMO Hub & Micro-DAW**.

---

## 0. I 4 Standard Architetturali Obbligatori per Ogni Modulo (V2)

A seguito dell'audit sul motore audio e sull'interfaccia grafica, ogni modulo dell'ecosistema deve soddisfare 4 requisiti inderogabili:

1. **Volume di Uscita e Mix Dry/Wet Reali su Ogni Effetto:**
   - Nessun effetto può essere inserito senza una manopola di **Mix Dry/Wet (0 - 100%)** e un controllo di **Output Level (+/- 12 dB)**.
   - Il volume deve agire direttamente sul buffer audio del nodo.
2. **Interruttore Power / Bypass con LED:**
   - Ogni scheda sul Canvas dispone di un interruttore di Bypass rapido. Quando è disattivato, il segnale audio attraversa il nodo intatto con 0% di consumo CPU e il LED del modulo si spegne.
3. **Mini VU-Meter / Feedback di Segnale Reale:**
   - Ogni nodo espone un valore atomico `realtimePeak` letto dal processore audio a 60 Hz. L'interfaccia disegna una barretta LED luminosa sul card: l'utente vede con gli occhi dove scorre il suono.
4. **Architettura a Grafo Audio Dinamico (Nessuna Catena Hardcoded):**
   - I suoni non passano più dentro una catena fissa globale. Ogni nodo sul canvas corrisponde a un'istanza autonoma derivata da `mimo::audio::NodeAudioProcessor`. I cavi audio deviano e sommano i singoli buffer in modo discreto e isolato.

---

## Indice delle Categorie & Priorità

- [Fase 1: Gli Strumenti Fondamentali (Core Sound Generators)](#fase-1-gli-strumenti-fondamentali-core-sound-generators)
- [Fase 2: Gli Effetti Chiave con Volume & Mix Reale (Essential FX)](#fase-2-gli-effetti-chiave-con-volume--mix-reale-essential-fx)
- [Fase 3: Modulatori & Utility Intelligenti (Smart Utilities & CV)](#fase-3-modulatori--utility-intelligenti-smart-utilities--cv)
- [Fase 4: Espansioni Sonore Avanzate (Extended Palette)](#fase-4-espansioni-sonore-avanzate-extended-palette)
- [Matrice di Stato e Avanzamento](#matrice-di-stato-e-avanzamento)

---

## Fase 1: Gli Strumenti Fondamentali (Core Sound Generators)

### 1.1 Kit Percussivo Analogico Reattivo (`ins-drum-analog`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Rifattorizzazione con generatore discreto a 4 voci indipendenti, non più hardcoded)
- **Destinazione d'Uso:** Modulo Batteria P1, Microfono/Piezo da tavolo, colpi dita.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia Pastello)
- **Caratteristiche Sonore:**
  1. *Kick / Sub 808*: Inviluppo di frequenza esponenziale rapido (da 180 Hz a 45 Hz) con saturazione morbida $\tanh$.
  2. *Snare / Snappy*: Corpo intonato a doppio oscillatore a T-bridge + generatore di rumore filtrato passa-banda.
  3. *Hi-Hat / Metal*: Banco di 6 oscillatori ad onda quadra sfasati (modello Roland TR-808/909) con filtro passa-alto a 8 kHz.
  4. *Acoustic Rim / Woodblock*: Modello risonante a barra legnosa per valorizzare i colpi sulle superfici acustiche.
- **Manopole Frontali Macro:** `Volume` (0 - 150%), `Pitch/Accordo` (-12 .. +12 st), `Decay/Durata` (0.05 .. 1.5 s), `Snap/Attacco` (0 - 100%)
- **Prese Jack I/O:** `trigger:in` (con velocity dinamica), `audio:out` (Stereo), `param:pitch`, `param:decay`
- **Riferimenti Bibliografici:**
  - Will C. Pirkle ("Designing Audio Effect Plugins in C++") - Envelope generator analog-modeled.
  - Émilie Gillet / Mutable Instruments Plaits (`analog_bass_drum.cc`, `analog_snare_drum.cc`).

---

### 1.2 Theremin 3D Gestuale a Scala Quantizzata (`ins-theremin-3d`)
- **Stato:** `[⭐ PRIORITÀ ASSOLUTA FASE C]` (Fondamentale per MiMO Eye / Smartphone / MediaPipe)
- **Destinazione d'Uso:** Suonare melodie in aria con le mani a 60 FPS senza stonare.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCyan` (Ciano Pastello)
- **Caratteristiche Sonore:**
  - Generatore a onda variabile (Sinusoide pura $\rightarrow$ Triangolo dolce $\rightarrow$ Saturazione armonica).
  - Smoothing one-pole avanzato sul pitch per eliminare qualsiasi jitter della webcam.
  - **Mappatura Assi 3D:**
    - *Asse X (Orizzontale)* $\rightarrow$ Pitch quantizzato alla scala musicale attiva.
    - *Asse Y (Altezza)* $\rightarrow$ Timbro / Morfologia filtro vocale.
    - *Asse Z (Profondità)* $\rightarrow$ Volume / Dinamica naturale o ampiezza vibrato.
- **Manopole Frontali Macro:** `Volume` (0 - 150%), `Scala` (Pentatonica, Minore, Maggiore, Blues, Dorica), `Glide/Portamento` (0 - 350 ms), `Vibrato` (0 - 100%)
- **Prese Jack I/O:** `mod:x`, `mod:y`, `mod:z`, `trigger:in` (opzionale pinch per articolazione), `audio:out`
- **Riferimenti Bibliografici:**
  - Udo Zölzer ("DAFX", Cap. 3: Modulators - Heterodyne Oscillator & Continuous Pitch).
  - Vadim Zavalishin ("The Art of VA Filter Design", One-pole smoothers).

---

### 1.3 Karplus-Strong Pluck (Kalimba & Felt Chime) (`ins-karplus-pluck`)
- **Stato:** `[⭐ PRIORITÀ ASSOLUTA FASE C]`
- **Destinazione d'Uso:** Modello fisico di corda/lamella metallica pizzicata; ideale per trigger pinch dita.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Caratteristiche Sonore:**
  - Linea di ritardo frazionaria ricorsiva con filtro di smorzamento passa-basso nel loop.
  - Reattività organica alla velocity: a colpi leggeri il suono è ovattato e caldo (pianoforte feltro); a colpi forti è scintillante e metallico (kalimba/carillon).
- **Manopole Frontali Macro:** `Volume` (0 - 150%), `Brilliance` (0 - 100%), `Corpo/Damping` (0.1 - 4.0 s), `Tono/Accordo`
- **Prese Jack I/O:** `trigger:in`, `audio:in` (per eccitazione esterna con voce o microfono), `audio:out`, `param:pitch`, `param:decay`
- **Riferimenti Bibliografici:**
  - Julius O. Smith III ("Physical Audio Signal Processing", Karplus-Strong Algorithm & Digital Waveguide Synthesis).
  - Kevin Karplus & Alex Strong (1983, Computer Music Journal).

---

### 1.4 Virtual Analog Lead & Bass (`ins-synth-va`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Risoluzione del loop monolitico; voce polifonica/parafonica con inviluppo rapido)
- **Destinazione d'Uso:** Bassline grasse, arpeggi ed effetti lead controllati da sequencer o tastiera.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Caratteristiche Sonore:**
  - Oscillatori PolyBLEP anti-aliased con morphing (Saw $\rightarrow$ Square/PWM $\rightarrow$ Sub-Osc).
  - Inviluppo ADSR analogico esponenziale a risposta rapida.
- **Manopole Frontali Macro:** `Volume` (0 - 150%), `Cutoff` (60 - 16000 Hz), `Resonance` (0 - 100%), `Decay/Release`
- **Prese Jack I/O:** `trigger:in`, `audio:out`, `param:cutoff`, `param:pitch`
- **Riferimenti Bibliografici:**
  - Vadim Zavalishin ("The Art of VA Filter Design", Cap. 4).
  - Will C. Pirkle ("Designing Audio Effect Plugins in C++").

---

## Fase 2: Gli Effetti Chiave con Volume & Mix Reale (Essential FX)

Ogni effetto implementa l'interfaccia `NodeAudioProcessor` con:
- `setDryWet(float mixNorm)`
- `setOutputGain(float gainLinear)`
- `setBypass(bool isBypassed)`
- Calcolo del picco audio in uscita per alimentare il VU-Meter del card.

---

### 2.1 Saturazione a Nastro & Overdrive Pastello (`fx-saturazione-soft`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Aggiunta controllo Mix reale e Output Gain compensato)
- **Destinazione d'Uso:** Scaldare i suoni digitali, dare spessore alla batteria e al basso.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Caratteristiche Sonore:**
  - Modellazione non-lineare sigmoidale asimmetrica per generazione di armoniche pari e dispari.
  - Filtro di tono pre/post saturazione (de-enfasi) per evitare asprezze digitali sulle alte.
- **Manopole Frontali Macro:**
  1. `Drive` (0 - 36 dB)
  2. `Tono` (400 - 14000 Hz)
  3. `Mix Dry/Wet` (0 - 100%)
  4. `Output Volume` (0 - 150%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:drive`, `param:mix`
- **Riferimenti Bibliografici:**
  - Udo Zölzer ("DAFX", Cap. 4: Nonlinear Processing - Polynomial and Hyperbolic Waveshaping).

---

### 2.2 Filtro Naif ZDF SVF Stereo (`fx-filtro-svf`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Isolamento come modulo indipendente con Bypass e Mix)
- **Destinazione d'Uso:** Sweeps di frequenza, wah-wah controllato dalla mano, modulazione ritmica.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCyan` (Ciano Pastello)
- **Caratteristiche Sonore:**
  - Filtro a Stato Variabile Zero-Delay Feedback (ZDF) bilineare con preservazione della topologia analogica.
  - Nessun cramming o perdita di risonanza vicino alla frequenza di Nyquist.
  - Selettore modalità: Low-Pass, Band-Pass, High-Pass.
- **Manopole Frontali Macro:**
  1. `Cutoff` (20 - 20000 Hz)
  2. `Risonanza` (0.1 - 10.0, con autolimitazione morbida)
  3. `Mix Dry/Wet` (0 - 100%)
  4. `Output Volume` (0 - 150%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:cutoff`, `param:res`
- **Riferimenti Bibliografici:**
  - Vadim Zavalishin ("The Art of VA Filter Design", Cap. 4: The State Variable Filter).

---

### 2.3 Eco a Nastro Stereo Ping-Pong (`fx-eco-nastro`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Risoluzione del buffer di delay per canale e gestione del volume)
- **Destinazione d'Uso:** Spazialità ritmica, code dub, allargamento stereo per chitarre e percussioni.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Caratteristiche Sonore:**
  - Delay a nastro frazionario con interpolazione lineare morbida (glissato analogico durante il cambio tempo).
  - Filtro di perdita ad ogni ripetizione (il segnale si scurisce progressivamente come su un vero registratore a bobine).
  - Modalità Ping-Pong con rimbalzo alternato tra canale L e R.
- **Manopole Frontali Macro:**
  1. `Tempo` (20 - 1200 ms o sync in battiti)
  2. `Feedback` (0 - 95%)
  3. `Mix Dry/Wet` (0 - 100%)
  4. `Output Volume` (0 - 150%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:time`, `param:feedback`, `param:mix`
- **Riferimenti Bibliografici:**
  - Udo Zölzer ("DAFX", Cap. 2: Delay Effects - Tape Saturation & Wow/Flutter).

---

### 2.4 Riverbero d'Ambiente Lush FDN (`fx-riverbero-fdn`)
- **Stato:** `[🔄 IN REFACTORING V2]` (Routing dedicato per nodo con Mix reale e Output Volume)
- **Destinazione d'Uso:** Spazio tridimensionale profondo, code eteree per theremin e pianoforte.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonLilac` (Lilla Pastello)
- **Caratteristiche Sonore:**
  - Feedback Delay Network (FDN) a matrice ortogonale con linee di ritardo mutualmente prime.
  - Filtri di smorzamento ad assorbimento d'aria (formula di Sabine) e assenza totale di risonanze metalliche.
- **Manopole Frontali Macro:**
  1. `Stanza/Size` (0 - 100%)
  2. `Damping HF` (0 - 100%)
  3. `Mix Dry/Wet` (0 - 100%)
  4. `Output Volume` (0 - 150%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:room`, `param:damp`, `param:mix`
- **Riferimenti Bibliografici:**
  - Julius O. Smith III ("Physical Audio Signal Processing", Section: Feedback Delay Networks - FDN).

---

## Fase 3: Modulatori & Utility Intelligenti (Smart Utilities & CV)

### 3.1 Smart Scale Lock (Quantizzatore Tonale Etereo) (`ut-scale-lock`)
- **Stato:** `[⭐ PRIORITÀ FASE C]` (Abbinamento perfetto con Theremin e gesture MediaPipe)
- **Funzione:** Prende in ingresso una tensione continua o una coordinata della mano (0.0 .. 1.0) e la converte istantaneamente nella frequenza esatta della scala musicale selezionata, eliminando stonature.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Manopole Frontali:** `Tonica` (Do..Si), `Tipo Scala` (Pentatonica, Minore, Maggiore, Blues, Dorica), `Ottave` (1 - 4)
- **Prese Jack I/O:** `mod:in` (CV da mano o LFO), `mod:out` (CV quantizzato per manopole Pitch)
- **Riferimento Bibliografico:** Surge XT Synthesizer Scale & Microtonal Engine.

---

### 3.2 Seguitore d'Inviluppo Audio (Envelope Follower) (`mod-env-follower`)
- **Stato:** `[⭐ PRIORITÀ FASE C]`
- **Funzione:** Converte l'intensità del suono in ingresso (es. colpo del microfono sul tavolo o voce) in un segnale di modulazione CV (0.0 .. 1.0) per aprire filtri o variare delay dinamicamente.
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonOchre` (Giallo Ocra)
- **Manopole Frontali:** `Attacco` (1 - 50 ms), `Rilascio` (10 - 500 ms), `Guadagno/Sensibilità`, `Inversione`
- **Prese Jack I/O:** `audio:in`, `mod:out`
- **Riferimento Bibliografico:** Udo Zölzer ("DAFX", Cap. 5: Envelope Extractors).

---

### 3.3 1-Button Looper a Nastro Integrato (`ut-looper-tape`)
- **Stato:** `[🔄 IN INTEGRAZIONE COME NODO]`
- **Funzione:** Modulo looper circolare sincrono posizionabile sul canvas per catturare frasi audio al volo con sovraincisione continua (Sound-on-Sound).
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia)
- **Manopole Frontali:** `Overdub Feedback` (0 - 100%), `Volume Loop`, `Play/Stop`
- **Prese Jack I/O:** `audio:in`, `trigger:in` (Start/Stop/Overdub), `audio:out`
- **Riferimento Bibliografico:** Udo Zölzer ("DAFX", Sound-on-Sound looping).

---

## Fase 4: Espansioni Sonore Avanzate (Extended Palette)

I seguenti moduli rimangono registrati nel catalogo per le iterazioni successive:
- **`fx-moog-ladder`**: Filtro 24 dB/oct a 4 poli analog ladder con transistor drive saturante.
- **`fx-cloud-reverb`**: Riverbero shimmer granulare con diffusione a shift di ottava.
- **`fx-vowel-formant`**: Filtro formantico per effetti vocali talkbox.
- **`fx-compressor-vca`**: Compressore da studio con ingresso sidechain per effetto pumping.
- **`fx-transient-shaper`**: Esaltatore di attacco/punch per percussioni.
- **`fx-chorus-bbd`**: Chorus stereo a ritardo variabile stile Bucket-Brigade.
- **`ins-granular-pad`**: Sintetizzatore a grani per texture d'atmosfera.
- **`ins-modal-resonator`**: Risonatore a 64 modi fisici stile Mutable Elements.
- **`mod-lfo-poly`**: Generatore di forme d'onda LFO per modulazioni cicliche.
- **`ut-mixer-4ch`**: Mixer summing stereo a 4 canali con pan-law a -3 dB.

---

## Matrice di Stato e Avanzamento

| Modulo ID | Nome Modulo | Tipo | Priorità | Stato Attuale | Volume & Mix Garantiti |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `ins-drum-analog` | Kit Percussivo Analogico | Strumento | **Alta (P1)** | `[🔄 Refactoring V2]` | Sì (Output Level + Voice Vol) |
| `ins-theremin-3d` | Theremin 3D Gestuale | Strumento | **Alta (P1)** | `[⭐ Da Fare Fase C]` | Sì (Output Level) |
| `ins-karplus-pluck`| Kalimba & Felt Pluck | Strumento | **Alta (P1)** | `[⭐ Da Fare Fase C]` | Sì (Output Level) |
| `ins-synth-va` | Virtual Analog Lead/Bass | Strumento | Media | `[🔄 Refactoring V2]` | Sì (Output Level) |
| `fx-saturazione-soft`| Saturazione Nastro | Effetto | **Alta (P1)** | `[🔄 Refactoring V2]` | Sì (Dry/Wet + Output Level) |
| `fx-filtro-svf` | Filtro ZDF SVF Stereo | Effetto | **Alta (P1)** | `[🔄 Refactoring V2]` | Sì (Dry/Wet + Output Level) |
| `fx-eco-nastro` | Eco a Nastro Ping-Pong | Effetto | **Alta (P1)** | `[🔄 Refactoring V2]` | Sì (Dry/Wet + Output Level) |
| `fx-riverbero-fdn`| Riverbero Lush FDN | Effetto | **Alta (P1)** | `[🔄 Refactoring V2]` | Sì (Dry/Wet + Output Level) |
| `ut-scale-lock` | Smart Scale Lock | Utility | **Alta (P1)** | `[⭐ Da Fare Fase C]` | N/A (Control Voltage) |
| `mod-env-follower` | Envelope Follower | Modulatore| Media | `[⭐ Da Fare Fase C]` | N/A (Control Voltage) |
| `ut-looper-tape` | 1-Button Looper | Utility | Media | `[🔄 Integrazione]` | Sì (Loop Level) |
| *Espansioni (15)*| Moog, Shimmer, Vowel, ecc.| Misti | Fase 4 | `[Pianificato]` | Standard V2 |
