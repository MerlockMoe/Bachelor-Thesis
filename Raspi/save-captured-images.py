#!/usr/bin/env python3
# Empfang von Bild-Frames per MQTT-Chunking für beliebig viele Kameras und Abspeicherung in kamera-spezifischen Unterverzeichnissen
import paho.mqtt.client as mqtt
import os
import datetime

# MQTT-Broker-Konfiguration
BROKER   = "192.168.0.120"
PORT     = 1883
USER     = "joe"
PASSWORD = "1337"

# Wildcard-Subscription für alle Kameras und Nachrichtenarten
TOPIC_ROOT = "esp32/#"

# Basis-Verzeichnis für empfangene Bilder
BASE_SAVE_DIR = "/home/joe/esp_images"

# Dictionaries für Zwischenspeicherung der Daten pro Kamera
buffers = {}
expected_lengths = {}

# Hilfsfunktion: Extrahiere Kamera-ID und Nachrichtentyp aus Topic
def parse_topic(topic):
    # Format erwartet: esp32/<camera_id>/<msg_type>
    parts = topic.split('/')
    if len(parts) < 3:
        return None, None
    return parts[1], parts[2]

# Callback bei erfolgreicher Verbindung
def on_connect(client, userdata, flags, rc):
    print(f"Verbunden zum MQTT-Broker (Code {rc}), abonniere '{TOPIC_ROOT}'...")
    client.subscribe(TOPIC_ROOT)

# Callback bei eingehender Nachricht
def on_message(client, userdata, msg):
    camera_id, msg_type = parse_topic(msg.topic)
    if not camera_id or not msg_type:
        return

    # Verzeichnis für die Kamera anlegen
    save_dir = os.path.join(BASE_SAVE_DIR, camera_id)
    os.makedirs(save_dir, exist_ok=True)

    if msg_type == "start":
        length = int(msg.payload.decode())
        expected_lengths[camera_id] = length
        buffers[camera_id] = bytearray()
        print(f"[{camera_id}] START: erwarte {length} Bytes")
    elif msg_type == "data":
        buffers[camera_id].extend(msg.payload)
        received = len(buffers[camera_id])
        expected = expected_lengths.get(camera_id, 0)
        print(f"[{camera_id}] DATA: {received}/{expected} Bytes")
    elif msg_type == "end":
        data = buffers.get(camera_id, bytearray())
        expected = expected_lengths.get(camera_id, 0)
        if len(data) == expected:
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = os.path.join(save_dir, f"{camera_id}_{ts}.jpg")
            with open(filename, 'wb') as f:
                f.write(data)
            print(f"[{camera_id}] Bild gespeichert: {filename}")
        else:
            print(f"[{camera_id}] Unvollständig: {len(data)}/{expected} Bytes")
        # Aufräumen
        buffers.pop(camera_id, None)
        expected_lengths.pop(camera_id, None)

# Hauptprogramm
if __name__ == '__main__':
    client = mqtt.Client()
    client.username_pw_set(USER, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT)
    client.loop_forever()
