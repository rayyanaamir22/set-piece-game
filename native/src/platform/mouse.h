// Port of src/input/mouse.ts.
#pragma once
#include <algorithm>
#include <cmath>

#include "sim/math/jsmath.h"

namespace platform {

inline constexpr double AIM_M_PER_PX = 0.012;      // metres of reticle movement per pixel at sensitivity 1
inline constexpr double CONTACT_PER_PX = 0.006;    // contact-point units (unit disc) per pixel at sensitivity 1
inline constexpr double SENS_MIN = 0.2, SENS_MAX = 3;

inline double clampSens(double s) { return std::isfinite(s) ? std::min(SENS_MAX, std::max(SENS_MIN, s)) : 1.0; }

struct Vec2 {
    double x, y;
};

// Mouse movement (pixels; y grows downward) -> reticle movement in metres (y grows upward).
inline Vec2 aimDeltaFromMouse(double dx, double dy, double sens) {
    const double k = AIM_M_PER_PX * clampSens(sens);
    return {dx * k, -dy * k};
}

// Move the contact-point cursor by a mouse delta, keeping it inside the unit disc.
inline Vec2 moveContactCursor(Vec2 cur, double dx, double dy, double sens) {
    const double k = CONTACT_PER_PX * clampSens(sens);
    double x = cur.x + dx * k, y = cur.y - dy * k;
    const double m = sim::jhypot(x, y);
    if (m > 1) {
        x /= m;
        y /= m;
    }
    return {x, y};
}

}  // namespace platform
