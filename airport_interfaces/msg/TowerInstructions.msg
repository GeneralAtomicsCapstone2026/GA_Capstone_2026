# This message defines the list of instructions the ground station or
# controller wil send to the robot that contains the route. 

TowerInstruction[] instructions

# Example usage:
# 
# from airport_interfaces.msg import (
#     SingleDirection,
#     TowerInstruction,
#     TowerInstructions,
# )
# 
# msg = TowerInstructions()
# 
# msg.instructions = [
#     TowerInstruction(
#         location="Taxiway A",
#         direction=SingleDirection(direction=SingleDirection.LEFT)
#     ),
#     TowerInstruction(
#         location="Runway 2",
#         direction=SingleDirection(direction=SingleDirection.RIGHT)
#     ),
# ]