#include "sim/ShotModel.h"

#include <cmath>

namespace sim {

namespace {
constexpr double SOLVE_DT = 1.0 / 240;

struct SpinResult {
    V3 spin;
    bool knuckle;
};

SpinResult spinFor(double az, const XY& contact, double power, bool driven, bool low, const Taker& taker,
                   const ShotParams& SP, const RunupEffect& fx) {
    double cx = contact.x;
    double cy = contact.y;
    const double m = jhypot(cx, cy);
    if (m > 1) {
        cx /= m;
        cy /= m;
    }
    const bool knuckle = jhypot(cx, cy) < SP.knuckleRadius && power >= SP.knuckleMinPower && !driven && fx.knuckleOk;
    const double rightX = std::cos(az);
    const double rightZ = std::sin(az);
    if (knuckle) {
        const double s = SP.knuckleSpin;
        return {{rightX * s * 0.3, s, rightZ * s * 0.3}, true};
    }
    const double scale = taker.curve * (driven ? SP.drivenSpin : 1);
    const double wy = cx * SP.spinSide * scale * fx.curl;
    const double wr = -(cy + (low ? SP.lowTopspin : 0)) * SP.spinVert * scale;
    return {{wr * rightX, wy, wr * rightZ}, false};
}

V3 launchVelocity(double az, double el, double speed) {
    const double ch = std::cos(el) * speed;
    return {std::sin(az) * ch, std::sin(el) * speed, -std::cos(az) * ch};
}

struct Crossing {
    double x, y;
    bool ok;
};

Crossing flyToGoalPlane(const V3& spot, const V3& vel, const V3& spin, const PhysicsParams& P,
                        std::vector<V3>* path = nullptr, const V3* wind = nullptr) {
    BallState b = newBall({spot.x, P.radius, spot.z});
    b.vel = vel;
    b.spin = spin;
    if (wind) b.wind = *wind;
    for (int i = 0; i < 1500; i++) {
        const V3 prev = b.pos;
        stepBall(b, P, SOLVE_DT);
        if (path && i % 6 == 0) path->push_back(b.pos);
        if (b.pos.z <= 0) {
            const double f = prev.z == b.pos.z ? 0 : prev.z / (prev.z - b.pos.z);
            if (path) path->push_back({prev.x + (b.pos.x - prev.x) * f, prev.y + (b.pos.y - prev.y) * f, 0});
            return {prev.x + (b.pos.x - prev.x) * f, prev.y + (b.pos.y - prev.y) * f, true};
        }
        if (b.t > 6) break;
    }
    return {b.pos.x, b.pos.y, false};
}

double ballisticElevation(double dist, double height, double speed, double g) {
    const double v2 = speed * speed;
    const double disc = v2 * v2 - g * (g * dist * dist + 2 * height * v2);
    if (disc < 0) return PI / 4;
    return std::atan((v2 - std::sqrt(disc)) / (g * dist));
}
}  // namespace

RunupEffect runupEffect(const std::optional<RunupSetting>& runup, const ShotParams& SP) {
    const double angle = runup ? runup->angle : SP.runupAngleDefault;
    const double dist = runup ? runup->dist : SP.runupDistDefault;
    const double t = clamp(angle / SP.runupAngleMax, 0, 1);
    const double t0 = SP.runupAngleDefault / SP.runupAngleMax;
    const double angPower = lerp(SP.powerStraight, SP.powerWide, t) / lerp(SP.powerStraight, SP.powerWide, t0);
    const double angCurl = lerp(SP.curlStraight, SP.curlWide, t) / lerp(SP.curlStraight, SP.curlWide, t0);
    const double dd = dist - SP.runupDistDefault;
    const double span = dd >= 0 ? SP.runupDistMax - SP.runupDistDefault : SP.runupDistDefault - SP.runupDistMin;
    const double distPower = 1 + SP.distPower * clamp(dd / span, -1, 1);
    return {angPower * distPower, angCurl, angle <= SP.knuckleMaxAngle};
}

double launchSpeed(double power, bool driven, bool low, const Taker& taker, const ShotParams& SP, double runupPower) {
    double v = (SP.vMin + (SP.vMax - SP.vMin) * clamp(power, 0, 1)) * taker.power * runupPower;
    if (driven) v *= SP.drivenSpeed;
    if (low) v *= SP.lowSpeed;
    return v;
}

Shot computeShot(const ShotRequest& req, const PhysicsParams& P, const ShotParams& SP, bool wantPath) {
    const V3& spot = req.spot;
    const Taker& taker = req.taker;

    double tx = req.target.x;
    double ty = req.target.y;
    if (req.low) ty = jmin(ty, SP.lowTargetY);

    double ex = 0;
    double ey = 0;
    std::array<double, 6> knucklePhase{};
    if (req.rng) {
        const double d = req.power < SP.sweetMin ? SP.sweetMin - req.power : req.power > SP.sweetMax ? req.power - SP.sweetMax : 0;
        const double mag = SP.baseError * (1 - taker.accuracy) + SP.missErrorGain * d;
        const double theta = rngNext(*req.rng) * PI * 2;
        ex = mag * std::cos(theta);
        ey = mag * std::sin(theta) * 0.6;
        if (req.power > SP.sweetMax) ey += SP.overpowerLift * d;
        for (int i = 0; i < 6; i++) knucklePhase[i] = rngNext(*req.rng) * PI * 2;
    }
    tx += ex;
    ty += ey;

    const RunupEffect fx = runupEffect(req.runup, SP);
    const double speed = launchSpeed(req.power, req.driven, req.low, taker, SP, fx.power);
    const double dh = jhypot(tx - spot.x, spot.z);

    double az = std::atan2(tx - spot.x, spot.z);
    double el = req.low ? (SP.lowElevation * PI) / 180 : ballisticElevation(dh, ty - P.radius, speed, P.gravity);

    for (int i = 0; i < SP.solverIterations; i++) {
        const SpinResult trial = spinFor(az, req.contact, req.power, req.driven, req.low, taker, SP, fx);
        const Crossing hit = flyToGoalPlane(spot, launchVelocity(az, el, speed), trial.spin, P);
        if (!hit.ok) {
            el += 0.05;
            continue;
        }
        const double errX = hit.x - tx;
        const double errY = hit.y - ty;
        if (std::fabs(errX) < 0.02 && std::fabs(errY) < 0.02) break;
        az -= (errX / dh) * 0.9;
        if (!req.low) el -= (errY / dh) * 0.9;
        el = clamp(el, -0.1, 1.1);
    }

    const SpinResult fin = spinFor(az, req.contact, req.power, req.driven, req.low, taker, SP, fx);
    const V3 vel = launchVelocity(az, el, speed);

    std::optional<std::vector<V3>> path;
    if (wantPath) {
        path = std::vector<V3>{{spot.x, P.radius, spot.z}};
        flyToGoalPlane(spot, vel, fin.spin, P, &*path, req.wind ? &*req.wind : nullptr);
    }

    Shot s;
    s.vel = vel;
    s.spin = fin.spin;
    s.speed = speed;
    s.azimuth = az;
    s.elevation = el;
    s.knuckle = fin.knuckle;
    s.knucklePhase = knucklePhase;
    s.aimed = {tx, ty};
    s.error = {ex, ey};
    s.path = std::move(path);
    return s;
}

}  // namespace sim
