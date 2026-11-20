#include "sim/layout.h"

#include <cmath>

namespace sim {

V3 dirToGoal(const V3& spot) { return norm({-spot.x, 0, -spot.z}); }
V3 rightOf(const V3& dir) { return {-dir.z, 0, dir.x}; }

KickerPositions kickerPositions(const V3& spot, char foot, double angle, double dist) {
    const V3 d = dirToGoal(spot);
    const V3 r = rightOf(d);
    const double side = foot == 'R' ? -1 : 1;
    const V3 strike = add(add(spot, mul(r, side * 0.8)), mul(d, -0.55));
    const double a = (angle * PI) / 180;
    V3 start = add(add(spot, mul(d, -std::cos(a) * dist)), mul(r, side * std::sin(a) * dist));
    start.y = 0;
    V3 st = strike;
    st.y = 0;
    return {start, st};
}

}  // namespace sim
