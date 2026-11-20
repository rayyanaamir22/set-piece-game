#include "sim/Collisions.h"

#include <cmath>

#include "sim/constants.h"

namespace sim {

namespace {
struct Segment {
    CollisionType kind;
    V3 a, b;
};

const Segment SEGMENTS[3] = {
    {CollisionType::Post, v3(-GOAL_HALF_WIDTH, 0, 0), v3(-GOAL_HALF_WIDTH, GOAL_HEIGHT, 0)},
    {CollisionType::Post, v3(GOAL_HALF_WIDTH, 0, 0), v3(GOAL_HALF_WIDTH, GOAL_HEIGHT, 0)},
    {CollisionType::Bar, v3(-GOAL_HALF_WIDTH, GOAL_HEIGHT, 0), v3(GOAL_HALF_WIDTH, GOAL_HEIGHT, 0)},
};

// Sphere vs capsule (segment + radius). Returns the impact speed, or 0 if no hit.
double collideCapsule(BallState& b, const PhysicsParams& P, const V3& a, const V3& c, double radius, double restitution,
                      double absorb, double spinKeep, double* along = nullptr) {
    const V3 ab = sub(c, a);
    const double t = clamp(dot(sub(b.pos, a), ab) / dot(ab, ab), 0, 1);
    if (along) *along = t;
    const V3 closest = add(a, mul(ab, t));
    const V3 d = sub(b.pos, closest);
    const double dist = len(d);
    const double minDist = P.radius + radius;
    if (dist >= minDist || dist < 1e-9) return 0;

    const V3 n = mul(d, 1 / dist);
    b.pos = add(closest, mul(n, minDist));
    const double vn = dot(b.vel, n);
    if (vn >= 0) return 0;
    b.vel = add(b.vel, mul(n, -(1 + restitution) * vn));
    b.vel = mul(b.vel, absorb);
    b.spin = mul(b.spin, spinKeep);
    return -vn;
}

double collideWall(BallState& b, const PhysicsParams& P, const WallState& wall) {
    double hardest = 0;
    const double r = wall.radius;
    for (size_t i = 0; i < wall.players.size(); i++) {
        const V3& p = wall.players[i];
        const double lift = wall.lift[i];
        const V3 a = v3(p.x, lift + r, p.z);
        const V3 c = v3(p.x, lift + jmax(r, wall.height - r), p.z);
        hardest = jmax(hardest, collideCapsule(b, P, a, c, r, 0.3, 0.75, 0.5));
    }
    if (wall.lying) {
        hardest = jmax(hardest, collideCapsule(b, P, wall.lying->a, wall.lying->b, wall.lying->radius, 0.3, 0.75, 0.5));
    }
    return hardest;
}

struct KeeperHit {
    double speed, preSpeed;
    bool hands;
};

std::optional<KeeperHit> collideKeeper(BallState& b, const PhysicsParams& P, const KeeperState& k, const KeeperAi& ai) {
    const double pre = len(b.vel);
    double t = 0;
    double hit = collideCapsule(b, P, k.a, k.b, ai.radius, 0.35, 0.7, 0.5, &t);
    bool hands = t >= ai.handsT;
    if (hit == 0 && keeperArmsOut(k)) {
        const Capsule arms = keeperArms(k, ai);
        hit = collideCapsule(b, P, arms.a, arms.b, ai.armRadius, 0.35, 0.7, 0.5);
        hands = hit > 0;
    }
    if (hit > 0) return KeeperHit{hit, pre, hands};
    return std::nullopt;
}

double collideNet(BallState& b, const PhysicsParams& P, double dt) {
    if (b.pos.z >= 0) return 0;
    const double R = P.radius;
    const double hx = GOAL_HALF_WIDTH - R;
    const double top = GOAL_HEIGHT - R;
    const double back = -GOAL_DEPTH + R;
    const double e = P.netRestitution;
    double impact = 0;

    if (b.pos.x > hx) {
        impact = jmax(impact, b.vel.x);
        b.pos.x = hx;
        if (b.vel.x > 0) b.vel.x *= -e;
    } else if (b.pos.x < -hx) {
        impact = jmax(impact, -b.vel.x);
        b.pos.x = -hx;
        if (b.vel.x < 0) b.vel.x *= -e;
    }
    if (b.pos.y > top) {
        impact = jmax(impact, b.vel.y);
        b.pos.y = top;
        if (b.vel.y > 0) b.vel.y *= -e;
    }
    if (b.pos.z < back) {
        impact = jmax(impact, -b.vel.z);
        b.pos.z = back;
        if (b.vel.z < 0) b.vel.z *= -e;
    }
    b.vel = mul(b.vel, std::exp(-P.netDrag * dt));
    return impact;
}
}  // namespace

void stepWorld(BallState& b, const PhysicsParams& P, double dt, bool netActive, const WallState* wall,
               const KeeperState* keeper, const KeeperAi& keeperAi, std::vector<CollisionEvent>& out) {
    stepBall(b, P, dt);

    const double bounce = groundContact(b, P, dt);
    if (bounce > 1.5) out.push_back({CollisionType::Bounce, bounce, b.pos, std::nullopt, false});

    for (const Segment& s : SEGMENTS) {
        const double hit = collideCapsule(b, P, s.a, s.b, POST_RADIUS, P.postRestitution, 0.95, 0.6);
        if (hit > 0.5) out.push_back({s.kind, hit, b.pos, std::nullopt, false});
    }

    if (wall) {
        const double hit = collideWall(b, P, *wall);
        if (hit > 0.5) out.push_back({CollisionType::Wall, hit, b.pos, std::nullopt, false});
    }

    if (keeper) {
        const auto hit = collideKeeper(b, P, *keeper, keeperAi);
        if (hit && hit->speed > 0.3) out.push_back({CollisionType::Keeper, hit->speed, b.pos, hit->preSpeed, hit->hands});
    }

    if (netActive) {
        const double hit = collideNet(b, P, dt);
        if (hit > 3) out.push_back({CollisionType::Net, hit, b.pos, std::nullopt, false});
    }
}

}  // namespace sim
