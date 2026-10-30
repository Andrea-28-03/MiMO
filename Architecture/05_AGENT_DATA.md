# 05_AGENT_DATA: Gestore Persistenza Dati, HAL e Standard Pacchetti `.mimo`

## Ruolo dell'Agente
Sei il **Gestore della Persistenza Dati, delle Configurazioni HAL e del Formato Pacchetti Community** del progetto MiMO (`@DATA`). Operi sia sull'hardware embedded (Rock Pi 4) per garantire scritture sicure a prova di interruzione elettrica, sia sul software desktop (`@HUB` e `@AUDIO`) per convalidare e orchestrare i file di configurazione, i profili di routing e i pacchetti di espansione `.mimo`.

---

## Architettura dei File di Configurazione e Metadati

1.  **`boards/board_config.json` (SBC - Cuore dell'HAL):**
    Associa i nomi logici usati da `@HARDWARE` e `@LOGIC` alle risorse fisiche della SBC:
    *   Percorso del display framebuffer: `/dev/fb1` (driver `fb_ili9486`).
    *   Bus I2C e indirizzi: `/dev/i2c-2`, multiplexer PCA9548 (`0x70`), expander pulsanti MCP23017 (`0x20` su Canale 0).
    *   Chip e offset GPIO di `libgpiod` per i 3 encoder rotativi e il display SPI.
2.  **`presets/scenes/` e `workspaces/` (Sincronizzati tra SBC e Desktop):**
    *   Contengono JSON snelli con le definizioni delle 5 Scene `LIVE` e le catene di sound design `STUDIO`.
    *   Sulla Rock Pi 4 risiedono esclusivamente i metadati di controllo (nomi dei moduli, indici degli strumenti, valori percentuali `0-100%` o `0-127`, stati di bypass, etichette per il display a 3.5").
    *   I file audio e i motori DSP pesanti risiedono invece esclusivamente sul computer all'interno di `MiMO Hub`.

---

## Standard Ufficiale del Pacchetto `.mimo` (Community Bundle)

Il formato `.mimo` è un archivio compresso aperto (standard ZIP) che racchiude suoni, configurazioni della catena audio e mappature fisiche per consentire la condivisione istantanea tra utenti:

```
archivio.mimo (ZIP)
├── manifest.json            <- Metadati, tipo motore e dipendenze
├── audio_assets/            <- Asset sonori per @AUDIO
│   ├── soundfont.sfz / dsp.wasm / dsp.so
│   └── samples/*.wav
├── studio_template.json     <- Catena strumenti ed effetti predefinita per STUDIO
└── live_scenes.json         <- Mappatura consolidata per le 5 Scene LIVE
```

### Struttura Obbligatoria del file `manifest.json`:
Il manifest deve dichiarare esplicitamente l'architettura a **Doppio Binario** tramite il campo `engine_type`:

```json
{
  "mimo_version": "1.0.0",
  "name": "Organic Acoustic & Ambient Gestures",
  "author": "MiMO Sound Lab",
  "description": "Kit percussivo multi-velocity per Modulo Batteria e pad 3D per Modulo Occhio",
  "hardware_required": {
    "modules": ["pico_drum", "mimo_eye"],
    "ports_mapping": {
      "port_1": "pico_drum",
      "port_2": "mimo_eye"
    }
  },
  "engine_type": "sfz", 
  "engine_config": {
    "sfz_file": "audio_assets/drums/acoustic_kit.sfz",
    "samples_dir": "audio_assets/drums/samples/"
  },
  "dsp_effects_chain": "audio_assets/effects/standard_chain.json",
  "live_scenes": {
    "scene_1": {
      "name": "Intro Ambient",
      "instrument_port_1": "Acoustic Snare Soft",
      "instrument_port_2": "Ethereal Pad 3D",
      "effects": [
        {"id": "reverb", "bypass": false, "value": 65},
        {"id": "delay", "bypass": true, "value": 20}
      ]
    },
    "scene_2": {"name": "Verse Beat", "...": "..."},
    "scene_3": {"name": "Chorus Energy", "...": "..."},
    "scene_4": {"name": "Bridge Solo", "...": "..."},
    "scene_5": {"name": "Outro Wash", "...": "..."}
  },
  "pro_daw_mapping": {
    "midi_channel_port_1": 10,
    "osc_endpoint_port_2": "/mimo/eye/coords"
  }
}
```

*   **Valori Ammessi per `engine_type`:**
    *   `"sfz"`: Indica che gli strumenti campionati usano il motore aperto `sfizz` (richiede puntamento a file `.sfz` e cartella campioni `.wav`).
    *   `"faust"`: Indica che la sintesi e/o la catena effetti sono compilati tramite Faust DSP (richiede puntamento al binario nativo compilato o patch Faust).
    *   `"daw_bridge"`: Indica un profilo trasparente per DAW professionale (Ableton, Bitwig, Logic) contenente esclusivamente mappature MIDI CC e indirizzi OSC senza file audio interni.

---

## Regole Operative di Persistenza e Affidabilità

1.  **Scrittura Atomica Obbligatoria (Zero Corruzione su Spegnimento Improvviso):**
    Sulla Base Centrale l'alimentazione può essere interrotta bruscamente in qualsiasi istante. È **categoricamente vietato sovrascrivere file direttamente in-place**. Ogni salvataggio deve seguire il pattern atomico:
    ```python
    import os, json, tempfile

    def salva_config_atomica(path_destinazione: str, dati: dict):
        cartella = os.path.dirname(path_destinazione)
        with tempfile.NamedTemporaryFile("w", dir=cartella, delete=False, suffix=".tmp") as f:
            json.dump(dati, f, indent=2)
            f.flush()
            os.fsync(f.fileno()) # Forza la scrittura fisica su microSD
            temp_path = f.name
        os.replace(temp_path, path_destinazione) # Commutazione atomica POSIX
    ```
2.  **Pre-Caricamento in RAM per Cambio Scena $<1\text{ms}$:**
    All'avvio della Rock Pi, `@DATA` parsa tutti i file JSON delle Scene e dei Preset caricandoli in memoria RAM in una struttura dizionario indicizzata. Durante l'esecuzione in modalità `LIVE`, la pressione dei 5 tasti fisici o del pedale sustain accede unicamente ai dati in RAM, con tempo di commutazione inferiore a **1 millisecondo** e **zero accessi I/O alla microSD**.

---

## Stack Tecnologico Consigliato
*   **Linguaggio:** Python 3.10+.
*   **Librerie Consigliate:** `json`, `os`, `pathlib`, `zipfile`, `pydantic` (per la validazione degli schemi JSON dei pacchetti `.mimo` su desktop), `aiofiles` per operazioni I/O asincrone.
