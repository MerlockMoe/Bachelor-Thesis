import os
import glob
import csv
import time
import paho.mqtt.client as mqtt
from collections import deque, defaultdict

# Konfiguration
MQTT_BROKER = "192.168.0.120"
MQTT_PORT = 1883
MQTT_USER = "joe"
MQTT_PASSWORD = "1337"
MQTT_TOPIC_PREFIX = "V"
COMMAND_TOPIC = "HW-888/command"

# pH-Schwellenwerte
PH_HIGH = 6.0
PH_LOW = 5.5
PH_DURATION_THRESHOLD = 60 * 60  # Fenstergröße in Sekunden
PH_COOLDOWN = 60 * 60            # Cooldown nach einer Anpassung in Sekunden

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
        with open(latest, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            rows = list(reader)
            if not rows:
                return counts
            last = rows[-1]
            for i in range(1, 5):
                key = f"V{i}"
                down_col = f"V{i}/phdown"
                up_col = f"V{i}/phup"
                down_val = last.get(down_col, "0") or "0"
                up_val = last.get(up_col, "0") or "0"
                try:
                    counts[key] = {"phDown": int(down_val), "phUp": int(up_val)}
                except ValueError:
                    counts[key] = {"phDown": 0, "phUp": 0}
    except Exception:
        pass
    return counts

# Initiale Zähler laden
loaded_counts = load_adjustment_counts()
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})
for versuch, vals in loaded_counts.items():
    adjustment_count[versuch] = vals

# Puffer für Messwerte (Timestamp, Wert)
ph_history = {
    f"{MQTT_TOPIC_PREFIX}{i}/pH": deque(maxlen=10000)
    for i in range(1, 5)
}
last_action_time = defaultdict(lambda: 0)

# MQTT-Client konfigurieren
client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

# Verbindung: Topics abonnieren und initiale Zähler publishen
 def on_connect(client, userdata, flags, rc):
    print("Verbunden mit MQTT-Broker")
    # Subscribe alle pH-Topics
    for topic in ph_history:
        client.subscribe(topic)
        print(f"Abonniert: {topic}")
    # Publish geladene Zähler
    for versuch, vals in adjustment_count.items():
        client.publish(f"{versuch}/phdown", vals.get("phDown", 0))
        client.publish(f"{versuch}/phpup", vals.get("phUp", 0))
        print(f"Initial publish {versuch}/phdown = {vals.get('phDown',0)}")
        print(f"Initial publish {versuch}/phup = {vals.get('phUp',0)}")

# Nachricht: Wert speichern und ggf. Action auslösen
 def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        ph_value = float(msg.payload.decode())
    except ValueError:
        return

    now = time.time()
    ph_history[topic].append((now, ph_value))

    # Werte im aktuellen Fenster
    recent = [(t, v) for t, v in ph_history[topic] if now - t <= PH_DURATION_THRESHOLD]
    # Prüfen, ob Fenster voll ist
    if not ph_history[topic] or now - ph_history[topic][0][0] < PH_DURATION_THRESHOLD:
        return

    versuch = topic.split("/")[0]
    # Cooldown
    if now - last_action_time[versuch] < PH_COOLDOWN:
        return

    # Durchschnitts-pH berechnen
    values = [v for _, v in recent]
    avg = sum(values) / len(values)
    if avg > PH_HIGH:
        print(f"{versuch}: pH > {PH_HIGH} → phdown")
        run_sequence(versuch, "down")
        adjustment_count[versuch]["phDown"] += 1
        publish_adjustment_count(versuch, "down")
        last_action_time[versuch] = now
    elif avg < PH_LOW:
        print(f"{versuch}: pH < {PH_LOW} → phup")
        run_sequence(versuch, "up")
        adjustment_count[versuch]["phUp"] += 1
        publish_adjustment_count(versuch, "up")
        last_action_time[versuch] = now

# Ausführungssequenz für Ventil und pH-Regulierung
 def run_sequence(versuch, direction="down"):
    valve_open = f"{versuch.lower()}valveopen"
    valve_close = f"{versuch.lower()}valveclose"
    client.publish(COMMAND_TOPIC, valve_open)
    client.loop_write()
    time.sleep(5)

    if direction == "down":
        for _ in range(2):
            client.publish(COMMAND_TOPIC, "phdown")
            client.loop_write()
            time.sleep(5)
    else:
        client.publish(COMMAND_TOPIC, "phup")
        client.loop_write()
        time.sleep(5)

    client.publish(COMMAND_TOPIC, "water")
    client.loop_write()
    time.sleep(15)
    client.publish(COMMAND_TOPIC, valve_close)
    client.loop_write()

# Zähler publizieren
 def publish_adjustment_count(versuch, direction):
    subtopic = f"{versuch}/ph{direction}"
    key = f"ph{direction.capitalize()}"
    count = adjustment_count[versuch][key]
    client.publish(subtopic, count)
    print(f"{subtopic}: {count}")

# MQTT-Handlers registrieren und Client starten
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()