// JavaScript-compatible number helpers. The TypeScript sim relies on JS semantics for
// Math.min/max (NaN propagates), Math.hypot (V8's scaled, compensated sum), Math.round and Math.sign.
// Keeping them identical keeps the port bit-close to the reference.
#pragma once
#include <cmath>
#include <limits>

namespace sim {

inline constexpr double INF = std::numeric_limits<double>::infinity();
inline constexpr double PI = 3.14159265358979323846;

inline double jmax(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    return a > b ? a : b;
}
inline double jmin(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    return a < b ? a : b;
}
inline double jround(double x) { return std::floor(x + 0.5); }
inline double jsign(double x) { return x > 0 ? 1.0 : (x < 0 ? -1.0 : x); }

// V8's Math.hypot: divide by the max magnitude and Kahan-sum the squares.
inline double jhypot(double a, double b, double c) {
    const double v[3] = {a, b, c};
    double mx = 0;
    bool nan = false;
    for (double x : v) {
        if (std::isinf(x)) return INF;
        if (std::isnan(x)) nan = true;
        else mx = std::fmax(mx, std::fabs(x));
    }
    if (nan) return std::numeric_limits<double>::quiet_NaN();
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double x : v) {
        const double n = std::fabs(x) / mx;
        const double summand = n * n - comp;
        const double prelim = sum + summand;
        comp = (prelim - sum) - summand;
        sum = prelim;
    }
    return std::sqrt(sum) * mx;
}
inline double jhypot(double a, double b) {
    const double v[2] = {a, b};
    double mx = 0;
    bool nan = false;
    for (double x : v) {
        if (std::isinf(x)) return INF;
        if (std::isnan(x)) nan = true;
        else mx = std::fmax(mx, std::fabs(x));
    }
    if (nan) return std::numeric_limits<double>::quiet_NaN();
    if (mx == 0) return 0;
    double sum = 0, comp = 0;
    for (double x : v) {
        const double n = std::fabs(x) / mx;
        const double summand = n * n - comp;
        const double prelim = sum + summand;
        comp = (prelim - sum) - summand;
        sum = prelim;
    }
    return std::sqrt(sum) * mx;
}

}  // namespace sim
