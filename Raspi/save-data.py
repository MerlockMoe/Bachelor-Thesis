import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
import csv
from datetime import datetime
from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message

class Ros2MessageLogger(Node):
    def __init__(self):
        super().__init__('ros2_message_logger')
        self.subscription_callbacks = {}
        self.received_messages = []
        self.start_subscription_to_all_topics()

        # Timer für das Schreiben in die CSV-Datei
        self.create_timer(60.0, self.write_to_csv)

    def start_subscription_to_all_topics(self):
        topic_names_and_types = self.get_topic_names_and_types()
        for topic_name, msg_types in topic_names_and_types:
            msg_type = msg_types[0]  # Nehme den ersten Typ, falls mehrere vorhanden sind
            try:
                msg_class = get_message(msg_type)
                self.create_subscription_to_topic(topic_name, msg_class)
            except Exception as e:
                self.get_logger().error(f"Fehler beim Abonnieren von {topic_name}: {e}")

    def create_subscription_to_topic(self, topic_name, msg_class):
        def callback(msg):
            timestamp = self.get_clock().now().to_msg()
            self.received_messages.append({
                "timestamp": datetime.now().isoformat(),
                "topic": topic_name,
                "message": str(msg)
            })
        qos_profile = QoSProfile(depth=10)
        self.subscription_callbacks[topic_name] = self.create_subscription(
            msg_class,
            topic_name,
            callback,
            qos_profile
        )
        self.get_logger().info(f"Abonniere Topic: {topic_name}")

    def write_to_csv(self):
        if not self.received_messages:
            self.get_logger().info("Keine Nachrichten zum Schreiben vorhanden.")
            return
        filename = f"ros_messages_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        with open(filename, mode='w', newline='') as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=["timestamp", "topic", "message"])
            writer.writeheader()
            writer.writerows(self.received_messages)
        self.get_logger().info(f"Nachrichten in {filename} gespeichert.")
        self.received_messages = []


def main(args=None):
    rclpy.init(args=args)
    logger_node = Ros2MessageLogger()
    try:
        rclpy.spin(logger_node)
    except KeyboardInterrupt:
        logger_node.get_logger().info('Knoten wird beendet.')
    finally:
        logger_node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
