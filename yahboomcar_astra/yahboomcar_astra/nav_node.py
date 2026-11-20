#imports
import rclpy
from rclpy.node import Node
import math

from enum import Enum, auto
from sensor_msgs.msg import LaserScan
from std_msgs.msg import String, Int32
from dataclasses import dataclass
from airport_interfaces.msg import DetectedObjectsList, Direction, DirectionOptions, TowerInstructions, TowerInstruction



@dataclass
class Sign:
    sign_type: str
    sign_label: str
    sign_direction: str | None

    @classmethod
    def from_string(cls, name_string):
        parts = name_string.lower().split("_", 2)

        try:
            sign_type, sign_label, sign_direction = parts
        except ValueError:
            sign_type, sign_label = parts
            sign_direction = None

        return cls(
            sign_type=sign_type,
            sign_label=sign_label,
            sign_direction=sign_direction
        )


class NavNode(Node):
    def __init__(self):
        super().__init__("nav_node")

        self.instructions = []
        self.options = []
        self.available_signs = []
        self.next_sign = None


        # self.sub_to_tower = self.create_subscription(  #sends a list of tuples [(taxiway A, LEFT), (taxiway B, RIGHT)]
        #     TowerInstructions,
        #     'InstructionTopic',
        #     self.tower_callback,
        #     10
        # )

        self.sub_to_CV = self.create_subscription(
            DetectedObjectsList,
            "/detected_objects",
            self.CV_callback,
            10,
        )

        self.sub_to_line_follow = self.create_subscription(
            DirectionOptions,
            "/turn_options",
            self.line_follow_callback,
            10
        )

        self.line_follow_instructions = self.create_publisher(
            Direction,
            '/turn_decision',
            10
            )


      
    #This is literally to grab the instructions from the tower and put them into a global var
    # def tower_callback(self, msg):
    #     self.instructions_raw = list(msg.data) #list() is used so that this file has a separate copy to change however
    #     self.instructions = [Sign.from_string(instruction) for instruction in self.instructions_raw]
    #     self.next_sign = self.instructions[0]

    def CV_callback(self, msg):
        # Start with an empty list for this set of detections.
        detected_signs = []

        for detection in msg.objects:
            class_name = detection.class_name
            confidence = detection.confidence

            self.get_logger().info(
                f"Detected: {class_name}, confidence: {confidence:.2f}"
            )

            detected_signs.append(class_name)

        # Replace the old detections with the newest ones.
        self.available_signs = detected_signs


    def line_follow_callback(self, msg):
        # Each item in msg.directions is a Direction message.
        # Store the numeric direction from each one.
        self.options = [
            option.direction for option in msg.directions
        ]

        self.BDT()

    def BDT(self):
        # Without an instruction, there is no target sign to look for.
        if not self.instructions:
            return

        direction_msg = Direction()

        # Default to straight until a matching sign and turn are found.
        direction_msg.direction = Direction.STRAIGHT

        # Example: ("taxiway_a", Direction.LEFT)
        target_sign_name, tower_direction = self.instructions[0]

        target_sign = Sign.from_string(target_sign_name)

        # Convert the directions in sign names into numeric values.
        sign_direction_values = {
            "left": Direction.LEFT,
            "right": Direction.RIGHT,
            "straight": Direction.STRAIGHT,
        }

        for sign_name in self.available_signs:
            available_sign = Sign.from_string(sign_name)

            # Is this the sign the tower wants us to find?
            if (available_sign.sign_type == target_sign.sign_type and available_sign.sign_label == target_sign.sign_label):
                # Prefer the tower direction if one was provided.
                if tower_direction is not None:
                    chosen_direction = tower_direction
                else:
                    # The sign direction is a string, so convert it.
                    # Returns None if it is missing or unrecognized.
                    chosen_direction = sign_direction_values.get(available_sign.sign_direction)

                # Only use the direction if line following reports
                # that it is available at the intersection.
                if (chosen_direction is not None and chosen_direction in self.options):
                    direction_msg.direction = chosen_direction                    
                    self.instructions.pop(0)  # Remove the completed instruction.

                break

        self.line_follow_instructions.publish(direction_msg)



def main(args=None):
    rclpy.init(args=args)

    node = NavNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

                
