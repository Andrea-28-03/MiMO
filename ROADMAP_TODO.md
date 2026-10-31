# MiMO Hub — Roadmap Operativa & Taskboard delle Funzionalità

> Questo file è il tracker ufficiale di avanzamento per tutte le migliorie di UX, Interfaccia, Routing e Sound Design di MiMO.
> Puoi aggiungere, modificare o riordinare le idee in questo file in qualsiasi momento. L'assistente leggerà questo file e implementerà i task fase per fase, spuntando i punti completati con `[x]`.

---

## 📋 Fase 1: Workflow & Interazione Canvas Core (UX Fondamentale)
*Obiettivo: Rendere il canvas agile, naturale e conforme agli standard professionali di disegno e montaggio.*

- [ ] **1.1. Selezione Multipla (Lasso / Marquee Selection)**
  - Cliccando e trascinando con il tasto sinistro su uno spazio vuoto del canvas compare un riquadro di selezione pastello.
  - Tutti i nodi intersecati dal riquadro vengono selezionati contemporaneamente (evidenziati con glow).
  - Supporto per `Shift + Click` per aggiungere/rimuovere singoli nodi alla selezione.
- [ ] **1.2. Copia & Incolla (CTRL+C / CTRL+V)**
  - Copiando più nodi selezionati vengono duplicati anche i cavi interni tra essi.
  - Incollando (`CTRL+V`), i nuovi elementi compaiono sfalsati di 30px rispetto all'originale per non sovrapporsi.
- [ ] **1.3. Sistema Undo / Redo (CTRL+Z / CTRL+SHIFT+Z)**
  - Storico cronologico atomico per annullare/ripristinare: aggiunta/cancellazione nodi, connessione/rimozione cavi, spostamenti di posizione e cambi parametri.
- [ ] **1.4. Navigazione Panning 2D a 360° per Touchpad & Mouse**
  - Rimozione del blocco/snapping sui due soli assi ortogonali (destra/sinistra e alto/basso).
  - Panning fluido bidirezionale che consente il movimento diagonale continuo con qualsiasi touchpad o trascinamento.
- [ ] **1.5. Zoom Diretto con Rotella del Mouse**
  - Lo zoom in/out si attiva direttamente ruotando la rotella del mouse senza dover premere alcun tasto modificatore.
- [ ] **1.6. Cursore a Manina su Spazio Vuoto**
  - Quando il puntatore del mouse si trova sul canvas libero (non sopra nodi, manopole o cavi), il cursore assume l'aspetto della manina di navigazione.
- [ ] **1.7. Occlusione Grafica Perfetta (Z-Index Opaque Cards)**
  - I nodi in primo piano coprono al 100% i nodi e i cavi sottostanti, eliminando trasparenze indesiderate e linee passanti.

---

## 🎨 Fase 2: Interfaccia, Menu Rapidi e Nomenclatura Professionale
*Obiettivo: Velocizzare la creazione musicale eliminando ogni conflitto grafico e gergo ambiguo.*

- [ ] **2.1. Quick-Add Menu con `Shift + A`**
  - Premendo `Shift + A` si apre un menù a comparsa direttamente alle coordinate attuali del mouse.
  - Cartelle chiuse ad albero (Strumenti, Effetti, Moduli, Utility); passando il mouse sopra una categoria (`hover`), questa si espande a cascata verso destra.
  - Cliccando sull'elemento desiderato, questo viene istanziato esattamente nella posizione del mouse.
- [ ] **2.2. Menu Contestuale con Tasto Destro del Mouse**
  - Menu dinamico e intelligente che si adatta all'elemento cliccato:
    - *Su Canvas libero:* Aggiungi, Incolla, Resetta Vista, Centra MiMO.
    - *Su Modulo Hardware:* Impostazioni canale, Assegna Strumento, Rimuovi.
    - *Su Strumento / Effetto:* Duplica, Bypassa, Rinomina, Salva come Preset, Elimina.
    - *Su Base MiMO:* Livello Master, Opzioni Routing, Pulisci Cavi Muti.
- [ ] **2.3. Sezione "Scorciatoie" nelle Impostazioni**
  - Pannello dedicato con l'elenco di tutte le shortcut da tastiera con possibilità di personalizzazione.
- [ ] **2.4. Eliminazione Conflitti Grafici & Accesso Sub-Canvas (Doppio Click / TAB)**
  - Rimozione del tasto "espandi" che si sovrapponeva al pulsante power/bypass.
  - L'ispezione della struttura interna del nodo avviene ora con **doppio click** o premendo il tasto **TAB** sul nodo selezionato.
  - Correzione della posizione dei jack inferiori per non sovrapporsi alle manopole del tempo/decay.
- [ ] **2.5. Correzione Caratteri e Codifica UTF-8**
  - Rimozione di emoji e caratteri speciali non supportati su alcuni schermi Windows, sostituiti con icone vettoriali disegnate via codice (`juce::Path`).
- [ ] **2.6. Nomenclatura Professionale della Libreria**
  - Revisione di tutti i nomi e sottotitoli del catalogo secondo gli standard del sound design e della produzione musicale (es. *Analog Ladder Filter*, *Tape Echo & Saturation*, *BBD Bucket-Brigade Chorus*, *Waveguide Acoustic Pluck*, *PolyBLEP Analog Synth*).

---

## ⚡ Fase 3: Smart Routing & Gestione Spaziale Cavi nel Canvas
*Obiettivo: Automatizzare le connessioni e l'organizzazione dello spazio, trasformando il canvas in un banco da lavoro intelligente.*

- [ ] **3.1. Auto-Drop Effetto su Cavo (Insert in Serie Automatico)**
  - Trascinando un effetto sopra un cavo audio esistente, il cavo si illumina con alone luminoso (glow pastello).
  - Al rilascio, il programma fa automaticamente spazio tra i due nodi e connette in serie l'effetto inserito.
- [ ] **3.2. Auto-Compattazione alla Rimozione (Ripple Delete)**
  - Eliminando un effetto posto tra due elementi, gli elementi adiacenti si riavvicinano automaticamente richiudendo lo spazio vuoto e ricollegando il cavo diretto.
- [ ] **3.3. Cavi Dinamici con Colore Timbrico dell'Effetto**
  - Il cavo in uscita da ciascun effetto assume il colore caratteristico dell'effetto stesso (es. Ciano per il Filtro, Pesca per il Chorus, Cherry per il Saturatore), visualizzando intuitivamente il percorso di trasformazione del suono.
- [ ] **3.4. Auto-Collegamento Diretto da Libreria**
  - Avendo uno strumento o un modulo selezionato, un semplice click su un effetto della libreria lo istanzia e lo connette automaticamente a valle.
- [ ] **3.5. Disposizione Automatica dello Strumento alla Destra del Modulo**
  - Quando si assegna uno strumento a un modulo hardware, lo strumento non scompare più "incorporato", ma si posiziona automaticamente alla destra del modulo facendogli spazio, mantenendo tutti i controlli visibili ed editabili.
- [ ] **3.6. Topologia Hub MiMO (Rock Pi)**
  - I moduli hardware convogliano i propri comandi verso la Base Centrale MiMO e verso gli strumenti, garantendo un albero di segnale pulito senza cavi diretti spuri al master bus.

---

## 🎙️ Fase 4: Moduli Hardware & Microfono Interattivo
*Obiettivo: Portare il controllo fisico ed espressivo al massimo livello di immediatezza.*

- [ ] **4.1. Modulo Microfono: Soglia Interattiva su Visualizer Live**
  - Rimozione dello slider separato.
  - L'utente imposta la soglia di trigger trascinando direttamente un marcatore/linea pastello sulla barra di visualizzazione del livello sonoro in tempo reale.
- [ ] **4.2. Modalità Continuous Retrigger / Clock**
  - Se il segnale microfonico rimane costantemente sopra la soglia impostata, il modulo genera trigger ritmici a cadenza musicale configurabile o emette un gate continuo.
- [ ] **4.3. Modulo Microfono: Rilevamento Pitch (Vocoder & Melodie con la Voce)**
  - Rimozione delle uscite inutilizzate e integrazione di un pitch detector monofonico (conversione intonazione vocale in CV/note MIDI) + Dynamic Envelope Follower.
- [ ] **4.4. Modulo Percussioni Piezo (da 1 a 5 sensori a contatto)**
  - Supporto indipendente per 1-5 ingressi piezoelettrici, ciascuno con sensibilità dedicata e assegnabile a uno strumento sonoro distinto.
- [ ] **4.5. Presa "Strumento" sui Moduli & Fan-Out Multi-Modulo**
  - Presa dedicata sul modulo in alto a destra (o una presa per mano sul modulo Camera in corrispondenza del relativo menu).
  - Possibilità per più moduli hardware di pilotare lo stesso strumento contemporaneamente.

---

## 🎹 Fase 5: Sound Design, Tape Looper & Macro Sub-Canvas
*Obiettivo: Strumenti sonori accattivanti, moduli componibili e animazioni fisiche entusiasmanti.*

- [ ] **5.1. Libreria Percussioni Granulare (Strumenti Singoli e Kit Completo)**
  - Oltre al kit 4-voci unificato, introduzione di moduli discreti dedicati: *Analog Kick 808*, *Punchy Snare*, *Metallic Hi-Hat*, *Warm Tom*, *Studio Cymbal*, ciascuno ottimizzato per il sound design puntuale.
- [ ] **5.2. Tape Looper Fisico & Animato (Reel-to-Reel)**
  - Grafica entusiasmante con due bobine a nastro che ruotano animate a diverse velocità.
  - Tasti fisici retrò (Rec, Play, Overdub, Reverse) con registrazione audio Sound-on-Sound reale, senza interruzioni.
- [ ] **5.3. Modulo "Composizione / Sequencer / Canzone"**
  - Modulo rinominabile e strutturabile per generare melodie, pattern e accordi collegando gli elementi di Utility & Controllo.
- [ ] **5.4. Sub-Canvas di Personalizzazione e Salvataggio Preset Custom**
  - Entrando in un effetto o strumento tramite doppio click o TAB, si accede a un canvas interno con blocchi elementari per costruire circuiti sonori personalizzati e salvarli nella libreria globale dell'utente.
