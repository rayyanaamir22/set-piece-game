// Port of src/sim/ShotModel.ts.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "sim/BallPhysics.h"
#include "sim/math/rng.h"

namespace sim {

struct ShotParams {
    double vMin, vMax, chargeTime, sweetMin, sweetMax, spinSide, spinVert, knuckleRadius, knuckleMinPower, knuckleSpin,
        baseError, missErrorGain, overpowerLift, drivenSpeed, drivenSpin, lowTargetY, lowTopspin, lowSpeed,
        lowElevation, solverIterations, runupTime, dummyDelay, runupAngleDefault, runupAngleMax, runupDistDefault,
        runupDistMin, runupDistMax, runupAngleRate, runupDistRate, powerStraight, powerWide, curlStraight, curlWide,
        distPower, knuckleMaxAngle, aimSpeedX, aimSpeedY, resultTime;
};

struct Taker {
    std::string name;
    std::string style;
    char foot = 'R';  // 'R' | 'L'
    double accuracy = 0, curve = 0, power = 1;
};

struct XY {
    double x = 0, y = 0;
};

struct RunupSetting {
    double angle, dist;
};

struct ShotRequest {
    V3 spot;
    XY target;
    XY contact;
    double power = 0;
    bool driven = false, low = false;
    Taker taker;
    std::optional<RunupSetting> runup;  // omitted = the neutral default
    std::optional<V3> wind;             // only used to draw the predicted-path aid
    RngState* rng = nullptr;            // null for a perfect, deterministic shot (aim preview)
};

struct Shot {
    V3 vel, spin;
    double speed = 0;
    double azimuth = 0, elevation = 0;
    bool knuckle = false;
    std::array<double, 6> knucklePhase{};
    XY aimed, error;
    std::optional<std::vector<V3>> path;
};

struct RunupEffect {
    double power, curl;
    bool knuckleOk;
};

RunupEffect runupEffect(const std::optional<RunupSetting>& runup, const ShotParams& SP);
double launchSpeed(double power, bool driven, bool low, const Taker& taker, const ShotParams& SP, double runupPower = 1);
Shot computeShot(const ShotRequest& req, const PhysicsParams& P, const ShotParams& SP, bool wantPath = false);

}  // namespace sim
