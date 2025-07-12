import os
import glob
import csv
import time
import math
import paho.mqtt.client as mqtt
from collections import deque, defaultdict

# Konfiguration
MQTT_BROKER = "192.168.0.120"
MQTT_PORT = 1883
MQTT_USER = "joe"
MQTT_PASSWORD = "1337"
MQTT_TOPIC_PREFIX = "V"
COMMAND_TOPIC = "HW-888/command"

# Regler-Parameter
PH_LOW = 5.5                   # Untere Grenze des Sollbereichs
PH_HIGH = 6.0                  # Obere Grenze des Sollbereichs
PULSE_STEP = 0.3               # Abweichung pro Puls
PH_DURATION_THRESHOLD = 60 * 60  # Fenstergröße in Sekunden für Mittelwert
PH_COOLDOWN = 60 * 60            # Cooldown nach Anpassung in Sekunden

# Logs-Verzeichnis zum Laden der Zähler
LOG_DIR = os.path.expanduser("~/logs")
CSV_PATTERN = os.path.join(LOG_DIR, "mqtt_log_*.csv")

# Hilfsfunktion: Lädt letzte Anpassungszähler aus der neuesten CSV
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
                down = int(last.get(f"V{i}/phdown", "0") or 0)
                up   = int(last.get(f"V{i}/phup",   "0") or 0)
                counts[key] = {"phDown": down, "phUp": up}
    except Exception:
        pass
    return counts

# Initiale Zähler laden und Default-Werte setzen
loaded_counts = load_adjustment_counts()
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})
for v, vals in loaded_counts.items():
    adjustment_count[v] = vals

# Puffer für pH-Messwerte
ph_history = {f"{MQTT_TOPIC_PREFIX}{i}/pH": deque(maxlen=10000) for i in range(1, 5)}
last_action_time = defaultdict(lambda: 0)

# MQTT-Client einrichten
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

# Initiale Zähler publishen
def publish_initial_counts():
    for v, vals in adjustment_count.items():
        client.publish(f"{v}/phdown", vals["phDown"])
        client.publish(f"{v}/phup",   vals["phUp"])

# Callback bei Verbindung
def on_connect(client, userdata, flags, rc):
    print("Verbunden mit MQTT-Broker (Code {}), abonniere Topics...".format(rc))
    for topic in ph_history:
        client.subscribe(topic)
        print(f"Abonniert: {topic}")
    publish_initial_counts()

# Callback bei eingehender Nachricht
def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        ph = float(msg.payload.decode())
    except ValueError:
        return
    now = time.time()
    ph_history[topic].append((now, ph))

    # Mittelwertbildung über definiertes Zeitfenster
    recent = [v for t, v in ph_history[topic] if now - t <= PH_DURATION_THRESHOLD]
    if not ph_history[topic] or now - ph_history[topic][0][0] < PH_DURATION_THRESHOLD:
        return

    v = topic.split('/')[0]
    # Cooldown prüfen
    if now - last_action_time[v] < PH_COOLDOWN:
        return

    avg = sum(recent) / len(recent)
    # Prüfen, ob avg außerhalb des Sollbereichs
    if PH_LOW <= avg <= PH_HIGH:
        return  # Innerhalb des gewünschten Bereichs

    # Abweichung bestimmen
    if avg < PH_LOW:
        delta = PH_LOW - avg
        direction = "up"
    else:
        delta = avg - PH_HIGH
        direction = "down"

    # Anzahl Pulsbefehle berechnen
    pulses = math.ceil(delta / PULSE_STEP)
    print(f"{v}: avg={avg:.2f}, delta={delta:.2f} -> {pulses} x ph{direction}")

    # Aktionssequenz
def run_sequence(v, direction, count):
    open_cmd  = f"{v.lower()}valveopen"
    close_cmd = f"{v.lower()}valveclose"
    client.publish(COMMAND_TOPIC, open_cmd)
    client.loop_write()
    time.sleep(5)
    for _ in range(count):
        client.publish(COMMAND_TOPIC, f"ph{direction}")
        client.loop_write()
        time.sleep(5)
    client.publish(COMMAND_TOPIC, "water")
    client.loop_write()
    time.sleep(15)
    client.publish(COMMAND_TOPIC, close_cmd)
    client.loop_write()

# Zähler publishen
def publish_adjustment_count(v, direction):
    topic = f"{v}/ph{direction}"
    count = adjustment_count[v]["ph" + direction.capitalize()]
    client.publish(topic, count)
    print(f"{topic}: {count}")

# MQTT-Handlers registrieren und Client starten
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
