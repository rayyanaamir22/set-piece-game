// Port of src/sim/constants.ts. SI units; origin = centre of the goal line on the ground.
//   +x = right as seen from the pitch looking at goal, +y = up, +z = from the goal line out into the pitch.
#pragma once

namespace sim {

inline constexpr double GOAL_HALF_WIDTH = 3.66;
inline constexpr double GOAL_HEIGHT = 2.44;
inline constexpr double GOAL_DEPTH = 2.0;
inline constexpr double POST_RADIUS = 0.06;

inline constexpr double PITCH_HALF_WIDTH = 34;
inline constexpr double PENALTY_BOX_Z = 16.5;
inline constexpr double PENALTY_BOX_HALF = 20.16;
inline constexpr double SIX_YARD_Z = 5.5;
inline constexpr double SIX_YARD_HALF = 9.16;
inline constexpr double PENALTY_SPOT_Z = 11;
inline constexpr double CENTRE_ARC_RADIUS = 9.15;

inline constexpr double WALL_DISTANCE = 9.15;

}  // namespace sim
