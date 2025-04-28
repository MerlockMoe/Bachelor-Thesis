#!/usr/bin/env python3
import socket
import struct
import os
from datetime import datetime

HOST = '0.0.0.0'
PORT = 6000
SAVE_DIR = '/home/joe/esp_images'

os.makedirs(SAVE_DIR, exist_ok=True)

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(1)
print(f"TCP-Server lauscht auf Port {PORT}")

while True:
    conn, addr = server.accept()
    try:
        # 4-Byte-Länge lesen
        raw_len = conn.recv(4)
        if len(raw_len) < 4:
            continue
        length = struct.unpack('<I', raw_len)[0]

        # Bilddaten empfangen
        data = b''
        while len(data) < length:
            chunk = conn.recv(min(4096, length - len(data)))
            if not chunk:
                break
            data += chunk

        # Datei speichern
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = f"{SAVE_DIR}/esp_capture_{timestamp}.jpg"
        with open(filename, 'wb') as f:
            f.write(data)
        print(f"Empfangen und gespeichert: {filename}")

    finally:
        conn.close()
