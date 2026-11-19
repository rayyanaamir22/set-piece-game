// Port of src/sim/challenge.ts.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "sim/ShotModel.h"
#include "sim/math/vec3.h"
#include "sim/settings.h"

namespace sim {

struct SpotXZ {
    double x = 0, z = 0;
};

struct ChallengeTarget {
    double x, y, r, points;  // ring on the goal plane (m), radius, points
};

struct ChallengeDef {
    std::string id, name, desc;
    double shots = 0;
    SpotXZ spot;
    SettingsPatch settings;
    std::vector<ChallengeTarget> targets;
    double goalPoints = 0;
    struct {
        double bronze = 0, silver = 0, gold = 0;
    } medals;
};

enum class Medal { Bronze, Silver, Gold };
enum class ResultType { Goal, Wide, Over, Miss, Saved, Retake };

struct ChallengeShot {
    ResultType type;
    double points;
    int ring;  // index of the ring that was hit, or -1
};

struct ChallengeRun {
    int index = 0;
    ChallengeDef def;
    double taken = 0, points = 0;
    std::vector<ChallengeShot> log;
    bool done = false;
    std::optional<Medal> medal;
    SimSettings backup;  // settings to restore when the challenge ends
};

struct Scored {
    double points;
    int ring;
};

// Only goals score; the best ring the ball crossed the line through is what counts.
Scored scoreShot(const ChallengeDef& def, const std::optional<XY>& crossing, ResultType type);
std::optional<Medal> medalFor(const ChallengeDef& def, double points);

const char* str(ResultType t);
const char* str(Medal m);

}  // namespace sim
