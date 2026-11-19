// Port of src/sim/layout.ts: taker / run-up geometry.
#pragma once
#include "sim/math/vec3.h"

namespace sim {

V3 dirToGoal(const V3& spot);
V3 rightOf(const V3& dir);

struct KickerPositions {
    V3 runupStart, strike;
};
KickerPositions kickerPositions(const V3& spot, char foot, double angle = 35, double dist = 3.5);

}  // namespace sim
