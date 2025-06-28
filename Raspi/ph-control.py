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
PH_DURATION_THRESHOLD = 10 * 60  # Fenstergröße in Sekunden, über die gemittelt wird
PH_COOLDOWN = 60 * 60            # Cooldown in Sekunden nach einer Anpassung

# Speichert (Timestamp, Wert)-Paar im Zeitfenster
ph_history = {
    f"{MQTT_TOPIC_PREFIX}{i}/pH": deque(maxlen=10000)  # Puffer groß genug, um alle Messungen aufzunehmen
    for i in range(1, 5)
}
last_action_time = defaultdict(lambda: 0)
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

def on_connect(client, userdata, flags, rc):
    print("Verbunden mit MQTT-Broker")
    for topic in ph_history.keys():
        client.subscribe(topic)
        print(f"Abonniert: {topic}")

def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        ph_value = float(msg.payload.decode())
    except ValueError:
        return  # Ungültiger Wert

    now = time.time()
    ph_history[topic].append((now, ph_value))

    # Einträge im aktuellen Zeitfenster
    recent = [(t, v) for t, v in ph_history[topic] if now - t <= PH_DURATION_THRESHOLD]

    # Prüfen, ob wir volle Fenstergröße abgedeckt haben
    earliest_time = ph_history[topic][0][0] if ph_history[topic] else now
    if now - earliest_time < PH_DURATION_THRESHOLD:
        return  # Noch nicht genug Daten gesammelt

    # Cooldown prüfen
    versuch = topic.split("/")[0]
    if now - last_action_time[versuch] < PH_COOLDOWN:
        return

    # Durchschnitt berechnen und ggf. anpassen
    values = [v for t, v in recent]
    avg = sum(values) / len(values)
    if avg > PH_HIGH:
        print(f"{versuch}: pH > {PH_HIGH} im Fenster → phdown")
        run_sequence(versuch, direction="down")
        adjustment_count[versuch]["phDown"] += 1
        publish_adjustment_count(versuch, "down")
        last_action_time[versuch] = now
    elif avg < PH_LOW:
        print(f"{versuch}: pH < {PH_LOW} im Fenster → phup")
        run_sequence(versuch, direction="up")
        adjustment_count[versuch]["phUp"] += 1
        publish_adjustment_count(versuch, "up")
        last_action_time[versuch] = now

def run_sequence(versuch, direction="down"):
    valve_open = f"{versuch.lower()}valveopen"
    valve_close = f"{versuch.lower()}valveclose"
    client.publish(COMMAND_TOPIC, valve_open)
    client.loop_write()
    time.sleep(5)

    # phdown zweimal, phup einmal
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

def publish_adjustment_count(versuch, direction):
    subtopic = f"{versuch}/ph{direction}"
    key = f"ph{direction.capitalize()}"
    count = adjustment_count[versuch][key]
    client.publish(subtopic, count)
    print(f"{subtopic}: {count}")

client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
