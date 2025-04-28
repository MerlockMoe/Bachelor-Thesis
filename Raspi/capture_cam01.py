#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import os, datetime

BROKER   = "192.168.0.120"
PORT     = 1883
USER     = "joe"
PASSWORD = "1337"

START_TOPIC = "esp32/image/start"
DATA_TOPIC  = "esp32/image/data"
END_TOPIC   = "esp32/image/end"
SAVE_DIR    = "/home/joe/esp_images"

os.makedirs(SAVE_DIR, exist_ok=True)
buffer = bytearray()
expected_len = 0

def on_connect(client, userdata, flags, rc):
    client.subscribe([(START_TOPIC,0),(DATA_TOPIC,0),(END_TOPIC,0)])

def on_message(client, userdata, msg):
    global buffer, expected_len
    if msg.topic == START_TOPIC:
        expected_len = int(msg.payload.decode())
        buffer = bytearray()
    elif msg.topic == DATA_TOPIC:
        buffer.extend(msg.payload)
    elif msg.topic == END_TOPIC:
        if len(buffer) == expected_len:
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            fn = f"{SAVE_DIR}/esp_{ts}.raw"
            with open(fn, "wb") as f:
                f.write(buffer)
            print(f"Bild gespeichert: {fn}")
        else:
            print(f"Unvollständig: {len(buffer)}/{expected_len}")
        buffer = bytearray()
        expected_len = 0

client = mqtt.Client()
client.username_pw_set(USER, PASSWORD)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT)
client.loop_forever()
