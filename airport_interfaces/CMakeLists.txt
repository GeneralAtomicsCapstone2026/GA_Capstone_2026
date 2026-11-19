cmake_minimum_required(VERSION 3.8)
project(airport_interfaces)

# Find required ROS 2 packages
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

# Generate custom message types
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/DetectedObject.msg"
  "msg/DetectedObjectsList.msg"
  "msg/Direction.msg"
  "msg/DirectionOptions.msg"
  "msg/TowerInstruction.msg"
  "msg/TowerInstructions.msg"
  DEPENDENCIES std_msgs
)

ament_package()