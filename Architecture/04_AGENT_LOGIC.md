# 04_AGENT_LOGIC: Architetto della Macchina a Stati e Workspace

## Ruolo dell'Agente
Sei l'**Architetto della Macchina a Stati (State Machine) e Controller Centrale** del progetto MiMO (`@LOGIC`). Operi come ponte logico tra gli eventi hardware (`@HARDWARE`), i flussi di comunicazione (`@COMMS`), il motore di rendering su display (`@GUI`) e l'applicazione desktop (`@HUB` / `@AUDIO`). Il tuo compito è mantenere lo stato deterministico del sistema, commutare i Workspace e gestire la reattività dei controlli fisici.

---

## Regola Universale "Press & Turn" per Modifica Valori e Navigazione

In qualsiasi schermata, sottomenu o lista di impostazioni della GUI, l'interazione con gli encoder rotativi obbedisce a una regola ergonomica universale valida in tutti i Workspace:
1.  **Rotazione Semplice (Turn):** Scorre tra le voci, gli elementi della lista o le impostazioni disponibili (spostamento del focus visivo / selezione).
2.  **Pressione e Rotazione Simultanea (Press & Turn):** Incrementa o decrementa direttamente il valore numerico o percentuale del parametro attualmente selezionato (ad es. `+1` / `-1` o `+5%` / `-5%`).
3.  **Click Singolo (Short Click < 300ms):** Seleziona un elemento, conferma una scelta o commuta uno stato ON/OFF (Bypass toggle).
4.  **Doppio Click Veloce (Double Click < 300ms):** Apre la schermata modale o il sottomenu con le impostazioni dettagliate dell'elemento selezionato.

---

## Comportamento dei 3 Encoder Fisici per Workspace

### 1. Encoder Workspace (Blocco di Sicurezza Anti-Errore)
*   *Rotazione a Vuoto:* **Completamente inattiva**. Ruotare accidentalmente l'encoder durante l'esecuzione musicale non produce alcun effetto, scongiurando cambi improvvisi di modalità sul palco.
*   *Sblocco e Cambio:* L'encoder deve essere **cliccato una volta** per sbloccare la selezione del Workspace; a questo punto la rotazione permette di scorrere e selezionare il Workspace desiderato (`STUDIO`, `LIVE` o profili `CUSTOM`).

---

### 2. Workspace `STUDIO` (Sound Design & Editing Approfondito)
Progettato per comporre, definire i suoni e creare le catene di elaborazione modulo per modulo:

*   **Pulsanti Fisici 1 - 5 (Tasti I2C su Scocca):**
    *   Fungono da **Selettori di Modulo Target**: la pressione del Tasto $N$ seleziona il modulo collegato alla Porta $N$ (es. Modulo Batteria su Porta 1, Modulo Occhio su Porta 2), aggiornando il contesto nella Top Bar e i dati nella Center Area.
*   **Encoder Strumenti (Encoder 3):**
    *   *Click Singolo:* Apre nella Center Area la lista degli strumenti/generatori disponibili per il modulo attivo (la Top Bar rimane invariata).
    *   *1 Click su uno strumento in lista:* Seleziona e attiva lo strumento.
    *   *Doppio Click veloce (<300ms) su uno strumento:* Apre il pannello con le impostazioni avanzate dello strumento (ADSR, Cutoff, Velocity Curve).
*   **Encoder Effetti (Encoder 2):**
    *   *Click Singolo:* Apre nella Center Area la lista degli effetti applicabili al modulo selezionato (la Top Bar rimane invariata).
    *   *1 Click su un effetto in lista:* Seleziona l'effetto.
    *   *Doppio Click veloce (<300ms) su un effetto:* Apre il pannello di modifica approfondita dei parametri dell'effetto.
*   **Salvataggio verso `LIVE`:**
    *   Dall'HUB desktop (`@HUB`) è possibile salvare l'intera configurazione attuale creata in Studio (strumenti selezionati + catena degli effetti con relativi valori) assegnandola direttamente a uno dei **5 pulsanti fisici** (Scene 1-5), pronta per essere richiamata istantaneamente dal vivo.

---

### 3. Workspace `LIVE` (Performance dal Vivo & Esecuzione Rapida)
Progettato per la massima reattività sul palco e la riduzione a zero delle distrazioni visive:

*   **Pulsanti Fisici 1 - 5 (Tasti I2C su Scocca):**
    *   Fungono da **Selettori di Scena (Preset Globali)**: la pressione del Tasto $N$ richiama in meno di 1 ms l'intera Scena $N$, riconfigurando contemporaneamente strumenti, effetti, volumi e routing di tutti i moduli connessi.
*   **Pedale Sustain (Jack 6.35mm):**
    *   Funge da **Scene Switcher Hands-Free**: a ogni pressione avanza alla Scena successiva (`Scene + 1`), consentendo al musicista di progredire nella scaletta o passare da Strofa a Ritornello senza togliere le mani dallo strumento o dai gesti.
*   **Encoder Strumenti (Encoder 3):**
    *   *Click Singolo:* Apre direttamente nella Center Area il pannello di modifica rapida delle impostazioni dello strumento selezionato.
    *   *Rotazione:* Scorre i parametri disponibili (es. Volume, Pitch, Decay).
    *   *Press & Turn:* Modifica all'istante il valore del parametro selezionato.
*   **Encoder Effetti (Encoder 2):**
    *   *Click Singolo:* Sblocca la lista degli effetti applicati nel template corrente.
    *   *1 Click su un effetto:* Attiva o disattiva l'effetto al volo (**Bypass Toggle** istantaneo).
    *   *Doppio Click veloce (<300ms) su un effetto:* Apre le impostazioni avanzate dell'effetto per regolazioni rapide durante il live.

---

## Tabella Sinottica delle Assegnazioni di Controllo

| Controllo Fisico | Modalità `STUDIO` (Sound Design) | Modalità `LIVE` (Performance) |
| :--- | :--- | :--- |
| **Encoder Workspace** | Rotazione bloccata. Click = sblocco selezione Workspace. | Rotazione bloccata. Click = sblocco selezione Workspace. |
| **Encoder Strumenti** | Click: Apre lista strumenti.<br>1 Click su strumento: Seleziona.<br>Doppio Click (<300ms): Impostazioni strumento. | Click: Apre modifica rapida impostazioni strumento.<br>Gira: Scorre parametri.<br>Press & Turn: Modifica valore. |
| **Encoder Effetti** | Click: Apre lista effetti.<br>1 Click su effetto: Seleziona.<br>Doppio Click (<300ms): Impostazioni effetto. | Click: Sblocca lista effetti template.<br>1 Click: Bypass ON/OFF.<br>Doppio Click (<300ms): Impostazioni avanzate. |
| **5 Pulsanti Fisici** | **Selezione Modulo Target (Porte 1 - 5):** Cambia il modulo visualizzato e modificabile. | **Richiamo Scena Istantaneo (Scene 1 - 5):** Carica preset globale di tutti i moduli in $<1\text{ms}$. |
| **Pedale Sustain** | Funzione ausiliaria (Tap tempo / Rec). | **Avanzamento Scena (`Scene + 1`):** Cambio Scena a mani libere. |

---

## Stack Tecnologico Consigliato e Vincoli di Flessibilità
*   **Linguaggio:** Python 3.10+ puro.
*   **Modello Architetturale:** State Pattern asincrono con `asyncio.Queue` per il disaccoppiamento dagli interrupt hardware.
*   **Isolamento:** Nessuna dipendenza hardware diretta (niente `gpiod` o `smbus2` qui) né grafica (niente `pygame-ce` qui): `@LOGIC` riceve eventi puri e notifica `@GUI` e `@COMMS`.
