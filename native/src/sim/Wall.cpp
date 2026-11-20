#include "sim/Wall.h"

#include <algorithm>
#include <cmath>

#include "sim/Prediction.h"
#include "sim/constants.h"

namespace sim {

namespace {
bool includes(const std::vector<int>& v, int i) { return std::find(v.begin(), v.end(), i) != v.end(); }
}  // namespace

WallRolls rollWall(RngState& rng, const SimSettings& settings, const WallAi& ai) {
    // Draw order is part of the contract: lying-man roll, encroach roll, then the encroacher pick.
    auto roll = [&](Frequency f, double chance) {
        return f == Frequency::Always ? true : f == Frequency::Sometimes ? rngNext(rng) < chance : false;
    };
    WallRolls r;
    r.lyingMan = roll(settings.lyingMan, ai.lyingManChance);
    if (roll(settings.encroachment, ai.encroachChance)) r.encroach = rngNext(rng);
    return r;
}

int autoWallCount(const V3& spot) {
    const double dist = jhypot(spot.x, spot.z);
    int n = dist < 21 ? 5 : dist < 25 ? 4 : dist < 30 ? 3 : 2;
    const double angle = std::fabs(std::atan2(spot.x, spot.z));
    if (angle > 0.6) n -= 1;
    return std::max(1, n);
}

std::optional<WallState> layoutWall(const V3& spot, const SimSettings& settings, const WallAi* ai, const WallRolls& rolls) {
    if (!settings.wallEnabled) return std::nullopt;
    if (jhypot(spot.x, spot.z) < WALL_DISTANCE + 1.5) return std::nullopt;
    const int n = settings.wallCount > 0 ? static_cast<int>(settings.wallCount) : autoWallCount(spot);
    const double width = (n - 1) * WALL_SPACING + 2 * WALL_PLAYER_RADIUS;

    const bool central = std::fabs(spot.x) < 1.2;
    const double side = spot.x >= 0 ? 1 : -1;
    const V3 target = central ? V3{0, 0, 0} : V3{side * GOAL_HALF_WIDTH, 0, 0};
    const V3 u = norm({target.x - spot.x, 0, target.z - spot.z});
    V3 p = {-u.z, 0, u.x};
    if (!central) {
        const double towardsCentre = -side;
        if (jsign(p.x) != towardsCentre) p = mul(p, -1);
    }
    const V3 centre = central ? add(spot, mul(u, WALL_DISTANCE)) : add(add(spot, mul(u, WALL_DISTANCE)), mul(p, width / 2));

    WallState w;
    for (int i = 0; i < n; i++) {
        const double off = (i - (n - 1.0) / 2) * WALL_SPACING;
        const V3 q = add(centre, mul(p, off));
        w.base.push_back({q.x, 0, q.z});
    }

    const V3 facing = mul(u, -1);
    if (ai && rolls.lyingMan) {
        const V3 c = add(centre, mul(facing, -ai->lyingManDistance));
        w.lying = LyingMan{{c.x - p.x * ai->lyingManHalfLength, ai->lyingManRadius, c.z - p.z * ai->lyingManHalfLength},
                           {c.x + p.x * ai->lyingManHalfLength, ai->lyingManRadius, c.z + p.z * ai->lyingManHalfLength},
                           ai->lyingManRadius};
    }

    std::vector<int> encroachers;
    if (ai && rolls.encroach) {
        const int first = std::min(n - 1, static_cast<int>(std::floor(*rolls.encroach * n)));
        for (int k = 0; k < jmax(1, ai->encroachPlayers); k++) encroachers.push_back((first + k) % n);
    }
    std::vector<int> uniq;  // [...new Set(encroachers)] keeps first-seen order
    for (int e : encroachers)
        if (!includes(uniq, e)) uniq.push_back(e);

    w.players = w.base;
    w.advance.assign(n, 0.0);
    w.shuffle.assign(n, 0.0);
    w.axis = p;
    w.encroachers = uniq;
    w.height = settings.wallHeight;
    w.radius = WALL_PLAYER_RADIUS;
    w.facing = facing;
    w.lift.assign(n, 0.0);
    w.earlyJumped.assign(n, false);
    return w;
}

void syncWallPositions(WallState& wall) {
    for (size_t i = 0; i < wall.players.size(); i++) {
        const V3& b = wall.base[i];
        wall.players[i] = {b.x + wall.facing.x * wall.advance[i] + wall.axis.x * wall.shuffle[i], 0,
                           b.z + wall.facing.z * wall.advance[i] + wall.axis.z * wall.shuffle[i]};
    }
}

void updateWallRunup(WallState& wall, const WallAi& ai, double t) {
    const double ramp = jmin(1, t / 0.2);
    for (size_t i = 0; i < wall.players.size(); i++) {
        wall.shuffle[i] = ai.shuffleAmp * ramp * std::sin(2 * PI * ai.shuffleFreq * t + static_cast<double>(i) * 1.7);
        wall.advance[i] = includes(wall.encroachers, static_cast<int>(i)) ? jmin(ai.creepMax, ai.creepSpeed * jmax(0, t - 0.1)) : 0;
    }
    syncWallPositions(wall);
}

bool wallEncroaching(const WallState& wall, const WallAi& ai) {
    return std::any_of(wall.advance.begin(), wall.advance.end(), [&](double a) { return a >= ai.encroachThreshold; });
}

void updateWallFlight(WallState& wall, const WallAi& ai, double dt) {
    if (wall.encroachers.empty()) return;
    for (int i : wall.encroachers) wall.advance[i] = jmin(ai.rushMax, wall.advance[i] + ai.rushSpeed * dt);
    syncWallPositions(wall);
}

bool wallShouldJump(const BallState& ball, const V3& spot, const PhysicsParams& P, const WallAi& ai) {
    const auto goal = predictAtPlane(ball.pos, ball.vel, ball.spin, P, 0, ball.wind);
    if (!goal) return false;
    const double m = ai.jumpOnTargetMargin;
    if (std::fabs(goal->x) > GOAL_HALF_WIDTH + m || goal->y > GOAL_HEIGHT + m) return false;
    if (ai.jumpMinBallHeight <= 0) return true;
    const auto atWall = predictAtDistance(ball.pos, ball.vel, ball.spin, P, spot, WALL_DISTANCE, ball.wind);
    return atWall && atWall->y > ai.jumpMinBallHeight;
}

void wallDummyJump(WallState& wall, const WallAi& ai, RngState& rng) {
    WallJump j;
    for (size_t i = 0; i < wall.players.size(); i++) {
        const bool jumps = rngNext(rng) < ai.dummyJumpChance && !includes(wall.encroachers, static_cast<int>(i));
        const double delay = rngRange(rng, 0, ai.dummyJumpReactionMax);
        const double h = ai.jumpHeight * (1 - ai.jumpHeightJitter * rngNext(rng));
        j.start.push_back(delay);
        j.height.push_back(jumps ? h : 0);
        wall.earlyJumped[i] = jumps;
    }
    j.duration = ai.jumpDuration;
    wall.jump = std::move(j);
}

void startWallJump(WallState& wall, const WallAi& ai, RngState& rng, bool shouldJump, double sinceDummy) {
    const std::optional<WallJump> early = wall.jump;
    WallJump j;
    for (size_t i = 0; i < wall.players.size(); i++) {
        const bool jumps = rngNext(rng) < ai.jumpChance;
        const double delay = rngRange(rng, ai.reactionMin, ai.reactionMax);
        const double h = ai.jumpHeight * (1 - ai.jumpHeightJitter * rngNext(rng));
        if (early && wall.earlyJumped[i]) {
            j.start.push_back(early->start[i] - sinceDummy);
            j.height.push_back(early->height[i]);
        } else {
            j.start.push_back(delay);
            j.height.push_back(shouldJump && jumps && !includes(wall.encroachers, static_cast<int>(i)) ? h : 0);
        }
    }
    j.duration = ai.jumpDuration;
    wall.jump = std::move(j);
}

void updateWallLift(WallState& wall, double t) {
    const auto& j = wall.jump;
    for (size_t i = 0; i < wall.lift.size(); i++) {
        if (!j || j->height[i] <= 0) {
            wall.lift[i] = 0;
            continue;
        }
        const double u = (t - j->start[i]) / j->duration;
        wall.lift[i] = u <= 0 || u >= 1 ? 0 : j->height[i] * 4 * u * (1 - u);
    }
}

}  // namespace sim
