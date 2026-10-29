import math

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan
from std_msgs.msg import Int32


class PointCounter(Node):
    def __init__(self):
        super().__init__('point_counter')
        self.sub = self.create_subscription(
            LaserScan, 'scan', self.on_scan, qos_profile_sensor_data)
        self.pub = self.create_publisher(Int32, 'scan_point_count', 10)

    def on_scan(self, msg):
        # A point is valid if it's a real number inside the lidar's range.
        # Misses come through as inf, and bad readings as 0 or nan.
        count = sum(
            1 for r in msg.ranges
            if math.isfinite(r) and msg.range_min <= r <= msg.range_max
        )
        self.pub.publish(Int32(data=count))
        self.get_logger().info(
            f'{count} of {len(msg.ranges)} points valid',
            throttle_duration_sec=1.0)


def main():
    rclpy.init()
    node = PointCounter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()


if __name__ == '__main__':
    main()
