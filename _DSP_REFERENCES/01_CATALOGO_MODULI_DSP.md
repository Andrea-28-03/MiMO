# 01_CATALOGO_MODULI_DSP: Matrice Completa dei Moduli per MiMO Hub

Questo documento costituisce il **Registro Ufficiale dei Moduli (Strumenti, Effetti, Modulatori e Utility)** dell'ecosistema **MiMO Hub & Micro-DAW**.
Ogni modulo rispetta rigorosamente:
1. **L'estetica "Modern Flat & Pastel Minimal":** Schede opache ad angoli arrotondati, palette pastello, manopole flat arc macro essenziali, porte jack visibili (`audio:in`, `trigger:in`, `audio:out`, `param:CV`).
2. **Il Protocollo Vincolante @AUDIO (FASE B):** Ogni algoritmo cita l'autore, il testo accademico locale (cartella `Documenti sulla musica/`) o il repository open-source di riferimento con capitoli e dettagli matematici.
3. **Lo stato di avanzamento:** Tracciamento immediato di ciò che è già implementato e funzionante (`[X]`) e di ciò che è pianificato per la **FASE C** (`[ ]`).

---

## Indice delle Categorie

- [1. Strumenti & Generatori Sonori (Sound Generators)](#1-strumenti--generatori-sonori-sound-generators)
- [2. Filtri & Modellazione Spettrale (Filters & Tone Shaping)](#2-filtri--modellazione-spettrale-filters--tone-shaping)
- [3. Spazio & Tempo (Time & Space Effects)](#3-spazio--tempo-time--space-effects)
- [4. Dinamica, Saturazione & Distorsione (Dynamics & Color)](#4-dinamica-saturazione--distorsione-dynamics--color)
- [5. Modulatori & Control Voltage (Modulation & CV)](#5-modulatori--control-voltage-modulation--cv)
- [6. Utility Musicali & Routing Intelligente (Smart Utilities)](#6-utility-musicali--routing-intelligente-smart-utilities)

---

## 1. Strumenti & Generatori Sonori (Sound Generators)

### 1.1 Kit Batteria Analogico (`ins-drum-analog`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::AnalogDrumKit`)
- **Categoria Interna:** Percussioni & Drum Kits
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia Pastello)
- **Manopole Frontali Macro:** `Volume` (0.0 - 1.5), `Accordo/Pitch` (-12 .. +12 st), `Decay` (0.05 .. 1.2 s)
- **Prese Jack I/O:**
  - `trigger:in`: Trigger per colpi piezo (Modulo Batteria) o pinch dita (Modulo Occhio) su 4 voci (Cassa, Rullante, Hat, Perc).
  - `audio:out`: Uscita audio stereo normalizzata.
  - `param:pitch`, `param:decay`: Modulazione continua CV.
- **Riferimento Bibliografico Accademico:**
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++")**: Envelope generator analog-modeled con curva di pitch esponenziale percussiva per la cassa.
  - **Émilie Gillet / Mutable Instruments Plaits (`analog_bass_drum.cc`, `analog_snare_drum.cc`)**: Modelli di risonatori a ponte a T a ponte sfasato per grancassa e generatore di rumore filtrato con inviluppo sagomato per rullante e piatti.
  - **Vadim Zavalishin ("The Art of VA Filter Design", Cap. 4)**: Filtraggio passa-banda ZDF del rumore bianco per simulare il ronzio della cordiera del rullante.

---

### 1.2 Sintetizzatore Melodico Virtual Analog (`ins-synth-va`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::MelodicSynthVoice`)
- **Categoria Interna:** Sintesi Virtual Analog
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Manopole Frontali Macro:** `Volume` (0.0 - 1.5), `Cutoff` (100 - 14000 Hz), `Decay` (0.1 - 3.0 s)
- **Prese Jack I/O:**
  - `trigger:in`: Trigger nota melodica (ad es. pinch dita o sequenza).
  - `audio:in`: Ingresso audio per eventuale FM/modulazione esterna.
  - `audio:out`: Uscita stereo.
  - `param:cutoff`, `param:decay`: Modulazione continua CV (es. Asse Y della mano).
- **Riferimento Bibliografico Accademico:**
  - **Vadim Zavalishin ("The Art of VA Filter Design", Cap. 4 - SVF 12dB/oct)**: Risoluzione algebrica del filtro di stato variabile a retroazione istantanea senza ritardo numerico.
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++")**: Generatore di forma d'onda PolyBLEP (anti-aliased saw/pulse/tri) e generatore ADSR analog-modeled esponenziale.
  - **Émilie Gillet / Mutable Instruments Plaits (`virtual_analog_engine.cc`)**: Oscillatore morphing a transizione fluida tra sega, quadra e impulso variabile.

---

### 1.3 Sub Bass & 808 Kick (`ins-sub-808`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` e `AudioEngine.h`)
- **Categoria Interna:** Bassi & Sub
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia)
- **Manopole Frontali Macro:** `Volume` (0.0 - 1.5), `Cutoff` (40 - 500 Hz), `Decay` (0.2 - 4.0 s)
- **Prese Jack I/O:** `trigger:in`, `audio:out`, `param:cutoff`, `param:decay`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 4: Nonlinear Processing)**: Saturazione dolce con funzione $\tanh(x)$ per generare seconde e terze armoniche udibili anche su speaker di piccole dimensioni.
  - **Will C. Pirkle**: Inviluppo di pitch esponenziale rapido a frequenza ultra-bassa (35-65 Hz).

---

### 1.4 Theremin Nuvola 3D (`ins-theremin-3d`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Sintesi Continua Gestuale
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCyan` (Ciano Pastello)
- **Manopole Frontali Macro:** `Glide` (0 - 500 ms), `Brilliance` (0 - 100%), `Vibrato` (0 - 100%)
- **Prese Jack I/O:**
  - `mod:x`: Modulazione continua frequenza / intonazione (da mano destra / Asse X).
  - `mod:y`: Modulazione timbro / cutoff (da altezza mano / Asse Y).
  - `mod:z`: Modulazione ampiezza / volume (distanza mano dalla camera / Asse Z).
  - `audio:out`: Uscita audio stereo.
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 3: Modulators)**: Oscillatore eterodina a battimento con modulazione continua della frequenza e vibrato sinusoidale naturale a 5-6 Hz.
  - **Will C. Pirkle**: Filtro one-pole low-pass per lo smoothing del portamento ad alta risoluzione (eliminazione dei gradini di frequenza dai frame video a 60 FPS).
  - **Émilie Gillet / Plaits**: Interpolazione non-lineare tra intonazione continua e griglia di snap tonale.

---

### 1.5 Risonatore Modale a Corde & Barre (`ins-modal-resonator`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Modelli Fisici & Risonatori
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Manopole Frontali Macro:** `Pitch/Accordo`, `Materiale` (Legno/Metallo/Nylon), `Brillantezza`, `Smorzamento`
- **Prese Jack I/O:**
  - `audio:in`: Segnale eccitatore esterno (rumore, voce, colpo di microfono).
  - `trigger:in`: Impulso percussivo (martelletto virtuale).
  - `audio:out`: Uscita audio stereo risonante.
  - `param:material`, `param:pitch`: Modulazione CV.
- **Riferimento Bibliografico Accademico:**
  - **Julius O. Smith III ("Physical Audio Signal Processing", Section: Modal Synthesis & Waveguides)**: Banco di filtri passa-banda risonanti del 2° ordine accordati sui modi vibrazionali di una corda o barra 1D/2D, con matrice di dissipazione dell'energia per frequenza.
  - **Émilie Gillet / Mutable Instruments Elements & Rings (`resonator.cc`, `part.cc`)**: Implementazione C++ ad altissima efficienza di un risonatore modale a 24-64 modi risonanti simultanei.

---

### 1.6 Pad Granulare Gessetto (`ins-granular-pad`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Granulare & Atmosfere
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonSage` (Salvia Pastello)
- **Manopole Frontali Macro:** `Grana/Dimensione` (10 - 500 ms), `Densità` (1 - 40 grani/s), `Dispersione/Jitter`, `Pitch Shift` (-24 .. +24 st)
- **Prese Jack I/O:**
  - `audio:in`: Segnale live da campionare e congelare nel buffer circolare.
  - `audio:out`: Flusso polifonico granulare stereo.
  - `param:pos`, `param:density`: Modulazione scansione temporale buffer.
- **Riferimento Bibliografico Accademico:**
  - **Émilie Gillet / Mutable Instruments Clouds (`granular_processor.cc`)**: Generatore di grani a finestra hann/blackman con jitter stocastico sulla posizione di lettura e interpolazione allpass per pitch-shifting.
  - **Udo Zölzer ("DAFX", Cap. 8: Spectral Processing & Granular Systems)**.

---

### 1.7 Campionatore Acustico SFZ Multi-Velocity (`ins-sfz-player`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Tastiere & Banchi Campionati
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonLilac` (Lilla Pastello)
- **Manopole Frontali Macro:** `Volume`, `Dinamica/Velocity Curve`, `Tono/Filtro`, `Release`
- **Prese Jack I/O:** `trigger:in` (con velocity dinamica), `audio:out`, `param:tone`
- **Riferimento Bibliografico Accademico:**
  - **Libreria C++ `sfizz-core` (Standard SFZ v2)**: Caricatore di strumenti campionati multi-layer (Pianobook, banchi acustici di batteria, chitarre e fiati).
  - **Will C. Pirkle**: Gestione polifonica a furto di voci (Voice Stealing FIFO) e interpolazione percussiva a curve di risposta dinamica personalizzate.

---

## 2. Filtri & Modellazione Spettrale (Filters & Tone Shaping)

### 2.1 Filtro Naif ZDF SVF (`fx-filtro-svf`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::SVF_ZDF`)
- **Categoria Interna:** Filtri Analog-Modeled
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCyan` (Ciano Pastello)
- **Manopole Frontali Macro:** `Cutoff` (20 - 20000 Hz), `Risonanza` (0.1 - 10.0), `Mix Dry/Wet` (0 - 100%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:cutoff`, `param:res`
- **Riferimento Bibliografico Accademico:**
  - **Vadim Zavalishin ("The Art of VA Filter Design", Cap. 4: The State Variable Filter)**: Topologia TPT (Topology-Preserving Transform) bilineare con risoluzione algebrica del loop di retroazione senza ritardo $z^{-1}$. Nessun de-tuning vicino alla frequenza di Nyquist. Uscite Lowpass, Bandpass, Highpass disponibili simultaneamente.

---

### 2.2 Filtro Moog Ladder 24dB a 4 Poli (`fx-moog-ladder`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Filtri Vintage Analog
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonOchre` (Giallo Ocra)
- **Manopole Frontali Macro:** `Cutoff` (20 - 18000 Hz), `Risonanza` (0.0 - 4.0 auto-oscillazione), `Drive/Calore` (0 - 24 dB)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:cutoff`, `param:res`, `param:drive`
- **Riferimento Bibliografico Accademico:**
  - **Vadim Zavalishin ("The Art of VA Filter Design", Cap. 5: The Ladder Filter)**: Cascata di 4 integratori TPT del 1° ordine con loop di retroazione globale non-lineare ($\tanh$). All'aumentare della risonanza, la saturazione interna impedisce al filtro di esplodere e comprime armonicamente le basse frequenze con il classico suono "grasso" dei sintetizzatori Moog.
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++")**: Metodo del solutore non-lineare di Newton-Raphson a 2 iterazioni o approssimazione Pade per calcolo ultra-rapido a zero latenza.

---

### 2.3 Filtro Formantico Vocale (`fx-vowel-formant`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Filtri Speciali & Talkbox
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Manopole Frontali Macro:** `Vocale` (Morphing continuo A - E - I - O - U), `Intensità Formanti`, `Risonanza Q`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:vowel`, `param:res`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 6: Time-Varying Filters - Formant Filters)**: 3-5 filtri biquad risonanti in parallelo posizionati sulle frequenze formanti umane ($F_1, F_2, F_3$) con interpolazione logaritmica continua tra le vocali.
  - **Émilie Gillet / Plaits (`formant_oscillator.cc`)**: Tabelle di formanti vocali maschili e femminili ottimizzate a basso costo computazionale.

---

### 2.4 Equalizzatore Parametrico Studio a 3 Bande (`fx-eq-3band`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Equalizzatori
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonSage` (Salvia Pastello)
- **Manopole Frontali Macro:** `Bassi/Low Shelf` (+/- 15 dB a 100 Hz), `Medi/Mid Peak` (+/- 15 dB, Freq 200-5000 Hz), `Alti/High Shelf` (+/- 15 dB a 8 kHz)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:mid_gain`, `param:mid_freq`
- **Riferimento Bibliografico Accademico:**
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++", Cap. 5: Audio Filter Designs)**: Implementazione biquad conforme alle formule RBJ Audio EQ Cookbook (Robert Bristow-Johnson) con coefficienti a fase minima e ricalcolo differenziale per evitare discontinuità audio durante il movimento delle manopole.

---

### 2.5 Phaser Polifasico Analogico a 6 Stadi (`fx-phaser-analog`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Modulazione di Fase
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Manopole Frontali Macro:** `Sweep Rate` (0.05 - 8.0 Hz), `Profondità Sweep`, `Feedback/Risonanza`, `Mix`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:rate`, `param:feedback`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 6: Phasers and Allpass Chains)**: Cascata in serie di 6 celle allpass del primo ordine $A(z) = \frac{a + z^{-1}}{1 + a z^{-1}}$ con sweep sinusoidale simultaneo del polo e re-immissione di retroazione nel loop per produrre 3 notch mobili nello spettro.

---

## 3. Spazio & Tempo (Time & Space Effects)

### 3.1 Eco a Nastro Vintage (`fx-eco-nastro`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::StereoTapeDelay`)
- **Categoria Interna:** Delay & Eco
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Manopole Frontali Macro:** `Tempo Delay` (20 - 1200 ms), `Feedback` (0 - 95%), `Tono/Filtro Nastro` (400 - 12000 Hz), `Mix Dry/Wet` (0 - 100%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:time`, `param:feedback`, `param:tone`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 2: Delay Effects - Tape Saturation & Wow/Flutter)**: Linea di ritardo frazionaria con interpolazione lineare o allpass per pitch-shifting durante il cambio di tempo; filtro passa-basso analogico nel loop di feedback e saturazione morbida $\tanh(x)$ per simulare la compressione magnetica del nastro ad ogni ripetizione.

---

### 3.2 Riverbero Cattedrale FDN (`fx-riverbero-fdn`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::LushReverb`)
- **Categoria Interna:** Riverberi Algoritmici
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonLilac` (Lilla Pastello)
- **Manopole Frontali Macro:** `Stanza/Size` (0 - 100%), `Smorzamento HF` (0 - 100%), `Mix Dry/Wet` (0 - 100%)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:room`, `param:damp`, `param:mix`
- **Riferimento Bibliografico Accademico:**
  - **Julius O. Smith III ("Physical Audio Signal Processing", Section: Feedback Delay Networks - FDN)**: Matrice ortogonale a $N$ linee di ritardo mutualmente prime per scongiurare risonanze metalliche, con filtri di assorbimento passa-basso dipendenti dalla frequenza per modellare l'aria e i materiali delle pareti secondo la formula di Sabine per il tempo $T_{60}$.
  - **Udo Zölzer ("DAFX", Cap. 7: Artificial Reverberation)**.

---

### 3.3 Riverbero a Nuvola Diffusa & Shimmer (`fx-cloud-reverb`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Riverberi Eterei & Shimmer
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCyan` (Ciano Pastello)
- **Manopole Frontali Macro:** `Dimensione` (0 - 100%), `Diffusione`, `Shimmer/Ottava` (0 - 100%), `Mix Dry/Wet`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:size`, `param:shimmer`
- **Riferimento Bibliografico Accademico:**
  - **Émilie Gillet / Mutable Instruments Clouds (`reverb.cc`)**: Rete di diffusori allpass ad alta densità con modulazione casuale dei tempi di ritardo per creare code eteree prive di eco distinti.
  - **Julius O. Smith III**: Algoritmo Shimmer con inserimento di un pitch-shifter a +12 semitoni all'interno del loop di feedback del riverbero, generando un tappeto armonico ascendente.

---

### 3.4 Chorus Stereo BBD Analogico (`fx-chorus-bbd`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Modulazione Spaziale
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Manopole Frontali Macro:** `Velocità/Rate` (0.1 - 6.0 Hz), `Profondità/Depth` (0 - 100%), `Spazialità Stereo` (0 - 180° sfasamento), `Mix`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:rate`, `param:depth`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 2: Flanging, Chorus, Slapback)**: Modulazione ciclica del tempo di ritardo di 15-35 ms con due LFO sinusoidali sfasati di 90° tra canale sinistro e destro per allargamento dell'immagine stereo senza cancellazioni di fase in mono.
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++", Cap. 6)**.

---

### 3.5 Simulatore Leslie Speaker Rotante (`fx-leslie-rotary`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Altoparlante Rotante & Doppler
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonOchre` (Giallo Ocra)
- **Manopole Frontali Macro:** `Velocità` (Lento / Veloce), `Accelerazione`, `Profondità Doppler`, `Drive Valvolare`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:speed`, `param:drive`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 3: Modulators and Rotary Speakers)**: Crossover a 800 Hz che divide il segnale tra tromba degli acuti (alta inerzia, rotazione rapida) e tamburo dei bassi (rotazione lenta). Modulazione combinata di ampiezza (AM) e frequenza/ritardo Doppler (FM) con curve polari di irradiazione cardioide.

---

## 4. Dinamica, Saturazione & Distorsione (Dynamics & Color)

### 4.1 Saturazione Morbida Pastello (`fx-saturazione-soft`)
- **Stato:** `[X] Implementato` (in `mimo_hub_juce/Source/Audio/AudioDSP.h` come `dsp::softClipTanh` e nel catalogo effetti)
- **Categoria Interna:** Saturazione & Calore
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonPeach` (Pesca Pastello)
- **Manopole Frontali Macro:** `Drive` (0 - 36 dB), `Tono` (500 - 16000 Hz), `Curva/Forma` (Morbida / Calda), `Mix`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:drive`, `param:tone`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 4: Nonlinear Processing - Polynomial and Hyperbolic Waveshaping)**: Mappatura non-lineare $y = \tanh(x \cdot \text{gain})$ per compressione progressiva dei picchi e generazione di armoniche dispari naturali, seguita da un filtro di de-enfasi analogico per addolcire le alte frequenze.

---

### 4.2 Wavefolder Analogico West-Coast (`fx-wavefolder`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Sintesi Timbrica Buchla / West-Coast
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia)
- **Manopole Frontali Macro:** `Fold` (Quantità pieghe 0 - 100%), `Simmetria/Bias` (-100% .. +100%), `Timbro`
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:fold`, `param:bias`
- **Riferimento Bibliografico Accademico:**
  - **Émilie Gillet / Mutable Instruments Plaits & Kinks (`wavefolder.cc`)**: Algoritmo a ripiegamento progressivo dell'onda sinusoidale o triangolare quando il segnale supera le soglie di clipping, creando ricchezza armonica metallica senza generare rumore sgradevole.
  - **Udo Zölzer ("DAFX", Cap. 4)**.

---

### 4.3 Compressore VCA Studio con Sidechain (`fx-compressor-vca`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Dinamica di Studio
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonSage` (Salvia Pastello)
- **Manopole Frontali Macro:** `Threshold` (-40 .. 0 dB), `Ratio` (1:1 .. 20:1), `Attacco` (0.5 .. 80 ms), `Rilascio` (10 .. 1000 ms)
- **Prese Jack I/O:**
  - `audio:in`: Segnale audio principale.
  - `trigger:in / audio:sidechain`: Ingresso sidechain (ad es. cassa per effetto pumping sul synth).
  - `audio:out`: Uscita audio compressa.
  - `param:threshold`: Modulazione CV soglia.
- **Riferimento Bibliografico Accademico:**
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++", Cap. 9: Audio Compressors)**: Rilevatore di picco/RMS disaccoppiato con ricalcolo del guadagno in scala logaritmica (dB) ed envelope follower con attacco e rilascio a filtri one-pole separati.
  - **Udo Zölzer ("DAFX", Cap. 5: Dynamics Processing)**.

---

### 4.4 Shaper dei Transienti per Percussioni (`fx-transient-shaper`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Dinamica Percussiva
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia)
- **Manopole Frontali Macro:** `Punch/Attacco` (-12 .. +12 dB), `Coda/Sustain` (-12 .. +12 dB), `Clip Protect` (ON/OFF)
- **Prese Jack I/O:** `audio:in`, `audio:out`, `param:attack`, `param:sustain`
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 5: Differential Envelope Followers)**: Calcolo parallelo di due envelope detector con tempi di salita differenti (un follower ultra-rapido a 1 ms per il fronte del transiente e uno più lento a 20 ms per il corpo). La differenza tra i due segnali isola l'attacco del colpo permettendo di amplificarlo o attenuarlo indipendentemente dalla soglia assoluta.

---

## 5. Modulatori & Control Voltage (Modulation & CV)

### 5.1 LFO Polimorfico Sincronizzato (`mod-lfo-poly`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Generatori di Modulazione
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonLilac` (Lilla Pastello)
- **Manopole Frontali Macro:** `Velocità/Rate` (0.05 - 20.0 Hz o sincronizzato in battiti), `Forma` (Seno, Triangolo, Rampa, Quadra, Sample&Hold), `Ampiezza` (0 - 100%)
- **Prese Jack I/O:**
  - `trigger:in`: Reset della fase dell'oscillatore al colpo.
  - `mod:out`: Tensione di controllo normalizzata (0.0 .. 1.0 o -1.0 .. +1.0) per modulare qualsiasi manopola del canvas.
- **Riferimento Bibliografico Accademico:**
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++", Cap. 7)**: Oscillatore a basso consumo con accumulatore di fase a 32-bit e generatore pseudo-casuale Linear Congruential Generator (LCG) per Sample & Hold analog-style.

---

### 5.2 Seguitore d'Inviluppo Audio (`mod-env-follower`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Audio-to-CV Tracker
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonOchre` (Giallo Ocra)
- **Manopole Frontali Macro:** `Attacco` (1 - 50 ms), `Rilascio` (10 - 500 ms), `Sensibilità`, `Inversione` (Norm/Inv)
- **Prese Jack I/O:**
  - `audio:in`: Segnale audio sorgente (es. microfono piezo della batteria o voce).
  - `mod:out`: Segnale CV continuo (0.0 .. 1.0) che traccia l'intensità acustica per aprire filtri o variare delay in sincronia con i colpi.
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 5: Envelope Extractors)**: Raddrizzatore a semionda o onda intera seguito da filtro IIR passa-basso con coefficienti distinti di salita e discesa.

---

## 6. Utility Musicali & Routing Intelligente (Smart Utilities)

### 6.1 Smart Scale Lock (Quantizzatore Tonale Armonico) (`ut-scale-lock`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Assistente Musicale Intelligente
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonMint` (Menta Pastello)
- **Manopole Frontali Macro:** `Tonica` (Do, Do#, Re, ... Si), `Scala` (Pentatonica, Minore, Maggiore, Blues, Dorica, Insen), `Ottave` (1 - 4)
- **Prese Jack I/O:**
  - `mod:in`: Valore continuo $0.0 - 1.0$ (es. coordinata X dell'Occhio o sensore).
  - `mod:out`: Frequenza quantizzata in Hz (o nota MIDI) per agganciare note sempre armoniche al synth senza alcuna possibilità di stonare.
- **Riferimento Bibliografico Accademico:**
  - **Surge XT Synthesizer / Microtonal & Scale Engine**: Mappatura ad array di intervalli in semitoni normalizzati a 12-TET o scale etniche con isteresi per evitare oscillazioni rapide al confine tra due note.

---

### 6.2 Mixer Summing Stereo 4 Canali (`ut-mixer-4ch`)
- **Stato:** `[ ] Da Implementare (Pianificato FASE C)`
- **Categoria Interna:** Routing & Mix
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonSage` (Salvia Pastello)
- **Manopole Frontali Macro:** `Vol 1`, `Vol 2`, `Vol 3`, `Master`
- **Prese Jack I/O:**
  - `audio:in1`, `audio:in2`, `audio:in3`, `audio:in4`: 4 ingressi audio indipendenti.
  - `audio:out`: Uscita mixata con compensazione di guadagno pan-law a -3dB.
- **Riferimento Bibliografico Accademico:**
  - **Will C. Pirkle ("Designing Audio Effect Plugins in C++")**: Legge di panning sinusoidale a potenza costante per evitare rigonfiamenti di volume al centro.

---

### 6.3 1-Button Looper a Nastro (`ut-looper-tape`)
- **Stato:** `[X] Parzialmente Implementato` (Interfaccia Live attiva con barra di progresso, da integrare come nodo autonomo inseribile sul canvas)
- **Categoria Interna:** Performance & Loop
- **Colore Flat Pastello:** `MiMOLookAndFeel::crayonCherry` (Rosso Ciliegia)
- **Manopole Frontali Macro:** `Lunghezza Loop` (1, 2, 4, 8 battute o libero), `Overdub Feedback` (0 - 100%), `Usura/Degrado`, `Play/Stop`
- **Prese Jack I/O:**
  - `audio:in`: Segnale da registrare e mandare in loop.
  - `trigger:in`: Impulso per azionare REC / PLAY / OVERDUB (tasto spazio, pedale sustain o tasto hardware).
  - `audio:out`: Uscita audio del loop continuo.
- **Riferimento Bibliografico Accademico:**
  - **Udo Zölzer ("DAFX", Cap. 2: Sound-on-Sound Looping)**: Buffer circolare sincrono con dissolvenza incrociata (crossfade) a bordo loop per evitare click e feedback parziale per dissolvenza graduale stile Frippertronics.

---

## Riepilogo Statistiche di Copertura Moduli

| Categoria | Moduli Totali | Implementati | Da Implementare | Copertura |
| :--- | :---: | :---: | :---: | :---: |
| **1. Strumenti & Generatori** | 7 | 3 | 4 | **43%** |
| **2. Filtri & Timbro** | 5 | 1 | 4 | **20%** |
| **3. Spazio & Tempo** | 5 | 2 | 3 | **40%** |
| **4. Dinamica & Saturazione** | 4 | 1 | 3 | **25%** |
| **5. Modulatori CV** | 2 | 0 | 2 | **0%** |
| **6. Utility Musicali** | 3 | 1 | 2 | **33%** |
| **TOTALE ECOSISTEMA** | **26** | **8** | **18** | **31%** |
