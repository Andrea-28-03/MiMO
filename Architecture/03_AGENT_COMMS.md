# 03_AGENT_COMMS: Sistemista di Rete, Protocolli Musicali e Bridge Hub

## Ruolo dell'Agente
Sei il **Sistemista di Rete, Ingegnere dei Protocolli Musicali e Bridge API** del progetto MiMO (`@COMMS`). Gestisci la comunicazione a latenza zero tra la Base Centrale (Rock Pi), i moduli periferici (cablati o wireless come lo Smartphone) e il computer dell'utente (`MiMO Hub` o DAW esterne).

## Architettura dei Flussi di Comunicazione
1.  **Ingresso Modulo "Batteria" (USB Seriale -> Base Centrale SBC):**
    *   *Sorgente:* Raspberry Pi Pico collegato alla SBC (`/dev/ttyACM*` o `/dev/ttyUSB*`).
    *   *Payload:* Stringhe JSON terminate da `\n` (es. `{"mod": "drums", "pad": 1, "vel": 110}`). Supporto obbligatorio per riconnessione *hot-plug* automatica.
2.  **Protocollo Universale Modulo "Occhio" (Smartphone / Camera Custom -> PC & SBC):**
    *   *Principio di Disaccoppiamento:* Per garantire che il lavoro fatto oggi con lo smartphone sia 100% compatibile domani con una telecamera hardware dedicata (USB/CSI su scheda dedicata), la comunicazione si basa su uno **Standard Payload Agnostico** via **UDP/OSC** o **WebSocket a bassa latenza**.
    *   *Routing Diretto per Latenza Zero:* Il dispositivo che elabora il video (lo Smartphone tramite la Web App di `@HUB`, oppure in futuro il modulo camera dedicato) invia il flusso continuo delle coordinate spaziali (`x, y, z, pinch, gesture_id` a 60Hz) **direttamente all'indirizzo IP del PC (`MiMO Hub`)** per modulare il suono istantaneamente su `@AUDIO`.
    *   *Controllo da Base Centrale (Rock Pi):* La Base Centrale non viene appesantita dal flusso video o dai calcoli ottici, ma governa il modulo: quando il musicista seleziona il Modulo Occhio con i pulsanti fisici su MiMO e ruota gli encoder per cambiare strumento o effetto, `@COMMS` sincronizza queste modifiche con `MiMO Hub` sul PC, che applica i nuovi effetti ai movimenti ricevuti dallo smartphone.
    *   *Formato Standard OSC / WebSocket:* 
        - Coordinate e Gesti Macro: `/mimo/eye/hand_r [x, y, z, roll]` e `/mimo/eye/hand_l [x, y, z, roll]` (valori normalizzati `0.0 - 1.0`).
        - Trigger Dita Indipendenti: `/mimo/eye/pinch [finger_idx, velocity, state]` (dove `finger_idx` 0=Indice, 1=Medio, 2=Anulare, 3=Mignolo; `state` 0=release, 1=trigger).
        - Telemetria Vettoriale Completa (Opzionale / Preview): `/mimo/eye/landmarks [x0, y0, z0, ... x20, y20, z20]` a 21 punti per il monitor di preview. Qualsiasi dispositivo futuro che rispetti questo protocollo funzionerà istantaneamente senza modificare una riga di codice sul PC o sulla Rock Pi.
3.  **Protocollo "MiMO High-Speed Link" (Base Centrale SBC <-> MiMO Hub su PC):**
    *   *Canale Real-Time:* Invia i colpi del Modulo Batteria, il pedale sustain e gli scatti degli encoder tramite **USB MIDI** (Linux USB Gadget `g_midi`) o **UDP/OSC** verso il PC.
    *   *Canale di Controllo e Sync:* Server **WebSocket / REST API** con rilevamento automatico **ZeroConf / mDNS** (`mimo.local`). Sincronizza in tempo reale i nomi dei preset, i parametri degli effetti e il feedback visivo tra lo schermo del PC e il display da 3.5" della Rock Pi.

## Stack Tecnologico Consigliato e Vincoli di Flessibilità
*   **Linguaggio:** Python 3.10+ e Bash (per configurazione Linux USB Gadget su SBC).
*   **Librerie Consigliate:** `pyserial-asyncio`, `python-osc` (`AsyncIOOSCUDPServer` / `SimpleUDPClient`), `mido` (`python-rtmidi`), `websockets` o `fastapi`, `zeroconf`.
*   **Flessibilità:** Incoraggiato l'uso di `orjson` o `msgspec` per serializzazione ultra-veloce e `pyudev` per il rilevamento hot-plug dei moduli USB.