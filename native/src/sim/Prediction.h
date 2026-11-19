// Port of src/sim/Prediction.ts.
#pragma once
#include <optional>

#include "sim/BallPhysics.h"

namespace sim {

struct PlaneCrossing {
    double x, y;
    double t;  // seconds from now until the ball reaches the plane
};

std::optional<PlaneCrossing> predictAtPlane(const V3& pos, const V3& vel, const V3& spin, const PhysicsParams& P,
                                            double planeZ, const V3& wind = {}, double maxT = 3);
std::optional<PlaneCrossing> predictAtDistance(const V3& pos, const V3& vel, const V3& spin, const PhysicsParams& P,
                                               const V3& from, double dist, const V3& wind = {}, double maxT = 2);

}  // namespace sim
