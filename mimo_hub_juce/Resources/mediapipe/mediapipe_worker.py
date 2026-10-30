"""
MiMO Eye - Worker Python Nativo per Google MediaPipe HandLandmarker
Elabora i fotogrammi a 30+ FPS via CPU XNNPACK e invia lo scheletro reale a 21 punti al server MiMO Hub.
Supporta entrambe le mani contemporaneamente (Right + Left) e sopprime falsi positivi su volti/oggetti.
"""
import sys
import time
import math
import io
import os

# Sopprime il log C++ di TensorFlow e MediaPipe su stderr per non bloccare la pipe su Windows
os.environ["GLOG_minloglevel"] = "2"
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"

import requests
import numpy as np
from PIL import Image
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

def dist3d(p1, p2):
    dx = p1.x - p2.x
    dy = p1.y - p2.y
    dz = (getattr(p1, 'z', 0.0) or 0.0) - (getattr(p2, 'z', 0.0) or 0.0)
    return math.sqrt(dx * dx + dy * dy + dz * dz)

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8088
    base_url = f"http://127.0.0.1:{port}"
    frame_url = f"{base_url}/frame.jpg"
    data_url = f"{base_url}/data"

    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_candidates = [
        os.path.join(script_dir, "hand_landmarker.task"),
        os.path.join(script_dir, "Resources", "mediapipe", "hand_landmarker.task"),
        os.path.join("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/Resources/mediapipe/hand_landmarker.task")
    ]
    model_path = None
    for cand in model_candidates:
        if os.path.isfile(cand):
            model_path = cand
            break

    if not model_path:
        sys.exit(1)

    base_options = python.BaseOptions(model_asset_path=model_path)
    # Confidenza 0.60 per evitare categoricamente allucinazioni su volti / vestiti / sfondi
    options = vision.HandLandmarkerOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.IMAGE,
        num_hands=2,
        min_hand_detection_confidence=0.60,
        min_hand_presence_confidence=0.60,
        min_tracking_confidence=0.60
    )
    detector = vision.HandLandmarker.create_from_options(options)

    session = requests.Session()
    last_seq = -1
    frame_count = 0
    last_fps_time = time.time()
    current_fps = 30

    while True:
        try:
            resp = session.get(frame_url, timeout=0.8)
            if resp.status_code == 200:
                seq_hdr = resp.headers.get("X-Frame-Seq")
                seq = int(seq_hdr) if seq_hdr else (last_seq + 1)
                if seq != last_seq:
                    last_seq = seq
                    pil_img = Image.open(io.BytesIO(resp.content)).convert('RGB')
                    img_np = np.array(pil_img)
                    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=img_np)
                    res = detector.detect(mp_image)

                    frame_count += 1
                    now = time.time()
                    if now - last_fps_time >= 1.0:
                        current_fps = max(1, int(frame_count / (now - last_fps_time) + 0.5))
                        frame_count = 0
                        last_fps_time = now

                    hands_out = []
                    if res.hand_landmarks:
                        for h_idx, lm in enumerate(res.hand_landmarks):
                            side = "Right" if h_idx == 0 else "Left"
                            if res.handedness and len(res.handedness) > h_idx and len(res.handedness[h_idx]) > 0:
                                raw_label = res.handedness[h_idx][0].category_name
                                # L'immagine e specchiata: inverte Left/Right per vista naturale a specchio
                                side = "Right" if raw_label == "Left" else "Left"

                            wrist = lm[0]
                            mid_mcp = lm[9]
                            palm_scale = max(0.04, dist3d(wrist, mid_mcp))
                            palm_x = max(0.0, min(1.0, (wrist.x + mid_mcp.x) * 0.5))
                            palm_y = max(0.0, min(1.0, 1.0 - (wrist.y + mid_mcp.y) * 0.5))
                            palm_z = max(0.0, min(1.0, (palm_scale - 0.08) / 0.22))

                            # Angolo rotazione palmo
                            ang = math.atan2(mid_mcp.x - wrist.x, wrist.y - mid_mcp.y)
                            tilt = max(0.0, min(1.0, 0.5 + (ang / 1.4)))

                            # Flessione 5 dita: [Pollice, Indice, Medio, Anulare, Mignolo]
                            tip_ids = [4, 8, 12, 16, 20]
                            mcp_ids = [2, 5, 9, 13, 17]
                            finger_flex = []
                            for f in range(5):
                                d_tip = dist3d(wrist, lm[tip_ids[f]])
                                d_mcp = max(0.03, dist3d(wrist, lm[mcp_ids[f]]))
                                ratio = d_tip / d_mcp
                                if f == 0:
                                    flex = max(0.0, min(1.0, (1.55 - ratio) / 0.55))
                                else:
                                    flex = max(0.0, min(1.0, (1.88 - ratio) / 0.90))
                                finger_flex.append(flex)

                            fist = sum(finger_flex[1:]) * 0.25

                            # 4 Pinch (Pollice [4] con Indice [8], Medio [12], Anulare [16], Mignolo [20])
                            thumb_tip = lm[4]
                            pinches = []
                            for t_idx in [8, 12, 16, 20]:
                                d_pinch = dist3d(thumb_tip, lm[t_idx]) / palm_scale
                                pinches.append(max(0.0, min(1.0, (0.45 - d_pinch) / 0.24)))

                            landmarks_coords = [[p.x, p.y, getattr(p, 'z', 0.0) or 0.0] for p in lm]

                            hands_out.append({
                                "handedness": side,
                                "x": palm_x,
                                "y": palm_y,
                                "z": palm_z,
                                "fist": fist,
                                "tilt": tilt,
                                "fingerFlex": finger_flex,
                                "pinches": pinches,
                                "landmarks": landmarks_coords
                            })

                        # Se ci sono 2 mani ed entrambe hanno ricevuto la stessa etichetta (es. entrambe Right),
                        # differenziale coerentemente in base alla posizione orizzontale nello specchio:
                        # Mano a sinistra dello schermo = Destra dell'utente; Mano a destra dello schermo = Sinistra dell'utente.
                        if len(hands_out) == 2:
                            if hands_out[0]["handedness"] == hands_out[1]["handedness"]:
                                if hands_out[0]["x"] < hands_out[1]["x"]:
                                    hands_out[0]["handedness"] = "Right"
                                    hands_out[1]["handedness"] = "Left"
                                else:
                                    hands_out[0]["handedness"] = "Left"
                                    hands_out[1]["handedness"] = "Right"

                            # Ordina in modo che la Mano Destra sia sempre l'indice 0 (primaria) e la Sinistra sia 1 (secondaria)
                            if hands_out[0]["handedness"] == "Left" and hands_out[1]["handedness"] == "Right":
                                hands_out[0], hands_out[1] = hands_out[1], hands_out[0]

                    payload = {
                        "fps": current_fps,
                        "hands": hands_out
                    }
                    session.post(data_url, json=payload, timeout=0.5)
                else:
                    time.sleep(0.012)
            else:
                time.sleep(0.035)
        except requests.exceptions.RequestException:
            time.sleep(0.1)
        except Exception:
            time.sleep(0.05)

if __name__ == '__main__':
    main()

