#!/usr/bin/env python3
import os
import glob
import numpy as np
from PIL import Image

# Parameter
WIDTH  = 320
HEIGHT = 240
RAW_EXT = ".raw"
OUT_EXT = ".png"
INPUT_DIR = "/home/joe/esp_images"

def convert_raw_to_png(raw_path, width, height, out_path):
    # 1) Rohdaten einlesen
    with open(raw_path, "rb") as f:
        raw = f.read()

    # 2) Als 16-Bit Big-Endian-Werte interpretieren (Sensor liefert High-Byte zuerst)
    data = np.frombuffer(raw, dtype=">u2")
    if data.size != width * height:
        print(f"Warnung: {raw_path} enthält {data.size} Pixel, erwartet {width*height}")
        return

    # 3) In 2D-Array umformen
    data = data.reshape((height, width))

    # 4) RGB565 → 8-Bit-RGB
    r = ((data >> 11) & 0x1F) << 3
    g = ((data >> 5 ) & 0x3F) << 2
    b = ( data        & 0x1F) << 3

    rgb = np.dstack((r, g, b)).astype(np.uint8)

    # 5) Als PNG speichern
    img = Image.fromarray(rgb, "RGB")
    img.save(out_path)
    print(f"Konvertiert: {raw_path} → {out_path}")

def main():
    pattern = os.path.join(INPUT_DIR, f"*{RAW_EXT}")
    raws = glob.glob(pattern)
    if not raws:
        print(f"Keine {RAW_EXT}-Dateien in {INPUT_DIR} gefunden.")
        return

    for raw_path in raws:
        base, _ = os.path.splitext(raw_path)
        out_path = base + OUT_EXT
        convert_raw_to_png(raw_path, WIDTH, HEIGHT, out_path)

if __name__ == "__main__":
    main()
