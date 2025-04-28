#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import os
import datetime

# Broker-Konfiguration
BROKER   = "192.168.0.120"
PORT     = 1883
USER     = "joe"
PASSWORD = "1337"

# Topics
START_TOPIC = "esp32/image/start"
DATA_TOPIC  = "esp32/image/data"
END_TOPIC   = "esp32/image/end"

# Speicherort für die Bilder
SAVE_DIR = "/home/joe/esp_images"
os.makedirs(SAVE_DIR, exist_ok=True)

# Empfangspuffer und erwartete Länge
buffer = bytearray()
expected_len = 0

def on_connect(client, userdata, flags, rc):
    print("Verbunden mit MQTT, Code:", rc)
    client.subscribe([(START_TOPIC, 0),
                      (DATA_TOPIC, 0),
                      (END_TOPIC, 0)])

def on_message(client, userdata, msg):
    global buffer, expected_len
    topic = msg.topic
    if topic == START_TOPIC:
        expected_len = int(msg.payload.decode())
        buffer = bytearray()
        print(f"[START] Erwarte {expected_len} Bytes")
    elif topic == DATA_TOPIC:
        buffer.extend(msg.payload)
        print(f"[DATA] {len(buffer)}/{expected_len} Bytes empfangen")
    elif topic == END_TOPIC:
        if len(buffer) == expected_len:
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = os.path.join(SAVE_DIR, f"esp_capture_{ts}.jpg")
            with open(filename, "wb") as f:
                f.write(buffer)
            print(f"[END] Bild gespeichert: {filename}")
        else:
            print(f"[END] Empfang unvollständig: {len(buffer)}/{expected_len}")
        buffer = bytearray()
        expected_len = 0

def main():
    client = mqtt.Client()
    client.username_pw_set(USER, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT)
    client.loop_forever()

if __name__ == "__main__":
    main()
