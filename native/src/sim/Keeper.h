// Port of src/sim/Keeper.ts.
#pragma once
#include <optional>

#include "sim/BallPhysics.h"
#include "sim/ai.h"
#include "sim/math/rng.h"
#include "sim/settings.h"

namespace sim {

struct TargetXY {
    double x, y;
};

enum class KeeperPhase { Ready, Committed, Diving, Watching };
enum class KeeperMove { None, Step, Dive };

struct KeeperState {
    KeeperPhase phase = KeeperPhase::Ready;
    KeeperMove kind = KeeperMove::None;
    double skill = 0;
    V3 a, b;  // body capsule: feet-side `a`, hands-side `b`
    double decideAt = INF;
    std::optional<TargetXY> target;
    double launchAt = INF;
    V3 startA, startB, endA, endB;
    double t0 = 0, dur = 1;
    double guess = 0;  // -1/+1 if he committed to a side before the strike
};

std::optional<KeeperState> layoutKeeper(const V3& spot, const SimSettings& settings, const KeeperAi& ai);
bool keeperArmsOut(const KeeperState& k);
struct Capsule {
    V3 a, b;
};
Capsule keeperArms(const KeeperState& k, const KeeperAi& ai);
void keeperOnStrike(KeeperState& k, const KeeperAi& ai, RngState& rng, bool dummy = false);
void keeperStep(KeeperState& k, double t, const BallState& ball, const PhysicsParams& P, const KeeperAi& ai, RngState& rng);

}  // namespace sim
