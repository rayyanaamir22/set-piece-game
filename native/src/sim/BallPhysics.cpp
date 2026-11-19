#include "sim/BallPhysics.h"

#include <cmath>

namespace sim {

BallState newBall(const V3& pos) {
    BallState b;
    b.pos = pos;
    return b;
}

double dragCoefficient(double speed, const PhysicsParams& P) {
    return P.cdHigh + (P.cdLow - P.cdHigh) * smoothstep(P.crisisStart, P.crisisEnd, speed);
}

namespace {
constexpr double TAU = PI * 2;
constexpr double KNUCKLE_FREQ[3] = {1.3, 2.9, 4.7};

double knuckleNoise(double t, const std::array<double, 6>& ph, int offset) {
    double n = 0;
    n += std::sin(TAU * KNUCKLE_FREQ[0] * t + ph[offset]);
    n += 0.7 * std::sin(TAU * KNUCKLE_FREQ[1] * t + ph[offset + 1]);
    n += 0.4 * std::sin(TAU * KNUCKLE_FREQ[2] * t + ph[offset + 2]);
    return n / 2.1;
}
}  // namespace

V3 acceleration(const V3& vel, const V3& spin, double t, const Knuckle& k, const PhysicsParams& P, const V3& wind) {
    double ax = 0;
    double ay = -P.gravity;
    double az = 0;
    // Velocity relative to the air.
    const V3 rel = {vel.x - wind.x, vel.y - wind.y, vel.z - wind.z};
    const double speed = len(rel);
    if (speed > 1e-6) {
        const double area = PI * P.radius * P.radius;
        const double kd = (0.5 * P.airDensity * dragCoefficient(speed, P) * area * speed) / P.mass;
        ax -= kd * rel.x;
        ay -= kd * rel.y;
        az -= kd * rel.z;

        const V3 w = cross(spin, rel);
        const double wl = len(w);
        if (wl > 1e-9) {
            const double spinPerp = wl / speed;
            const double S = (P.radius * spinPerp) / speed;
            const double cl = jmin(P.clMax, P.clGain * S);
            const double mag = (0.5 * P.airDensity * area * cl * speed * speed) / P.mass;
            ax += (w.x / wl) * mag;
            ay += (w.y / wl) * mag;
            az += (w.z / wl) * mag;
        }

        if (k.amp > 0) {
            const double fade = smoothstep(P.knuckleSpeedMin, P.knuckleSpeedFull, speed);
            const V3 vhat = mul(rel, 1 / speed);
            V3 b1 = norm(cross(vhat, v3(0, 1, 0)));
            if (len(b1) < 0.5) b1 = v3(1, 0, 0);
            const V3 b2 = cross(b1, vhat);
            const double na = knuckleNoise(t, k.phase, 0) * k.amp * fade;
            const double nb = knuckleNoise(t, k.phase, 3) * k.amp * fade;
            ax += b1.x * na + b2.x * nb;
            ay += b1.y * na + b2.y * nb;
            az += b1.z * na + b2.z * nb;
        }
    }
    return {ax, ay, az};
}

void stepBall(BallState& b, const PhysicsParams& P, double dt) {
    const double h = dt / 2;
    const V3 v0 = b.vel;
    const V3 a1 = acceleration(v0, b.spin, b.t, b.knuckle, P, b.wind);
    const V3 v1 = add(v0, mul(a1, h));
    const V3 a2 = acceleration(v1, b.spin, b.t + h, b.knuckle, P, b.wind);
    const V3 v2 = add(v0, mul(a2, h));
    const V3 a3 = acceleration(v2, b.spin, b.t + h, b.knuckle, P, b.wind);
    const V3 v3_ = add(v0, mul(a3, dt));
    const V3 a4 = acceleration(v3_, b.spin, b.t + dt, b.knuckle, P, b.wind);

    const V3 dp = mul(add(add(v0, v3_), mul(add(v1, v2), 2)), dt / 6);
    const V3 dv = mul(add(add(a1, a4), mul(add(a2, a3), 2)), dt / 6);
    b.pos = add(b.pos, dp);
    b.vel = add(b.vel, dv);
    b.spin = mul(b.spin, std::exp(-P.spinDecay * dt));
    b.t += dt;
}

double groundContact(BallState& b, const PhysicsParams& P, double dt) {
    const double r = P.radius;
    if (b.pos.y >= r) return 0;
    b.pos.y = r;

    double impact = 0;
    double jn;  // normal impulse per unit mass
    if (b.vel.y < -0.5) {
        impact = -b.vel.y;
        jn = -(1 + P.restitution) * b.vel.y;
        b.vel.y = -P.restitution * b.vel.y;
    } else {
        b.vel.y = jmax(0, b.vel.y);
        jn = P.gravity * dt;
    }

    const V3 rc = v3(0, -r, 0);
    const V3 vc = add(b.vel, cross(b.spin, rc));
    const V3 vct = v3(vc.x, 0, vc.z);
    const double st = len(vct);
    if (st > 1e-6) {
        const double k = P.inertiaFactor;
        const double stick = st / (1 + 1 / k);
        const double jt = jmin(stick, P.friction * jn);
        const V3 dir = mul(vct, -1 / st);
        b.vel = add(b.vel, mul(dir, jt));
        b.spin = add(b.spin, mul(cross(rc, dir), jt / (k * r * r)));
    }
    if (impact == 0) {
        const double f = std::exp(-P.rollingResistance * dt);
        b.vel.x *= f;
        b.vel.z *= f;
    }
    return impact;
}

}  // namespace sim
