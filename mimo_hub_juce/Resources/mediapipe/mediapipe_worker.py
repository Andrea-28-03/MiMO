"""
MiMO Eye - Worker Python Nativo per Google MediaPipe HandLandmarker (RunningMode.VIDEO)
Elabora i fotogrammi video continui a 30-60 FPS con XNNPACK CPU SIMD.
Traccia stabilmente le mani (Right / Left) con memoria spazio-temporale:
- Nel video specchiato selfie di JUCE, riconosce correttamente Mano Destra e Mano Sinistra dell'utente.
- Il movimento orizzontale della mano da destra a sinistra non inverte l'identità della mano.
- Riconoscimento anatomico 3D per rotazioni, pugni chiusi e flessione dita invariante per orientamento.
- Supporto dual-hand simultaneo con canali indipendenti per mano destra e mano sinistra.
"""
import sys
import time
import math
import io
import os
import traceback

# Sopprime i log rumorosi di TensorFlow e MediaPipe su stderr per non intasare le pipe su Windows
os.environ["GLOG_minloglevel"] = "3"
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"

import requests
import numpy as np
from PIL import Image
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

LOG_FILE = os.path.join(os.environ.get("LOCALAPPDATA", r"C:\Users\Utente\AppData\Local"), "MiMOHub", "worker.log")

def log(msg):
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"[{time.strftime('%H:%M:%S')}] {msg}\n")
    except Exception:
        pass

def dist3d(p1, p2):
    dx = p1.x - p2.x
    dy = p1.y - p2.y
    dz = (getattr(p1, 'z', 0.0) or 0.0) - (getattr(p2, 'z', 0.0) or 0.0)
    return math.sqrt(dx * dx + dy * dy + dz * dz)

class HandTrack:
    def __init__(self, side):
        self.side = side  # "Right" o "Left"
        self.active = False
        self.pos = (0.5, 0.5)
        self.lost_count = 0
        self.last_data = None

class HandTracker:
    def __init__(self):
        self.tracks = {
            "Right": HandTrack("Right"),
            "Left": HandTrack("Left")
        }

    def update(self, detected_hands):
        """
        Associa le mani rilevate nel fotogramma corrente ai track persistenti.
        Preserva l'identità della mano (Right o Left) lungo l'intera traiettoria,
        anche quando una mano destra si sposta sul lato sinistro dello schermo o viceversa.
        """
        if len(detected_hands) == 0:
            for t in self.tracks.values():
                if t.active:
                    t.lost_count += 1
                    if t.lost_count > 15:
                        t.active = False
            return self._get_active_hands()

        if len(detected_hands) == 1:
            det = detected_hands[0]
            det_pos = (det["x"], det["y"])
            active_tracks = [t for t in self.tracks.values() if t.active]

            if len(active_tracks) == 1:
                cur = active_tracks[0]
                dist = math.hypot(det_pos[0] - cur.pos[0], det_pos[1] - cur.pos[1])
                if dist < 0.38:
                    # Stessa mano in movimento fluido: mantieni l'identità esistente
                    cur.pos = det_pos
                    cur.lost_count = 0
                    det["handedness"] = cur.side
                    cur.last_data = det
                    # Incrementa lost sull'altra mano inattiva
                    other_side = "Left" if cur.side == "Right" else "Right"
                    if self.tracks[other_side].active:
                        self.tracks[other_side].lost_count += 1
                        if self.tracks[other_side].lost_count > 15:
                            self.tracks[other_side].active = False
                    return [det]
                else:
                    # Salto spaziale notevole: usa la predizione neurale nativa
                    assigned_side = det["native_side"]
                    t = self.tracks[assigned_side]
                    t.active = True
                    t.pos = det_pos
                    t.lost_count = 0
                    det["handedness"] = assigned_side
                    t.last_data = det
                    return [det]

            elif len(active_tracks) == 2:
                d_right = math.hypot(det_pos[0] - self.tracks["Right"].pos[0], det_pos[1] - self.tracks["Right"].pos[1])
                d_left = math.hypot(det_pos[0] - self.tracks["Left"].pos[0], det_pos[1] - self.tracks["Left"].pos[1])
                closer_side = "Right" if d_right < d_left else "Left"
                other_side = "Left" if closer_side == "Right" else "Right"

                cur = self.tracks[closer_side]
                cur.pos = det_pos
                cur.lost_count = 0
                det["handedness"] = closer_side
                cur.last_data = det

                other = self.tracks[other_side]
                other.lost_count += 1
                if other.lost_count > 12:
                    other.active = False
                return [det]
            else:
                # Nessun track attivo precedente: usa l'etichetta nativa di MediaPipe
                assigned_side = det["native_side"]
                t = self.tracks[assigned_side]
                t.active = True
                t.pos = det_pos
                t.lost_count = 0
                det["handedness"] = assigned_side
                t.last_data = det
                return [det]

        # Due mani rilevate contemporaneamente
        if len(detected_hands) >= 2:
            h1 = detected_hands[0]
            h2 = detected_hands[1]
            p1 = (h1["x"], h1["y"])
            p2 = (h2["x"], h2["y"])

            if self.tracks["Right"].active and self.tracks["Left"].active:
                # Bipartite matching con le posizioni precedenti
                cost_direct = (math.hypot(p1[0] - self.tracks["Right"].pos[0], p1[1] - self.tracks["Right"].pos[1]) +
                               math.hypot(p2[0] - self.tracks["Left"].pos[0], p2[1] - self.tracks["Left"].pos[1]))
                cost_crossed = (math.hypot(p2[0] - self.tracks["Right"].pos[0], p2[1] - self.tracks["Right"].pos[1]) +
                                math.hypot(p1[0] - self.tracks["Left"].pos[0], p1[1] - self.tracks["Left"].pos[1]))

                if cost_direct <= cost_crossed:
                    h1["handedness"] = "Right"
                    h2["handedness"] = "Left"
                    self.tracks["Right"].pos = p1
                    self.tracks["Left"].pos = p2
                else:
                    h2["handedness"] = "Right"
                    h1["handedness"] = "Left"
                    self.tracks["Right"].pos = p2
                    self.tracks["Left"].pos = p1

                self.tracks["Right"].lost_count = 0
                self.tracks["Left"].lost_count = 0
            else:
                # Almeno un track non è attivo: usa la classificazione anatomica
                s1 = h1["native_side"]
                s2 = h2["native_side"]
                if s1 != s2:
                    h1["handedness"] = s1
                    h2["handedness"] = s2
                else:
                    # Ambiguità anatomica: nello schermo selfie specchiato la mano destra dell'utente è a destra (x maggiore)
                    if h1["x"] >= h2["x"]:
                        h1["handedness"] = "Right"
                        h2["handedness"] = "Left"
                    else:
                        h1["handedness"] = "Left"
                        h2["handedness"] = "Right"

                for h in [h1, h2]:
                    side = h["handedness"]
                    self.tracks[side].active = True
                    self.tracks[side].pos = (h["x"], h["y"])
                    self.tracks[side].lost_count = 0

            # Restituisce le due mani: prima Right (DX) poi Left (SX)
            out = [h1, h2] if h1["handedness"] == "Right" else [h2, h1]
            return out

        return self._get_active_hands()

    def _get_active_hands(self):
        res = []
        if self.tracks["Right"].active and self.tracks["Right"].last_data:
            res.append(self.tracks["Right"].last_data)
        if self.tracks["Left"].active and self.tracks["Left"].last_data:
            res.append(self.tracks["Left"].last_data)
        return res

def compute_kinematic_flex(mcp, pip, dip, tip):
    """
    Calcola la flessione cinematica 3D del dito:
    L = lunghezza totale delle tre falangi
    D = distanza euclidea 3D punta-nocca
    Invariante per rotazione della mano, orientamento e distanza dalla camera.
    """
    l1 = dist3d(mcp, pip)
    l2 = dist3d(pip, dip)
    l3 = dist3d(dip, tip)
    l_tot = max(0.015, l1 + l2 + l3)
    d_tip_mcp = dist3d(mcp, tip)
    ratio = d_tip_mcp / l_tot
    # ratio ~ 0.95-1.0 (dito disteso dritto) -> flex = 0.0
    # ratio ~ 0.25-0.35 (dito piegato nel pugno) -> flex = 1.0
    return max(0.0, min(1.0, (0.95 - ratio) / 0.62))

def compute_thumb_flex(cmc, mcp, ip, tip, wrist, pinky_mcp):
    """
    Calcola la flessione del pollice verso il palmo.
    """
    l1 = dist3d(cmc, mcp)
    l2 = dist3d(mcp, ip)
    l3 = dist3d(ip, tip)
    l_tot = max(0.015, l1 + l2 + l3)
    d_tip_cmc = dist3d(cmc, tip)
    ratio = d_tip_cmc / l_tot
    return max(0.0, min(1.0, (0.92 - ratio) / 0.50))

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8088
    base_url = f"http://127.0.0.1:{port}"
    frame_url = f"{base_url}/frame.jpg"
    data_url = f"{base_url}/data"

    log(f"Worker avviato. PID={os.getpid()}, Python={sys.executable}, TargetPort={port}")

    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_candidates = [
        os.path.join(script_dir, "hand_landmarker.task"),
        os.path.join(script_dir, "Resources", "mediapipe", "hand_landmarker.task"),
        os.path.join(r"C:\Users\Utente\AppData\Local\MiMOHub\Resources\mediapipe\hand_landmarker.task"),
        os.path.join(r"H:\Il mio Drive\5. Programmazione MIMO\MiMO_Project\mimo_hub_juce\Resources\mediapipe\hand_landmarker.task")
    ]
    model_path = None
    for cand in model_candidates:
        if os.path.isfile(cand):
            model_path = cand
            break

    if not model_path:
        log(f"ERRORE CRITICO: hand_landmarker.task non trovato nei candidati: {model_candidates}")
        sys.exit(1)

    log(f"Modello MediaPipe caricato da: {model_path}")

    base_options = python.BaseOptions(model_asset_path=model_path)
    # Modalità RunningMode.VIDEO per tracciamento continuo a bassissima latenza e massima robustezza
    options = vision.HandLandmarkerOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.VIDEO,
        num_hands=2,
        min_hand_detection_confidence=0.25,
        min_hand_presence_confidence=0.25,
        min_tracking_confidence=0.25
    )
    detector = vision.HandLandmarker.create_from_options(options)
    tracker = HandTracker()
    log("MediaPipe HandLandmarker inizializzato con successo in RunningMode.VIDEO")

    session = requests.Session()
    last_seq = -1
    last_ts_ms = 0
    frame_count = 0
    last_fps_time = time.time()
    current_fps = 30
    consecutive_network_errors = 0
    total_detections_logged = 0

    while True:
        try:
            resp = session.get(frame_url, timeout=0.9)
            if resp.status_code == 200:
                consecutive_network_errors = 0
                seq_hdr = resp.headers.get("X-Frame-Seq")
                seq = int(seq_hdr) if seq_hdr else (last_seq + 1)

                if seq != last_seq:
                    last_seq = seq
                    pil_img = Image.open(io.BytesIO(resp.content)).convert('RGB')
                    img_np = np.array(pil_img)
                    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=img_np)

                    # Timestamp rigorosamente crescente per RunningMode.VIDEO
                    now_ms = int(time.time() * 1000)
                    ts_ms = max(now_ms, last_ts_ms + 1)
                    last_ts_ms = ts_ms

                    res = detector.detect_for_video(mp_image, ts_ms)

                    frame_count += 1
                    now_sec = time.time()
                    if now_sec - last_fps_time >= 1.0:
                        current_fps = max(1, int(frame_count / (now_sec - last_fps_time) + 0.5))
                        frame_count = 0
                        last_fps_time = now_sec

                    raw_detections = []
                    if res.hand_landmarks:
                        for h_idx, lm in enumerate(res.hand_landmarks):
                            # Etichetta anatomica:
                            # Poiché il frame in arrivo da JUCE è specchiato per il selfie mirror dell'utente,
                            # l'etichetta restituita da MediaPipe (addestrato su immagini non specchiate) è invertita:
                            # "Left" prodotta da MediaPipe corrisponde all'effettiva mano DESTRA dell'utente.
                            # "Right" prodotta da MediaPipe corrisponde all'effettiva mano SINISTRA dell'utente.
                            score = 0.5
                            mp_cat = "Left"
                            if res.handedness and len(res.handedness) > h_idx and len(res.handedness[h_idx]) > 0:
                                mp_cat = res.handedness[h_idx][0].category_name
                                score = res.handedness[h_idx][0].score

                            user_side = "Right" if mp_cat == "Left" else "Left"

                            wrist = lm[0]
                            mid_mcp = lm[9]
                            pinky_mcp = lm[17]

                            # Centro palmo 3D normalizzato
                            palm_x = max(0.0, min(1.0, (wrist.x + mid_mcp.x) * 0.5))
                            palm_y = max(0.0, min(1.0, 1.0 - (wrist.y + mid_mcp.y) * 0.5))

                            # Profondità Z calibrata sulla dimensione della mano
                            palm_scale = max(0.04, dist3d(wrist, mid_mcp))
                            palm_z = max(0.0, min(1.0, (palm_scale - 0.08) / 0.22))

                            # Rotazione / Tilt palmo (inclinazione su piano 2D)
                            ang = math.atan2(mid_mcp.x - wrist.x, wrist.y - mid_mcp.y)
                            tilt = max(0.0, min(1.0, 0.5 + (ang / 1.5)))

                            # Flessione individuale delle 5 dita: [Pollice, Indice, Medio, Anulare, Mignolo]
                            f_thumb = compute_thumb_flex(lm[1], lm[2], lm[3], lm[4], wrist, pinky_mcp)
                            f_idx   = compute_kinematic_flex(lm[5], lm[6], lm[7], lm[8])
                            f_mid   = compute_kinematic_flex(lm[9], lm[10], lm[11], lm[12])
                            f_rng   = compute_kinematic_flex(lm[13], lm[14], lm[15], lm[16])
                            f_pky   = compute_kinematic_flex(lm[17], lm[18], lm[19], lm[20])
                            finger_flex = [f_thumb, f_idx, f_mid, f_rng, f_pky]

                            # Chiusura pugno: media della chiusura delle 4 dita principali
                            fist = (f_idx + f_mid + f_rng + f_pky) * 0.25

                            # 4 Pinch Triggers: distanza tra la punta del pollice e le altre 4 dita
                            thumb_tip = lm[4]
                            pinches = []
                            for t_idx in [8, 12, 16, 20]:
                                d_pinch = dist3d(thumb_tip, lm[t_idx]) / palm_scale
                                # d_pinch < 0.22 -> tocco attivo (1.0), > 0.50 -> aperto (0.0)
                                p_val = max(0.0, min(1.0, (0.50 - d_pinch) / 0.28))
                                pinches.append(p_val)

                            landmarks_coords = [[p.x, p.y, getattr(p, 'z', 0.0) or 0.0] for p in lm]

                            raw_detections.append({
                                "native_side": user_side,
                                "mp_score": score,
                                "x": palm_x,
                                "y": palm_y,
                                "z": palm_z,
                                "fist": fist,
                                "tilt": tilt,
                                "fingerFlex": finger_flex,
                                "pinches": pinches,
                                "landmarks": landmarks_coords
                            })

                    hands_out = tracker.update(raw_detections)

                    if len(hands_out) > 0 and total_detections_logged < 10:
                        total_detections_logged += 1
                        sides = [h.get("handedness") for h in hands_out]
                        log(f"Rilevate mani: {sides} (seq={seq}, fps={current_fps})")

                    payload = {
                        "fps": current_fps,
                        "hands": hands_out
                    }
                    session.post(data_url, json=payload, timeout=0.5)
                else:
                    time.sleep(0.010)
            elif resp.status_code == 204:
                # La camera non ha ancora prodotto un nuovo frame: attendi brevemente
                time.sleep(0.020)
            else:
                time.sleep(0.050)

        except requests.exceptions.RequestException:
            consecutive_network_errors += 1
            if consecutive_network_errors > 80:
                log("MiMO Hub non risponde più su HTTP (chiuso): uscita worker.")
                sys.exit(0)
            time.sleep(0.10)
        except Exception as e:
            log(f"Errore ciclo worker: {traceback.format_exc()}")
            time.sleep(0.04)

if __name__ == '__main__':
    main()
