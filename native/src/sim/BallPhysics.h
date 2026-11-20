// Port of src/sim/BallPhysics.ts.
#pragma once
#include <array>

#include "sim/math/vec3.h"

namespace sim {

struct PhysicsParams {
    double mass, radius, gravity, airDensity, cdHigh, cdLow, crisisStart, crisisEnd, clGain, clMax, spinDecay,
        restitution, friction, inertiaFactor, rollingResistance, knuckleAccel, knuckleSpeedMin, knuckleSpeedFull,
        postRestitution, netRestitution, netDrag;
};

struct Knuckle {
    double amp = 0;  // peak lateral acceleration (m/s^2); 0 disables the wobble
    std::array<double, 6> phase{};
};

struct BallState {
    V3 pos, vel;
    V3 spin;  // angular velocity (rad/s), world frame
    double t = 0;
    Knuckle knuckle;
    V3 wind;  // air velocity (m/s)
};

BallState newBall(const V3& pos);
double dragCoefficient(double speed, const PhysicsParams& P);
V3 acceleration(const V3& vel, const V3& spin, double t, const Knuckle& k, const PhysicsParams& P, const V3& wind);
void stepBall(BallState& b, const PhysicsParams& P, double dt);
// Returns the impact speed of a bounce, or 0 when rolling / airborne.
double groundContact(BallState& b, const PhysicsParams& P, double dt);

}  // namespace sim
