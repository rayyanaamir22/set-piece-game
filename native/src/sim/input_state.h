// Port of InputState / emptyInput from src/sim/Sim.ts. Plain data, no dependencies.
#pragma once

namespace sim {

struct InputState {
    // Reticle velocity axes, -1..1 (+x right, +y up).
    double aimX = 0, aimY = 0;
    // Absolute contact point on the ball, unit disc.
    double contactX = 0, contactY = 0;
    bool shoot = false;
    bool driven = false;
    bool low = false;
    bool toggleFoot = false;
    bool retry = false;
    bool newSpot = false;
    // Run-up arc angle / distance axes, -1..1 (wider/longer = +1).
    double runupAngle = 0, runupDist = 0;
    // Mouse: reticle movement in metres this frame (applied once, not per sim step).
    double aimDeltaX = 0, aimDeltaY = 0;
    // Hold at the release to send a dummy runner over the ball first.
    bool dummy = false;
};

inline InputState emptyInput() { return {}; }

}  // namespace sim
