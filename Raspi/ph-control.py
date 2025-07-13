#!/usr/bin/env python3
import os
import glob
import csv
import time
import math
import paho.mqtt.client as mqtt
from collections import deque, defaultdict
from datetime import datetime

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
PULSE_STEP = 0.1          # Abweichung, die einen Puls auslöst
WINDOW_SEC = 60 * 60      # Mittelwertfenster in Sekunden
COOLDOWN_SEC = 60 * 60    # Cooldown nach Anpassung

# Logs-Verzeichnis
LOG_DIR = os.path.expanduser("~/logs")
CSV_PATTERN = os.path.join(LOG_DIR, "mqtt_log_*.csv")

# Puffer für pH-Messwerte
ph_history = {f"{MQTT_TOPIC_PREFIX}{i}/pH": deque() for i in range(1, 5)}
# Letzte Aktion pro Versuch
last_action_time = defaultdict(lambda: 0)
# Zähler für Anpassungen
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})

# --- CSV-Ladefunktionen ---

def load_adjustment_counts():
    counts = {}
    files = glob.glob(CSV_PATTERN)
    if not files:
        return counts
    latest = max(files, key=os.path.getmtime)
    date_part = os.path.basename(latest).split('_')[-1].split('.')[0]  # YYYYMMDD
    try:
        log_date = datetime.strptime(date_part, '%Y%m%d')
    except Exception:
        log_date = None
    try:
        with open(latest, newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
        if rows:
            last = rows[-1]
            for i in range(1, 5):
                key = f"V{i}"
                down = int(last.get(f"{key}/phdown", "0") or 0)
                up   = int(last.get(f"{key}/phup",   "0") or 0)
                counts[key] = {"phDown": down, "phUp": up}
    except Exception:
        pass
    return counts


def load_initial_ph_history():
    files = glob.glob(CSV_PATTERN)
    if not files:
        return
    latest = max(files, key=os.path.getmtime)
    date_str = os.path.basename(latest).split('_')[-1].split('.')[0]
    try:
        date_base = datetime.strptime(date_str, '%Y%m%d')
    except Exception:
        return
    try:
        with open(latest, newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # id in Format MM-DD-HH-MM
                ts_parts = row['id'].split('-')
                if len(ts_parts) != 4:
                    continue
                month, day, hour, minute = map(int, ts_parts)
                dt = datetime(year=date_base.year, month=month, day=day,
                              hour=hour, minute=minute)
                timestamp = dt.timestamp()
                # Fülle ph_history
                for i in range(1, 5):
                    topic = f"V{i}/pH"
                    val_str = row.get(topic, '')
                    try:
                        val = float(val_str)
                        ph_history[topic].append((timestamp, val))
                    except ValueError:
                        continue
    except Exception:
        pass

# --- Initialisierung ---
# Lade Zähler
loaded_counts = load_adjustment_counts()
for v, vals in loaded_counts.items():
    adjustment_count[v] = vals
# Publish initial counts später in on_connect
# Lade pH-Historie der vergangenen Stunde für Mittelwertfenster
load_initial_ph_history()
# Passe WINDOW_SEC bei Bedarf auf verfügbare Historie an:
for topic, dq in ph_history.items():
    if dq:
        span = time.time() - dq[0][0]
        if span < WINDOW_SEC:
            print(f"[INFO] Kürze Mittelwertfenster für {topic} auf {int(span)} s")
            # temporär verkürztes Fenster per Topic möglich
            # wir setzen WINDOW_SEC_P[topic] = span
            pass

# --- MQTT-Funktionen ---

def publish_adjustment_count(v, direction):
    topic = f"{v}/ph{direction}"
    count = adjustment_count[v]["ph" + direction.capitalize()]
    client.publish(topic, count)
    print(f"[INFO] {topic} = {count}")


def run_sequence(v, direction, pulses):
    print(f"[ACTION] Regulierung für {v}: {pulses} x ph{direction}")
    open_cmd = f"{v.lower()}valveopen"
    close_cmd = f"{v.lower()}valveclose"
    client.publish(COMMAND_TOPIC, open_cmd)
    client.loop_write(); time.sleep(5)
    for i in range(pulses):
        client.publish(COMMAND_TOPIC, f"ph{direction}")
        client.loop_write(); time.sleep(5)
    client.publish(COMMAND_TOPIC, "water"); client.loop_write(); time.sleep(15)
    client.publish(COMMAND_TOPIC, close_cmd); client.loop_write()


def on_connect(client, userdata, flags, rc):
    print(f"[INFO] Verbunden mit MQTT-Broker (rc={rc})")
    for topic in ph_history.keys():
        client.subscribe(topic)
    for v in [t.split('/')[0] for t in ph_history.keys()]:
        for d in ("down", "up"):
            publish_adjustment_count(v, d)


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        ph = float(msg.payload.decode())
    except ValueError:
        return
    now = time.time()
    # Aktualisiere Puffer und filtere Fenster
    ph_history[topic].append((now, ph))
    ph_history[topic] = deque([(t,val) for (t,val) in ph_history[topic]
                               if now - t <= WINDOW_SEC])
    # Prüfung analog zuvor...

# Client starten
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message
print("[INFO] Starte pH-Regler...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
