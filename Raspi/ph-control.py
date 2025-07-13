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
WINDOW_SEC = 10 * 60      # Mittelwertfenster in Sekunden
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
                adjustment_count[key] = {"phDown": down, "phUp": up}
    except Exception:
        pass
    return adjustment_count


def load_initial_ph_history():
    # Fülle ph_history mit Werten der letzten Stunde aus der letzten CSV
    files = glob.glob(CSV_PATTERN)
    if not files:
        return
    latest = max(files, key=os.path.getmtime)
    # Datum aus Dateiname extrahieren
    try:
        date_str = os.path.basename(latest).split('_')[-1].split('.')[0]  # YYYYMMDD
        date_base = datetime.strptime(date_str, '%Y%m%d')
    except Exception:
        return
    try:
        with open(latest, newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Zeitpunkt parsieren: MM-DD-HH-MM
                parts = row['id'].split('-')
                if len(parts) != 4:
                    continue
                month, day, hour, minute = map(int, parts)
                dt = datetime(year=date_base.year, month=month, day=day, hour=hour, minute=minute)
                ts = dt.timestamp()
                # Füge zu jedem Thema hinzu
                for i in range(1, 5):
                    topic = f"V{i}/pH"
                    val_str = row.get(topic, '')
                    try:
                        val = float(val_str)
                        # Nur Werte der letzten WINDOW_SEC
                        if time.time() - ts <= WINDOW_SEC:
                            ph_history[topic].append((ts, val))
                    except ValueError:
                        continue
    except Exception:
        pass

# --- Initialisierung ---
# Lade Zähler und initial publishing erfolgt in on_connect
load_adjustment_counts()
# Lade historische pH-Werte für initiales Fenster
load_initial_ph_history()

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
    time.sleep(5)
    for _ in range(pulses):
        client.publish(COMMAND_TOPIC, f"ph{direction}")
        time.sleep(5)
    client.publish(COMMAND_TOPIC, "water")
    time.sleep(15)
    client.publish(COMMAND_TOPIC, close_cmd)


def on_connect(client, userdata, flags, rc):
    print(f"[INFO] Verbunden mit MQTT-Broker (rc={rc})")
    for topic in ph_history.keys():
        client.subscribe(topic)
    # Initiale Zähler publishen
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
    # Puffer aktualisieren
    ph_history[topic].append((now, ph))
    # Werte im aktuellen Fenster
    recent = [(t, val) for (t, val) in ph_history[topic] if now - t <= WINDOW_SEC]
    # Fenster voll?
    if not ph_history[topic] or now - ph_history[topic][0][0] < WINDOW_SEC:
        return
    key = topic.split('/')[0]
    # Cooldown prüfen
    if now - last_action_time[key] < COOLDOWN_SEC:
        return
    # Durchschnitt
    avg = sum(val for (_, val) in recent) / len(recent)
    if PH_LOW <= avg <= PH_HIGH:
        return
    # Abweichung und Richtung
    if avg < PH_LOW:
        delta = PH_LOW - avg
        direction = "up"
    else:
        delta = avg - PH_HIGH
        direction = "down"
    pulses = math.ceil(delta / PULSE_STEP)
    # Aktion ausführen
    last_action_time[key] = now
    run_sequence(key, direction, pulses)
    # Zähler aktualisieren und publishen
    adj_key = "phDown" if direction == "down" else "phUp"
    adjustment_count[key][adj_key] += pulses
    publish_adjustment_count(key, direction)

# Client starten
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message
print("[INFO] Starte pH-Regler...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
