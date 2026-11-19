#include "sim/challenge.h"

namespace sim {

Scored scoreShot(const ChallengeDef& def, const std::optional<XY>& crossing, ResultType type) {
    if (type != ResultType::Goal) return {0, -1};
    Scored best{def.goalPoints, -1};
    if (crossing) {
        for (size_t i = 0; i < def.targets.size(); i++) {
            const ChallengeTarget& t = def.targets[i];
            if (jhypot(crossing->x - t.x, crossing->y - t.y) <= t.r && t.points > best.points) best = {t.points, static_cast<int>(i)};
        }
    }
    return best;
}

std::optional<Medal> medalFor(const ChallengeDef& def, double points) {
    if (points >= def.medals.gold) return Medal::Gold;
    if (points >= def.medals.silver) return Medal::Silver;
    if (points >= def.medals.bronze) return Medal::Bronze;
    return std::nullopt;
}

const char* str(ResultType t) {
    switch (t) {
    case ResultType::Goal: return "goal";
    case ResultType::Wide: return "wide";
    case ResultType::Over: return "over";
    case ResultType::Miss: return "miss";
    case ResultType::Saved: return "saved";
    case ResultType::Retake: return "retake";
    }
    return "miss";
}
const char* str(Medal m) { return m == Medal::Gold ? "gold" : m == Medal::Silver ? "silver" : "bronze"; }

}  // namespace sim
