# MiMO Base Centrale - Registro Ufficiale Cablaggi e Connessioni Elettriche
**File:** `Hardware/WIRING_PINOUT.md`  
**Target Hardware:** Radxa ROCK 4B+ (v1.73) - SoC Rockchip OP1 / RK3399  
**Obiettivo:** Documentare in modo univoco e dettagliato ogni singolo filo e connessione fisica per prevenire errori di cablaggio, inversioni di polarità o conflitti di pin.

---

## 1. Regole Elettriche Auree di Sicurezza (Zero-Damage Policy)
1. **Livelli Logici:** I GPIO standard della Rock Pi 4B+ operano a **3.0V** (Tolleranza massima assoluta: **3.14V**). **È vietato immettere 5V in qualsiasi pin GPIO**.
   * *Eccezione GPIO:* `GPIO3_C0` (**Pin 32**) opera a **3.3V**.
   * *Eccezione ADC:* `ADC_IN0` (**Pin 26**) supporta un massimo di **1.8V**.
2. **Resistenze di Pull-Up Hardware Integrate:** I pin fisici **3, 5, 27, 28, 29 e 31** hanno resistenze di pull-up hardware interne da **4.7kΩ collegate a 3.0V**.
3. **Encoder Passivi (PATIKIL EC11):** Non hanno bisogno di VCC/alimentazione attiva. Chiudono semplicemente i circuiti verso GND.
4. **Massa Comune (GND):** Tutte le masse dei componenti (display, encoder, pedali, schede I2C) devono essere collegate allo stesso potenziale di riferimento (pin GND della scheda: Pin 30, 34, 39).

---

## 2. Mappa Consolidata dell'Header 40-Pin della Rock Pi 4

| Pin Fisico | Funzione / Componente Assegnato | Stato | Note Elettriche e Kernel |
| :---: | :--- | :---: | :--- |
| **1 - 26** | **ZONA ROSSA: Display SPI CUQI 3.5"** | **CONSOLIDATO** | Socket 26 pin, bus `spi1`, driver `fb_ili9486` |
| **27** | **Bus I2C2 - SDA** (Multiplexer PCA9548) | **CONSOLIDATO** | Pull-up HW 4.7kΩ a 3V (`GPIO2_A0`, Kernel 64) |
| **28** | **Bus I2C2 - SCL** (Multiplexer PCA9548) | **CONSOLIDATO** | Pull-up HW 4.7kΩ a 3V (`GPIO2_A1`, Kernel 65) |
| **29** | **Encoder 1 (Workspace) - CLK (Fase A)** | **CONSOLIDATO** | Pull-up HW 4.7kΩ (`GPIO2_B2`, Kernel 74) |
| **30** | **GND (Massa)** | **CONSOLIDATO** | Potenziale di riferimento 0V |
| **31** | **Encoder 1 (Workspace) - DT (Fase B)** | **CONSOLIDATO** | Pull-up HW 4.7kΩ (`GPIO2_B1`, Kernel 73) |
| **32** | **Encoder 2 (Effetti) - CLK (Fase A)** | **CONSOLIDATO** | Tolleranza 3.3V (`GPIO3_C0`, Kernel 112) |
| **33** | **Encoder 1 (Workspace) - SW (Tasto)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO2_B4`, Kernel 76) |
| **34** | **GND (Massa comune Encoder)** | **CONSOLIDATO** | Collegato ai piedini C e S2 degli encoder |
| **35** | **Encoder 3 (Strumenti) - CLK (Fase A)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO4_A5`, Kernel 133) |
| **36** | **Encoder 2 (Effetti) - DT (Fase B)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO4_A4`, Kernel 132) |
| **37** | **Encoder 3 (Strumenti) - DT (Fase B)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO4_D6`, Kernel 158) |
| **38** | **Encoder 2 (Effetti) - SW (Tasto)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO4_A6`, Kernel 134) |
| **39** | **GND (Massa)** | **CONSOLIDATO** | Potenziale di riferimento 0V |
| **40** | **Encoder 3 (Strumenti) - SW (Tasto)** | **CONSOLIDATO** | Linea GPIO standard 3.0V (`GPIO4_A7`, Kernel 135) |

---

## 3. Dettaglio Connessioni Consolidate

### A. Display SPI CUQI 3.5" (Pin 1 - 26: ZONA ROSSA)
* **Tipo Connessione:** Connettore a pettine femmina a 26 pin inserito direttamente sui Pin 1-26 dell'header della scheda (driver `fb_ili9486` su `/dev/fb1`).
* **Pin Chiave:**
  * **Pin 18 (RST):** Hardware Reset -> `GPIO4_D4` (Kernel **156**, Active-LOW).
  * **Pin 19 (MOSI):** Master Out Slave In -> `GPIO1_B0` (Kernel **40**).
  * **Pin 21 (MISO):** Master In Slave Out -> `GPIO1_A7` (Kernel **39**).
  * **Pin 22 (DC):** Data/Command -> `GPIO4_D5` (Kernel **157**).
  * **Pin 23 (CLK / SCLK):** SPI Clock -> `GPIO1_B1` (Kernel **41**).
  * **Pin 24 (CS / CS0):** Chip Select -> `GPIO1_B2` (Kernel **42**).

---

### B. Mappatura dei 3 Encoder Meccanici PATIKIL EC11

```text
               [ MANOPOLA PATIKIL EC11 ]
                    +--------------+
                    |    CORPO     |
                    +--------------+
                   /       |        \
                [ A ]    [ C ]    [ B ]   <-- Lato a 3 PIN (Rotazione)

                [ S1 ]            [ S2 ]  <-- Lato a 2 PIN (Pulsante)
```

#### 1. Encoder 1 (Workspace - Selezione Protetto):
* **Piedino A (CLK):** Pin Fisico **29** (`GPIO2_B2`, Kernel 74)
* **Piedino B (DT):** Pin Fisico **31** (`GPIO2_B1`, Kernel 73)
* **Piedino S1 (SW):** Pin Fisico **33** (`GPIO2_B4`, Kernel 76)
* **Piedini C & S2 (GND):** Pin Fisico **30** o **34** (Massa)

#### 2. Encoder 2 (Effetti):
* **Piedino A (CLK):** Pin Fisico **32** (`GPIO3_C0`, Kernel 112)
* **Piedino B (DT):** Pin Fisico **36** (`GPIO4_A4`, Kernel 132)
* **Piedino S1 (SW):** Pin Fisico **38** (`GPIO4_A6`, Kernel 134)
* **Piedini C & S2 (GND):** Pin Fisico **34** o **39** (Massa)

#### 3. Encoder 3 (Strumenti):
* **Piedino A (CLK):** Pin Fisico **35** (`GPIO4_A5`, Kernel 133)
* **Piedino B (DT):** Pin Fisico **37** (`GPIO4_D6`, Kernel 158)
* **Piedino S1 (SW):** Pin Fisico **40** (`GPIO4_A7`, Kernel 135)
* **Piedini C & S2 (GND):** Pin Fisico **34** o **39** (Massa)

---

## 4. Dettaglio Bus I2C Principale (Espansioni & Moduli)
* **SDA:** Pin Fisico **27** (`GPIO2_A0`, Kernel 64).
* **SCL:** Pin Fisico **28** (`GPIO2_A1`, Kernel 65).
* **Destinazione:** Multiplexer **Adafruit TCA9548A / PCA9548** ad 8 canali (Indirizzo `0x70` a 3.0V).
  * **Canale 0:** I/O Expander **MCP23017** per i 5 pulsanti fisici della scocca.
  * **Canali 1 - 7:** Connettori esterni STEMMA QT / Qwiic ("MiMO Link") con monitoraggio daemon diagnostico asincrono (ping ogni 2 secondi, dizionario RAM `CONNESSO`, `VUOTO`, `ERRORE`).
