#include "sim/Keeper.h"

#include <cmath>

#include "sim/Prediction.h"
#include "sim/constants.h"

namespace sim {

namespace {
double gauss(RngState& r) {
    // Three draws, in order (C++ leaves operand evaluation order unspecified, so sequence them).
    const double a = rngNext(r);
    const double b = rngNext(r);
    const double c = rngNext(r);
    return (a + b + c - 1.5) * 2;
}

struct Read {
    double px, py;
    double t;  // seconds from now until the ball reaches his line
};

std::optional<Read> read(const KeeperState& k, const BallState& ball, const PhysicsParams& P, const KeeperAi& ai,
                         RngState& rng, double noiseScale) {
    const double spinRead = lerp(ai.spinReadMin, ai.spinReadMax, k.skill);
    const V3 spin = {ball.spin.x * spinRead, ball.spin.y * spinRead, ball.spin.z * spinRead};
    const auto pred = predictAtPlane(ball.pos, ball.vel, spin, P, ai.lineZ, ball.wind);
    if (!pred) return std::nullopt;
    const double sd = (1 - k.skill) * ai.predictNoise * noiseScale;
    const double px = pred->x + gauss(rng) * sd;
    const double py = pred->y + gauss(rng) * sd * 0.7;
    const double off = ai.ignoreMargin;
    if (std::fabs(px) > GOAL_HALF_WIDTH + off || py > GOAL_HEIGHT + off || py < -off) return std::nullopt;
    return Read{px, py, pred->t};
}

void plan(KeeperState& k, const Read& r, const KeeperAi& ai) {
    const double x0 = k.b.x;
    double dx = r.px - x0;
    double dy = r.py - ai.reachCenterY;
    const double e = jhypot(dx / ai.reachX, dy / ai.reachY);
    if (e > 1) {
        dx /= e;
        dy /= e;
    }
    const double tx = x0 + dx;
    const double ty = jmax(0.2, ai.reachCenterY + dy);

    const double dist = jhypot(tx - k.b.x, ty - k.b.y);
    const double speed = ai.diveSpeed * (0.8 + 0.4 * k.skill);
    k.dur = clamp(dist / speed, ai.minDiveTime, ai.maxDiveTime);
    k.kind = dist < 1.0 ? KeeperMove::Step : KeeperMove::Dive;
    k.target = TargetXY{tx, ty};

    double l = jhypot(tx - x0, ty);
    if (l == 0 || std::isnan(l)) l = 1;  // JS: `|| 1`
    const double dirx = (tx - x0) / l;
    const double diry = ty / l;
    const V3 endB = v3(tx, ty, k.b.z);
    V3 endA = v3(tx - dirx * ai.bodyLength, jmax(0, ty - diry * ai.bodyLength), k.b.z);
    if (k.kind == KeeperMove::Step) {
        endA.x = tx;
        endA.y = 0;
    }
    k.startA = k.a;
    k.startB = k.b;
    k.endA = endA;
    k.endB = endB;
}

void decide(KeeperState& k, double t, const BallState& ball, const PhysicsParams& P, const KeeperAi& ai, RngState& rng) {
    const auto r = read(k, ball, P, ai, rng, 1);
    if (!r) {
        k.phase = KeeperPhase::Watching;
        return;
    }
    plan(k, *r, ai);
    k.launchAt = jmax(t, t + r->t - k.dur - 0.03);
    k.phase = KeeperPhase::Committed;
}
}  // namespace

std::optional<KeeperState> layoutKeeper(const V3& spot, const SimSettings& settings, const KeeperAi& ai) {
    if (!settings.keeperPresent) return std::nullopt;
    const double shade = ai.shadeMax * clamp(std::fabs(spot.x) / 10, 0, 1);
    const double x = spot.x == 0 ? 0 : -jsign(spot.x) * shade;
    const V3 a = v3(x, 0, ai.lineZ);
    const V3 b = v3(x, ai.standHeight, ai.lineZ);
    KeeperState k;
    k.phase = KeeperPhase::Ready;
    k.kind = KeeperMove::None;
    k.skill = settings.keeperSkill;
    k.a = a;
    k.b = b;
    k.startA = a;
    k.startB = b;
    k.endA = a;
    k.endB = b;
    return k;
}

bool keeperArmsOut(const KeeperState& k) { return k.phase != KeeperPhase::Diving || k.kind == KeeperMove::Step; }

Capsule keeperArms(const KeeperState& k, const KeeperAi& ai) {
    const double x = k.b.x;
    return {v3(x - ai.armSpan, ai.armY, k.b.z), v3(x + ai.armSpan, ai.armY, k.b.z)};
}

void keeperOnStrike(KeeperState& k, const KeeperAi& ai, RngState& rng, bool dummy) {
    k.decideAt = lerp(ai.reactionSlow, ai.reactionFast, k.skill) + rngRange(rng, -ai.reactionJitter, ai.reactionJitter);
    k.phase = KeeperPhase::Ready;
    k.guess = 0;
    if (rngNext(rng) < (ai.earlyGuessChance + (dummy ? ai.dummyBite : 0)) * (1 - k.skill)) {
        k.guess = rngNext(rng) < 0.5 ? -1 : 1;
        k.phase = KeeperPhase::Committed;
        k.launchAt = 0.05;
    }
}

void keeperStep(KeeperState& k, double t, const BallState& ball, const PhysicsParams& P, const KeeperAi& ai, RngState& rng) {
    if (k.phase == KeeperPhase::Ready && t >= k.decideAt) decide(k, t, ball, P, ai, rng);
    if (k.phase == KeeperPhase::Committed && t >= k.launchAt) {
        std::optional<Read> r;
        if (k.guess != 0) r = Read{k.b.x + k.guess * 1.9, 1.0, 0};  // a guess has no look: he just goes
        else r = read(k, ball, P, ai, rng, 0.5);
        if (!r) {
            k.phase = KeeperPhase::Watching;
            return;
        }
        plan(k, *r, ai);
        k.t0 = t;
        k.phase = KeeperPhase::Diving;
    }
    if (k.phase != KeeperPhase::Diving) return;

    const double u = clamp((t - k.t0) / k.dur, 0, 1);
    const double e = 1 - (1 - u) * (1 - u);
    V3 a = v3(lerp(k.startA.x, k.endA.x, e), lerp(k.startA.y, k.endA.y, e), k.a.z);
    V3 b = v3(lerp(k.startB.x, k.endB.x, e), lerp(k.startB.y, k.endB.y, e), k.b.z);
    if (k.kind == KeeperMove::Dive && u >= 1) {
        const double s = clamp((t - k.t0 - k.dur) / ai.sinkTime, 0, 1);
        a.y = lerp(a.y, 0.1, s);
        b.y = lerp(b.y, 0.3, s);
    }
    k.a = a;
    k.b = b;
}

}  // namespace sim
