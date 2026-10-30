# Radxa ROCK 4B+ (v1.73) - Hardware & Pinout Datasheet
**Target Agent:** `@HARDWARE` (Embedded Electronics Engineer)

This document contains the official specifications and physical constraints for the Base Central SBC of the MiMO project, based on the Rockchip OP1 SoC.

## 1. Core Electrical Specifications
*   **Power Input:** 
    *   USB Type-C (PD 2.0 or QC 3.0/2.0): 9V/2A or 12V/1.5A-2A.
    *   Fixed voltage: 6V to 12V on USB-C, or 5V applied directly to GPIO Pins 2 & 4.
*   **Operating Temperature:** 0°C to 50°C. *Note: SoC throttles at 80°C internal temp. Active cooling (fan/heatsink) is recommended for sustained DSP workloads.*
*   **Storage Expansion:** M.2 M-Key connector supports PCIe 2.1 NVMe SSDs only (SATA is NOT supported).

## 2. GPIO & Logic Level Constraints (CRITICAL)
Unlike the Raspberry Pi which is strictly 3.3V, the ROCK 4B+ has specific logic level tolerances. Wiring components with incorrect logic levels will damage the SoC.

*   **Standard GPIO Logic Level:** **3.0V** (Tolerance: Max 3.14V)
*   **GPIO3_C0 Logic Level:** **3.3V** (Tolerance: Max 3.465V)
*   **ADC_IN0 Logic Level:** **1.8V** (Tolerance: Max 1.98V)
*   **Hardware Pull-ups:** Pins **3, 5, 27, 28, 29, and 31** have an internal 4.7kΩ pull-up resistor tied to 3V. Keep this in mind when designing I2C buses or connecting default-low pushbuttons.

## 3. Peripheral Power Limits
*   **USB 3.0 (Upper Port):** Can provide up to 1.44A. Switchable to OTG mode via a physical slide switch on the underside of the board.
*   **USB 3.0 (Lower) + 2x USB 2.0:** Share a single regulator. The *cumulative* power output across these three ports combined cannot exceed 1.44A.

## 4. Complete Kernel Pinout Mapping
The "Numero Kernel" column is the absolute reference for `libgpiod` in Python. Do not use physical pin numbers for interrupt attachments.

| Pin Fisico | Nome GPIO | Numero Kernel | Funzione 1 | Funzione 2 | 
| ----- | ----- | ----- | ----- | ----- | 
| 1 | - | - | +3.3V | - | 
| 2 | - | - | +5.0V | - | 
| 3 | GPIO2_A7 | 71 | I2C7_SDA | - | 
| 4 | - | - | +5.0V | - | 
| 5 | GPIO2_B0 | 72 | I2C7_SCL | - | 
| 6 | - | - | GND | - | 
| 7 | GPIO2_B3 | 75 | SPI2_CLK | - | 
| 8 | GPIO4_C4 | 148 | DEBUG_TXD | - | 
| 9 | - | - | GND | - | 
| 10 | GPIO4_C3 | 147 | DEBUG_RXD | - | 
| 11 | GPIO4_C2 | 146 | PWM0 | - | 
| 12 | GPIO4_A3 | 131 | I2S1_SCLK | - | 
| 13 | GPIO4_C6 | 150 | PWM1 | - | 
| 14 | - | - | GND | - | 
| 15 | GPIO4_C5 | 149 | SPDIF_TX | - | 
| 16 | GPIO4_D2 | 154 | - | - | 
| 17 | - | - | +3.3V | - | 
| 18 | GPIO4_D4 | 156 | - | - | 
| 19 | GPIO1_B0 | 40 | SPI1_TXD | UART4_TXD | 
| 20 | - | - | GND | - | 
| 21 | GPIO1_A7 | 15 | SPI1_RXD | UART4_RXD | 
| 22 | GPIO4_D5 | 157 | - | - | 
| 23 | GPIO1_B1 | 41 | SPI1_CLK | - | 
| 24 | GPIO1_B2 | 42 | SPI1_CSn | - | 
| 25 | - | - | GND | - | 
| 26 | - | - | ADC_IN0 | - | 
| 27 | GPIO2_A0 | 64 | I2C2_SDA | - | 
| 28 | GPIO2_A1 | 65 | I2C2_CLK | - | 
| 29 | GPIO2_B2 | 74 | SPI2_TXD | I2C6_SCL | 
| 30 | - | - | GND | - | 
| 31 | GPIO2_B1 | 73 | SPI2_RXD | I2C6_SDA | 
| 32 | GPIO3_C0 | 112 | SPDIF_TX | UART3_CTSn | 
| 33 | GPIO2_B4 | 76 | SPI2_CSn | - | 
| 34 | - | - | GND | - | 
| 35 | GPIO4_A5 | 133 | I2S1_LRCK_TX | - | 
| 36 | GPIO4_A4 | 132 | I2S1_LRCK_RX | - | 
| 37 | GPIO4_D6 | 158 | - | - | 
| 38 | GPIO4_A6 | 134 | I2S1_SDI | - | 
| 39 | - | - | GND | - | 
| 40 | GPIO4_A7 | 135 | I2S1_SDO | - | 
