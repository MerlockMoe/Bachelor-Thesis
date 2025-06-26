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
PH_HIGH = 6
PH_LOW = 5.5
PH_DURATION_THRESHOLD = 2 * 60  # 60 Minuten in Sekunden
PH_COOLDOWN = 5 * 60  # 60 Minuten in Sekunden

# Speichert pH-Verlauf je Versuch
ph_history = {
    f"{MQTT_TOPIC_PREFIX}{i}/pH": deque(maxlen=PH_DURATION_THRESHOLD)
    for i in range(1, 5)
}
last_action_time = defaultdict(lambda: 0)
adjustment_count = defaultdict(lambda: {"phDown": 0, "phUp": 0})  # Zähler für Anpassungen

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
        return

    now = time.time()
    ph_history[topic].append((now, ph_value))
    recent = [v for t, v in ph_history[topic] if now - t <= PH_DURATION_THRESHOLD]

    versuch = topic.split("/")[0]  # z.B. V1
    if now - last_action_time[versuch] < PH_COOLDOWN:
        return  # Cooldown aktiv

    if len(recent) >= PH_DURATION_THRESHOLD:
        avg = sum(recent) / len(recent)
        if avg > PH_HIGH:
            print(f"{versuch}: pH > {PH_HIGH} seit 60min → sende phdown-Sequenz")
            run_sequence(versuch, direction="down")
            last_action_time[versuch] = now
            adjustment_count[versuch]["phDown"] += 1
            publish_adjustment_count(versuch, "down")
        elif avg < PH_LOW:
            print(f"{versuch}: pH < {PH_LOW} seit 60min → sende phup-Sequenz")
            run_sequence(versuch, direction="up")
            last_action_time[versuch] = now
            adjustment_count[versuch]["phUp"] += 1
            publish_adjustment_count(versuch, "up")

def run_sequence(versuch, direction="down"):
    valve_open = f"{versuch.lower()}valveopen"
    valve_close = f"{versuch.lower()}valveclose"
    client.publish(COMMAND_TOPIC, valve_open)
    client.loop_write()
    time.sleep(5)

    if direction == "down":
        client.publish(COMMAND_TOPIC, "phdown")
        time.sleep(3)
        client.publish(COMMAND_TOPIC, "phdown")
    else:
        client.publish(COMMAND_TOPIC, "phup")
        time.sleep(3)
        client.publish(COMMAND_TOPIC, "phup")
    client.loop_write()
    time.sleep(5)

    client.publish(COMMAND_TOPIC, "water")
    client.loop_write()
    time.sleep(15)

    client.publish(COMMAND_TOPIC, valve_close)
    client.loop_write()

def publish_adjustment_count(versuch, direction):
    """Sendet den Zähler für Anpassungen an das jeweilige Sub-Topic."""
    subtopic = f"{versuch}/ph{direction}"
    key = f"ph{direction.capitalize()}"
    count = adjustment_count[versuch][key]
    client.publish(subtopic, count)
    print(f"{subtopic}: Anpassungszähler = {count}")

client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
