#!/usr/bin/env python3
"""
MiMO - Test 01: Validazione Hardware Display CUQI 3.5" (SPI Framebuffer)
Modulo: @TESTER (Ingegnere Diagnostico)

Descrizione:
Script autonomo ed isolato per validare la connessione SPI a 26 pin
e il driver framebuffer (/dev/fb*), colorando lo schermo in sequenza:
ROSSO (2s) -> VERDE (2s) -> BLU (2s) -> NERO (Pulizia e uscita).

Compatibile con ambienti puramente headless (senza desktop X11/Wayland).
"""

import sys
import os
import time
import glob
import platform

# Costanti di default per display CUQI 3.5"
DEFAULT_FB = "/dev/fb1"
SCREEN_WIDTH = 480
SCREEN_HEIGHT = 320


def banner():
    print("=" * 60)
    print("   MiMO HARDWARE DIAGNOSTICS - TEST 01: DISPLAY CUQI (SPI)   ")
    print("=" * 60)


def find_available_framebuffers():
    """Restituisce la lista dei dispositivi framebuffer disponibili."""
    return sorted(glob.glob("/dev/fb*"))


def detect_framebuffer_info(fb_dev):
    """Rileva risoluzione e profondità di colore da sysfs se disponibile."""
    fb_name = os.path.basename(fb_dev)
    sysfs_path = f"/sys/class/graphics/{fb_name}"

    if not os.path.exists(sysfs_path):
        return None

    info = {}
    try:
        with open(f"{sysfs_path}/name", "r") as f:
            info["name"] = f.read().strip()
    except Exception:
        info["name"] = "Sconosciuto"

    try:
        with open(f"{sysfs_path}/virtual_size", "r") as f:
            w, h = f.read().strip().split(",")
            info["width"] = int(w)
            info["height"] = int(h)
    except Exception:
        info["width"] = SCREEN_WIDTH
        info["height"] = SCREEN_HEIGHT

    try:
        with open(f"{sysfs_path}/bits_per_pixel", "r") as f:
            info["bpp"] = int(f.read().strip())
    except Exception:
        info["bpp"] = 16

    return info


def init_pygame_headless(fb_dev):
    """Inizializza pygame forzando il driver video sul framebuffer specificato."""
    # Imposta la variabile d'ambiente per il device framebuffer
    os.environ["SDL_FBDEV"] = fb_dev

    # In ambiente headless (console pura), SDL può usare diversi driver a seconda della versione.
    # Proviamo nell'ordine: 'fbcon', 'directfb', 'kmsdrm', 'dummy'
    drivers_to_try = ["fbcon", "directfb", "kmsdrm", "dummy"]
    selected_driver = None

    # pyrefly: ignore[missing-import]
    import pygame

    for driver in drivers_to_try:
        os.environ["SDL_VIDEODRIVER"] = driver
        try:
            pygame.display.init()
            selected_driver = driver
            break
        except pygame.error:
            continue

    if not selected_driver:
        # Fallback estremo: prova senza impostare forzatamente SDL_VIDEODRIVER
        if "SDL_VIDEODRIVER" in os.environ:
            del os.environ["SDL_VIDEODRIVER"]
        try:
            pygame.display.init()
            selected_driver = "default"
        except pygame.error as err:
            raise RuntimeError(f"Impossibile inizializzare alcun driver video SDL: {err}")

    return pygame, selected_driver


def run_desktop_simulation():
    """Modalità di sviluppo su PC (Windows/Mac/Linux Desktop): apre una finestra 480x320."""
    print("[*] Ambiente Desktop rilevato (Nessun /dev/fb* presente).")
    print("[*] Avvio in MODALITÀ SIMULATORE DESKTOP (Finestra 480x320)...")
    try:
        # pyrefly: ignore[missing-import]
        import pygame
        pygame.init()
        screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("MiMO CUQI 3.5\" Display Simulator (480x320)")

        test_sequence = [
            ("ROSSO (Red)", (255, 0, 0), 2.0),
            ("VERDE (Green)", (0, 255, 0), 2.0),
            ("BLU (Blue)", (0, 0, 255), 2.0),
        ]

        print("\n[*] Avvio ciclo diagnostico RGB...")
        for name, color, duration in test_sequence:
            print(f"    -> Visualizzazione: {name} per {duration}s")
            screen.fill(color)
            pygame.display.flip()
            t_end = time.time() + duration
            while time.time() < t_end:
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        pygame.quit()
                        return
                time.sleep(0.05)

        print("\n[*] Pulizia schermo simulato...")
        screen.fill((0, 0, 0))
        pygame.display.flip()
        time.sleep(0.5)
        pygame.quit()
        print("[OK] Test Simulatore completato con successo.")
    except Exception as e:
        print(f"[FAIL] Errore simulazione desktop: {e}")


def print_rockpi_troubleshooting():
    """Istruzioni pratiche per abilitare il display SPI sulla Rock Pi."""
    print("\n" + "=" * 60)
    print("   GUIDA CONFIGURAZIONE DISPLAY CUQI SU ROCK PI / ARMBIAN   ")
    print("=" * 60)
    print("Il kernel Linux della Rock Pi non ha trovato alcun /dev/fb*.")
    print("Significa che il driver SPI o l'overlay del display non è attivo.\n")
    print("1. Verifica se il bus SPI è abilitato:")
    print("   ls -l /dev/spi*")
    print("\n2. Abilita l'overlay hardware (Armbian):")
    print("   sudo armbian-config -> System -> Hardware -> seleziona 'spi' e il display")
    print("\n3. Oppure carica il modulo fbtft manualmente:")
    print("   sudo modprobe fbtft_device name=piscreen speed=32000000 rotate=90")
    print("\n4. Dopo aver caricato il modulo, controlla se compare il framebuffer:")
    print("   ls -l /dev/fb*")
    print("   (dovrebbe comparire /dev/fb0 o /dev/fb1)")
    print("=" * 60)


def main():
    banner()

    # Se l'utente ha passato un argomento esplicito da riga di comando
    if len(sys.argv) > 1:
        fb_dev = sys.argv[1]
    else:
        # Autodiscovery dei framebuffer esistenti
        available_fbs = find_available_framebuffers()
        if DEFAULT_FB in available_fbs:
            fb_dev = DEFAULT_FB
        elif available_fbs:
            fb_dev = available_fbs[0]
            print(f"[*] Rilevato automaticamente framebuffer alternativo: {fb_dev}")
        else:
            fb_dev = DEFAULT_FB

    # Se non siamo su Linux o nessun /dev/fb esiste
    if not os.path.exists(fb_dev):
        print(f"[!] Target Framebuffer: {fb_dev} (non esistente)")

        # Se siamo su Windows o macOS, offriamo la simulazione desktop
        if platform.system() != "Linux":
            run_desktop_simulation()
            return

        # Siamo su Linux (Rock Pi), ma il display non è configurato
        print(f"[FAIL] Nessun dispositivo framebuffer trovato (/dev/fb*)!")
        print_rockpi_troubleshooting()
        sys.exit(1)

    print(f"[*] Target Framebuffer: {fb_dev}")

    # Lettura info dal kernel
    fb_info = detect_framebuffer_info(fb_dev)
    if fb_info:
        print(f"    - Controller: {fb_info['name']}")
        print(f"    - Risoluzione rilevata: {fb_info['width']}x{fb_info['height']} ({fb_info['bpp']} bpp)")
        width = fb_info["width"]
        height = fb_info["height"]
    else:
        print(f"    - Info sysfs non disponibili, uso default: {SCREEN_WIDTH}x{SCREEN_HEIGHT}")
        width = SCREEN_WIDTH
        height = SCREEN_HEIGHT

    # 2. Inizializzazione Pygame Headless
    print(f"[*] Inizializzazione Pygame in modalità headless su {fb_dev}...")
    try:
        pygame, driver = init_pygame_headless(fb_dev)
        print(f"[OK] Pygame video backend attivo: '{driver}'")
    except Exception as e:
        print(f"[FAIL] Errore inizializzazione Pygame: {e}")
        print("[!] Suggerimento: esegui con permessi di root (sudo) per accedere a /dev/fb*.")
        sys.exit(1)

    try:
        # Nascondi il cursore del mouse
        pygame.mouse.set_visible(False)

        # Inizializza superficie display
        flags = pygame.FULLSCREEN
        screen = pygame.display.set_mode((width, height), flags)
        print(f"[OK] Surface creata con successo ({width}x{height})")

        # 3. Sequenza Diagnostica RGB
        test_sequence = [
            ("ROSSO (Red)", (255, 0, 0), 2.0),
            ("VERDE (Green)", (0, 255, 0), 2.0),
            ("BLU (Blue)", (0, 0, 255), 2.0),
        ]

        print("\n[*] Avvio ciclo diagnostico RGB...")
        for name, color, duration in test_sequence:
            print(f"    -> Visualizzazione: {name} per {duration}s")
            screen.fill(color)
            pygame.display.flip()
            time.sleep(duration)

        # 4. Pulizia Finale dello Schermo
        print("\n[*] Pulizia dello schermo (Schermata Nera)...")
        screen.fill((0, 0, 0))
        pygame.display.flip()
        time.sleep(0.5)

        print("[OK] Test completato con successo! Schermo ripristinato.")

    except KeyboardInterrupt:
        print("\n[!] Interruzione manuale da utente (Ctrl+C). Pulizia in corso...")
        if 'screen' in locals():
            screen.fill((0, 0, 0))
            pygame.display.flip()
    except Exception as e:
        print(f"\n[FAIL] Errore imprevisto durante il rendering: {e}")
    finally:
        pygame.display.quit()
        pygame.quit()
        print("=" * 60)
        print("   FINE DIAGNOSTICA TEST 01   ")
        print("=" * 60)


if __name__ == "__main__":
    main()
