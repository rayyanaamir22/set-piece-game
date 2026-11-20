// Port of src/sim/math/vec3.ts. double everywhere, same operation order.
#pragma once
#include "sim/math/jsmath.h"

namespace sim {

struct V3 {
    double x = 0, y = 0, z = 0;
};

inline V3 v3(double x = 0, double y = 0, double z = 0) { return {x, y, z}; }
inline V3 add(const V3& a, const V3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 sub(const V3& a, const V3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 mul(const V3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double len(const V3& a) { return jhypot(a.x, a.y, a.z); }
inline V3 norm(const V3& a) {
    const double l = len(a);
    return l < 1e-12 ? v3() : mul(a, 1 / l);
}

inline double clamp(double x, double lo, double hi) { return jmin(hi, jmax(lo, x)); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double smoothstep(double e0, double e1, double x) {
    const double t = clamp((x - e0) / (e1 - e0), 0, 1);
    return t * t * (3 - 2 * t);
}

}  // namespace sim
