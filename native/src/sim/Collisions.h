// Port of src/sim/Collisions.ts.
#pragma once
#include <optional>
#include <vector>

#include "sim/BallPhysics.h"
#include "sim/Keeper.h"
#include "sim/Wall.h"
#include "sim/ai.h"

namespace sim {

enum class CollisionType { Post, Bar, Net, Bounce, Wall, Keeper };

struct CollisionEvent {
    CollisionType type;
    double speed = 0;
    V3 pos;
    std::optional<double> preSpeed;  // keeper contacts: ball speed before the touch
    bool hands = false;              // ... and whether it was on the hands/upper body
};

// One fixed physics step of the ball in the world; collision events are appended to `out`.
void stepWorld(BallState& b, const PhysicsParams& P, double dt, bool netActive, const WallState* wall,
               const KeeperState* keeper, const KeeperAi& keeperAi, std::vector<CollisionEvent>& out);

}  // namespace sim
