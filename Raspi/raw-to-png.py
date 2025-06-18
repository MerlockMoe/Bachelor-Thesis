#!/usr/bin/env python3
import os, glob, numpy as np
from PIL import Image

# Parameter
WIDTH, HEIGHT = 320, 240
INPUT_DIR  = "/home/joe/esp_images" 
RAW_EXT    = ".raw"
OUT_EXT    = ".png"

def yuv422_to_rgb(raw_bytes, w, h):
    # Rohbytes in 1D-Array
    data = np.frombuffer(raw_bytes, dtype=np.uint8)
    # In (H, W/2, 4)-Array umformen: [Y0, U, Y1, V]
    arr = data.reshape((h, w // 2, 4))

    # YUV-Kanäle extrahieren
    Y0 = arr[:, :, 0].astype(np.int16)
    U  = arr[:, :, 1].astype(np.int16) - 128
    Y1 = arr[:, :, 2].astype(np.int16)
    V  = arr[:, :, 3].astype(np.int16) - 128

    # Formel: R = Y + 1.402*V, G = Y - 0.344*U - 0.714*V, B = Y + 1.772*U
    def clip(x): return np.clip(x, 0, 255).astype(np.uint8)

    # Für zwei Pixel pro U/V-Paar
    R0 = clip(Y0 + 1.402 * V)
    G0 = clip(Y0 - 0.344 * U - 0.714 * V)
    B0 = clip(Y0 + 1.772 * U)

    R1 = clip(Y1 + 1.402 * V)
    G1 = clip(Y1 - 0.344 * U - 0.714 * V)
    B1 = clip(Y1 + 1.772 * U)

    # Pixel wieder nebeneinanderlegen
    top  = np.dstack((R0, G0, B0))
    bot  = np.dstack((R1, G1, B1))
    # Interleave die beiden Hälften
    rgb = np.empty((h, w, 3), dtype=np.uint8)
    rgb[:, 0::2] = top
    rgb[:, 1::2] = bot
    return rgb

def convert_all():
    os.makedirs(INPUT_DIR, exist_ok=True)
    for raw_path in glob.glob(os.path.join(INPUT_DIR, f"*{RAW_EXT}")):
        with open(raw_path, "rb") as f:
            raw = f.read()
        if len(raw) != WIDTH * HEIGHT * 2:
            print(f"Warnung: {raw_path} hat {len(raw)} Bytes, erwartet {WIDTH*HEIGHT*2}")
            continue
        rgb = yuv422_to_rgb(raw, WIDTH, HEIGHT)
        img = Image.fromarray(rgb, "RGB")
        out_path = os.path.splitext(raw_path)[0] + OUT_EXT
        img.save(out_path)
        print(f"Konvertiert: {raw_path} → {out_path}")

if __name__ == "__main__":
    convert_all()
