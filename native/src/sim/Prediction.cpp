#include "sim/Prediction.h"

namespace sim {

std::optional<PlaneCrossing> predictAtPlane(const V3& pos, const V3& vel, const V3& spin, const PhysicsParams& P,
                                            double planeZ, const V3& wind, double maxT) {
    BallState b = newBall(pos);
    b.vel = vel;
    b.spin = spin;
    b.wind = wind;
    const double dt = 1.0 / 120;
    while (b.t < maxT) {
        const V3 prev = b.pos;
        stepBall(b, P, dt);
        groundContact(b, P, dt);
        if (b.pos.z <= planeZ) {
            const double f = prev.z == b.pos.z ? 0 : (prev.z - planeZ) / (prev.z - b.pos.z);
            return PlaneCrossing{prev.x + (b.pos.x - prev.x) * f, prev.y + (b.pos.y - prev.y) * f, b.t - dt * (1 - f)};
        }
    }
    return std::nullopt;
}

std::optional<PlaneCrossing> predictAtDistance(const V3& pos, const V3& vel, const V3& spin, const PhysicsParams& P,
                                               const V3& from, double dist, const V3& wind, double maxT) {
    BallState b = newBall(pos);
    b.vel = vel;
    b.spin = spin;
    b.wind = wind;
    const double dt = 1.0 / 120;
    while (b.t < maxT) {
        stepBall(b, P, dt);
        groundContact(b, P, dt);
        if (jhypot(b.pos.x - from.x, b.pos.z - from.z) >= dist) return PlaneCrossing{b.pos.x, b.pos.y, b.t};
    }
    return std::nullopt;
}

}  // namespace sim
