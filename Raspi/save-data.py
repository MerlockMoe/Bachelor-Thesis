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

        self.latest_messages = {}  # Speichert letzte Nachricht je Topic
        self.topic_list = set()

        # Verzeichnis für Logdateien definieren
        self.log_dir = os.path.expanduser("~/logs")  # Pfad anpassen
        os.makedirs(self.log_dir, exist_ok=True)

        # Dateiname basierend auf aktuellem Tag
        self.date_str = datetime.now().strftime('%Y%m%d')
        self.filename = os.path.join(self.log_dir, f"mqtt_log_{self.date_str}.csv")

        # Datei anlegen mit Header, falls sie noch nicht existiert
        if not os.path.isfile(self.filename):
            with open(self.filename, mode='w', newline='') as csv_file:
                pass  # Header wird beim ersten Logeintrag dynamisch geschrieben

        self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
        self.client.loop_start()

        # Timer zum regelmäßigen Schreiben in CSV
        self.start_csv_timer()

    def on_connect(self, client, userdata, flags, rc):
        print("Verbunden mit MQTT-Broker. Starte Wildcard-Abonnement.")
        self.client.subscribe("#")  # Alle Topics abonnieren

    def on_message(self, client, userdata, msg):
        print(f"Nachricht empfangen: Topic = {msg.topic}, Payload = {msg.payload}")
        topic = msg.topic
        payload = msg.payload.decode(errors='replace')
        self.latest_messages[topic] = payload
        self.topic_list.add(topic)

    def write_to_csv(self):
        print("write_to_csv wird aufgerufen")  # Debugging-Ausgabe
        if not self.latest_messages:
            print("Keine Nachrichten zum Schreiben vorhanden.")
        else:
            is_new_file = not os.path.isfile(self.filename) or os.stat(self.filename).st_size == 0
            timestamp_id = datetime.now().strftime('%d-%H-%M')

            # Filtere Topics und entferne alle "waterLow"-Daten direkt beim Speichern
            filtered_topics = [topic for topic in self.topic_list if "waterLow" not in topic]
            filtered_messages = {topic: self.latest_messages.get(topic, "") for topic in filtered_topics}

            # Sicherstellen, dass Spaltenreihenfolge konsistent bleibt
            filtered_topics = sorted(filtered_topics)

            with open(self.filename, mode='a', newline='') as csv_file:
                writer = csv.writer(csv_file)

                # Header wird nur einmal geschrieben, wenn die Datei neu ist
                if is_new_file:
                    header = ["id"] + filtered_topics
                    writer.writerow(header)

                row = [timestamp_id] + [filtered_messages.get(topic, "") for topic in filtered_topics]
                writer.writerow(row)

            print(f"Messreihe um {timestamp_id} in {self.filename} gespeichert.")

        self.start_csv_timer()  # Timer neu starten

    def start_csv_timer(self):
        Timer(10.0, self.write_to_csv).start()  # Alle 10 Sekunden

def main():
    logger = MqttLogger()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Beende MQTT-Logger.")

if __name__ == '__main__':
    main()
