import time
import csv
import os
from datetime import datetime
import paho.mqtt.client as mqtt
from threading import Timer

MQTT_BROKER = "192.168.0.120"
MQTT_PORT = 1883
MQTT_USER = "joe"
MQTT_PASSWORD = "1337"

class MqttLogger:
    def __init__(self):
        self.client = mqtt.Client()
        self.client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

        self.latest_messages = {}   # Speichert letzte Nachricht je Topic
        self.topic_list = set()

        self.log_dir = os.path.expanduser("~/logs")
        os.makedirs(self.log_dir, exist_ok=True)

        self.date_str = datetime.now().strftime('%Y%m%d')
        self.filename = os.path.join(self.log_dir, f"mqtt_log_{self.date_str}.csv")

        if not os.path.isfile(self.filename):
            with open(self.filename, mode='w', newline='') as csv_file:
                pass  # Header wird beim ersten Logeintrag dynamisch geschrieben

        self.capture_topics = [
            "esp32/camV1/cmd",
            "esp32/camV2/cmd",
            "esp32/camV3/cmd",
            "esp32/camV4/cmd"
        ]

        self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
        self.client.loop_start()

        self.start_csv_timer()

    def on_connect(self, client, userdata, flags, rc):
        print("Verbunden mit MQTT-Broker. Starte Wildcard-Abonnement.")
        self.client.subscribe("#")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode(errors='replace')
        self.latest_messages[topic] = payload
        self.topic_list.add(topic)

    def publish_to(self, topic: str):
        self.client.publish(topic, "capture")
        print(f"Nachricht 'capture' gesendet an {topic}")

    def send_capture(self):
        for idx, topic in enumerate(self.capture_topics):
            Timer(2 * idx, self.publish_to, args=(topic,)).start()

    def write_to_csv(self):
        print("write_to_csv wird aufgerufen")
        if not self.latest_messages:
            print("Keine Nachrichten zum Schreiben vorhanden.")
        else:
            is_new = not os.path.isfile(self.filename) or os.stat(self.filename).st_size == 0
            timestamp_id = datetime.now().strftime('%m-%d-%H-%M')

            # Nur Topics, die mit V1/, V2/, V3/ oder V4/ beginnen; ohne "waterLow"
            filtered_topics = [
                t for t in self.topic_list
                if any(t.startswith(f"V{n}/") for n in range(1,5))
                   and "waterLow" not in t
            ]
            filtered_topics.sort()

            filtered_messages = {t: self.latest_messages.get(t, "") for t in filtered_topics}

            with open(self.filename, mode='a', newline='') as csv_file:
                writer = csv.writer(csv_file)
                if is_new:
                    header = ["id"] + filtered_topics
                    writer.writerow(header)
                row = [timestamp_id] + [filtered_messages[t] for t in filtered_topics]
                writer.writerow(row)

            print(f"Messreihe um {timestamp_id} in {self.filename} gespeichert.")
            self.send_capture()

        self.start_csv_timer()

    def start_csv_timer(self):
        Timer(300.0, self.write_to_csv).start()

def main():
    logger = MqttLogger()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Beende MQTT-Logger.")

if __name__ == '__main__':
    main()
