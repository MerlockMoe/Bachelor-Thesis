```python
#!/usr/bin/env python3
import os
import glob
import csv
import time
import math
import paho.mqtt.client as mqtt
from collections import deque, defaultdict

# --- Konfiguration ---
MQTT_BROKER = "192.168.0.120"
MQTT_PORT = 1883
MQTT_USER = "joe"
MQTT_PASSWORD = "1337"
MQTT_TOPIC_PREFIX = "V"
COMMAND_TOPIC = "HW-888/command"

# Sollbereich und Reglerparameter
PH_LOW = 5.5              # Untere Grenze
PH_HIGH = 6.0             # Obere Grenze
PULSE_STEP = 0.3          # Abweichung, die einen Puls auslöst
WINDOW_SEC = 60 * 60      # Mittelwertfenster in Sekunden
COOLDOWN_SEC = 60 * 60    # Cooldown nach Anpassung

# Logs-Verzeichnis zum Laden der alten Zähler
LOG_DIR = os.path.expanduser("~/logs")
CSV_PATTERN = os.path.join(LOG_DIR, "mqtt_log_*.csv")

# --- Hilfsfunktionen ---
def load_adjustment_counts():
    counts = {}
    files = glob.glob(CSV_PATTERN)
    if not files:
        return counts
    latest = max(files, key=os.path.getmtime)
    try:
        with open(latest, newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        if not rows:
            return counts
        last = rows[-1]
        for i in range(1, 5):
            key = f"V{i}"
            down = int(last.get(f"{key}/phdown", "0") or 0)
            up   = int(last.get(f"{key}/phup",   "0") or 0)
            counts[key] = {"phDown": down, "phUp": up}
    except Exception:
        pass
    return counts

# Initiale Zähler laden
loaded_counts = load_adjustment_counts()
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})
for v, vals in loaded_counts.items():
    adjustment_count[v] = vals

# Puffer für pH-Messwerte
ph_history = {f"{MQTT_TOPIC_PREFIX}{i}/pH": deque() for i in range(1, 5)}
last_action_time = defaultdict(lambda: 0)

# --- MQTT-Aktionen ---

def publish_adjustment_count(v, direction):
    topic = f"{v}/ph{direction}"
    count = adjustment_count[v]["ph" + direction.capitalize()]
    client.publish(topic, count)
    print(f"[INFO] {topic} = {count}")


def run_sequence(v, direction, pulses):
    open_cmd = f"{v.lower()}valveopen"
    close_cmd = f"{v.lower()}valveclose"
    print(f"[ACTION] Öffne Ventil {open_cmd}")
    client.publish(COMMAND_TOPIC, open_cmd)
    client.loop_write()
    time.sleep(5)

    for i in range(pulses):
        cmd = f"ph{direction}"
        print(f"[ACTION] Sende {cmd} ({i+1}/{pulses})")
        client.publish(COMMAND_TOPIC, cmd)
        client.loop_write()
        time.sleep(5)

    print("[ACTION] Spülen")
    client.publish(COMMAND_TOPIC, "water")
    client.loop_write()
    time.sleep(15)

    print(f"[ACTION] Schließe Ventil {close_cmd}")
    client.publish(COMMAND_TOPIC, close_cmd)
    client.loop_write()


def on_connect(client, userdata, flags, rc):
    print(f"[INFO] Verbunden mit MQTT-Broker (rc={rc})")
    # Abonniere alle pH-Topics
    for topic in ph_history.keys():
        client.subscribe(topic)
        print(f"[INFO] Subscribed to {topic}")
    # Publish der geladenen Zähler
    for v in ph_history.keys():
        key = v.split('/')[0]
        for d in ("down", "up"):
            publish_adjustment_count(key, d)


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        ph = float(msg.payload.decode())
    except ValueError:
        return
    now = time.time()

    # Eintragen in Puffer und beschneiden
    ph_history[topic].append((now, ph))
    ph_history[topic] = deque([
        (t, val) for (t, val) in ph_history[topic]
        if now - t <= WINDOW_SEC
    ])

    # Prüfen, ob Fenster voll
    if not ph_history[topic] or now - ph_history[topic][0][0] < WINDOW_SEC:
        return

    key = topic.split('/')[0]
    # Cooldown prüfen
    if now - last_action_time[key] < COOLDOWN_SEC:
        return

    # Mittelwert berechnen
    avg = sum(val for (_, val) in ph_history[topic]) / len(ph_history[topic])
    if PH_LOW <= avg <= PH_HIGH:
        print(f"[INFO] {key}: avg={avg:.2f} im Sollbereich")
        return

    # Bestimme Richtung und Abweichung
    if avg < PH_LOW:
        delta = PH_LOW - avg
        direction = "up"
    else:
        delta = avg - PH_HIGH
        direction = "down"

    pulses = math.ceil(delta / PULSE_STEP)
    print(f"[INFO] {key}: avg={avg:.2f}, delta={delta:.2f}, pulses={pulses}")

    # Führe Sequenz aus
    last_action_time[key] = now
    run_sequence(key, direction, pulses)
    # Zähler updaten und publizieren
    adj_key = "phDown" if direction == "down" else "phUp"
    adjustment_count[key][adj_key] += pulses
    publish_adjustment_count(key, direction)

# Client einrichten
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message

print("[INFO] Starte pH-Regler...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
```
