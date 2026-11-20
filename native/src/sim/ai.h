// Port of src/sim/ai.ts: tuning types (data/ai.json).
#pragma once
#include <array>

#include "sim/settings.h"

namespace sim {

struct WallAi {
    double jumpChance, reactionMin, reactionMax, jumpHeight, jumpHeightJitter, jumpDuration, afterHitWait,
        jumpMinBallHeight, jumpOnTargetMargin, lyingManChance, lyingManDistance, lyingManHalfLength, lyingManRadius,
        encroachChance, encroachPlayers, creepSpeed, creepMax, encroachThreshold, rushSpeed, rushMax, shuffleAmp,
        shuffleFreq, dummyJumpChance, dummyJumpReactionMax;
};

struct KeeperAi {
    double lineZ, shadeMax, reactionSlow, reactionFast, reactionJitter, reachX, reachY, reachCenterY, bodyLength,
        standHeight, radius, armSpan, armRadius, armY, diveSpeed, minDiveTime, maxDiveTime, spinReadMin, spinReadMax,
        predictNoise, ignoreMargin, holdSpeedMin, holdSpeedMax, handsT, knuckleHoldFactor, afterTouchWait,
        earlyGuessChance, reboundMaxSpeed, reboundBoxX, reboundZMin, reboundZMax, maxRebounds, dummyBite, sinkTime;
};

struct AiParams {
    WallAi wall;
    KeeperAi keeper;
    // The keeper tuning actually in force per type: base values with the type's overrides already applied
    // (done at load time; the TS builds the same thing with `{ ...ai.keeper, ...ai.keeperPresets[type] }`).
    std::array<KeeperAi, 4> effectiveKeeper;
};

inline const KeeperAi& effectiveKeeperAi(const AiParams& ai, KeeperType type) {
    return ai.effectiveKeeper[static_cast<size_t>(type)];
}

}  // namespace sim
