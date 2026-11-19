#include "sim/settings.h"

#include <cmath>
#include <cstring>

namespace sim {

SimSettings defaultSettings() { return {}; }

void applyPatch(SimSettings& s, const SettingsPatch& p) {
    if (p.wallEnabled) s.wallEnabled = *p.wallEnabled;
    if (p.wallHeight) s.wallHeight = *p.wallHeight;
    if (p.wallCount) s.wallCount = *p.wallCount;
    if (p.wallJumps) s.wallJumps = *p.wallJumps;
    if (p.lyingMan) s.lyingMan = *p.lyingMan;
    if (p.encroachment) s.encroachment = *p.encroachment;
    if (p.rebounds) s.rebounds = *p.rebounds;
    if (p.keeperType) s.keeperType = *p.keeperType;
    if (p.keeperPresent) s.keeperPresent = *p.keeperPresent;
    if (p.keeperSkill) s.keeperSkill = *p.keeperSkill;
    if (p.windEnabled) s.windEnabled = *p.windEnabled;
    if (p.windSpeed) s.windSpeed = *p.windSpeed;
    if (p.windDirection) s.windDirection = *p.windDirection;
}

SimSettings sanitizeSettings(const SimSettings& raw) {
    const SimSettings d = defaultSettings();
    auto num = [](double v, double lo, double hi, double fallback) {
        return std::isfinite(v) ? jmin(hi, jmax(lo, v)) : fallback;
    };
    SimSettings o = raw;
    o.wallHeight = num(raw.wallHeight, WALL_HEIGHT_MIN, WALL_HEIGHT_MAX, d.wallHeight);
    o.wallCount = jround(num(raw.wallCount, 0, WALL_COUNT_MAX, d.wallCount));
    o.keeperSkill = num(raw.keeperSkill, 0, 1, d.keeperSkill);
    o.windSpeed = num(raw.windSpeed, 0, WIND_SPEED_MAX, d.windSpeed);
    o.windDirection = num(raw.windDirection, 0, 360, d.windDirection);
    return o;
}

V3 windVector(const SimSettings& s) {
    if (!s.windEnabled || s.windSpeed <= 0) return {0, 0, 0};
    const double a = (s.windDirection * PI) / 180;
    return {std::sin(a) * s.windSpeed, 0, -std::cos(a) * s.windSpeed};
}

const char* str(Frequency f) { return f == Frequency::Off ? "off" : f == Frequency::Sometimes ? "sometimes" : "always"; }
const char* str(KeeperType t) {
    switch (t) {
    case KeeperType::Balanced: return "balanced";
    case KeeperType::Quick: return "quick";
    case KeeperType::Tall: return "tall";
    case KeeperType::Strong: return "strong";
    }
    return "balanced";
}
bool parseFrequency(const char* s, Frequency& out) {
    if (!std::strcmp(s, "off")) out = Frequency::Off;
    else if (!std::strcmp(s, "sometimes")) out = Frequency::Sometimes;
    else if (!std::strcmp(s, "always")) out = Frequency::Always;
    else return false;
    return true;
}
bool parseKeeperType(const char* s, KeeperType& out) {
    if (!std::strcmp(s, "balanced")) out = KeeperType::Balanced;
    else if (!std::strcmp(s, "quick")) out = KeeperType::Quick;
    else if (!std::strcmp(s, "tall")) out = KeeperType::Tall;
    else if (!std::strcmp(s, "strong")) out = KeeperType::Strong;
    else return false;
    return true;
}

}  // namespace sim
