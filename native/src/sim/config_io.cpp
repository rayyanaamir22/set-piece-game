#include "sim/config_io.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace sim {

using json = nlohmann::json;

namespace {

#define F(name) j.at(#name).get_to(o.name)

void from(const json& j, PhysicsParams& o) {
    F(mass); F(radius); F(gravity); F(airDensity); F(cdHigh); F(cdLow); F(crisisStart); F(crisisEnd); F(clGain); F(clMax);
    F(spinDecay); F(restitution); F(friction); F(inertiaFactor); F(rollingResistance); F(knuckleAccel);
    F(knuckleSpeedMin); F(knuckleSpeedFull); F(postRestitution); F(netRestitution); F(netDrag);
}

void from(const json& j, ShotParams& o) {
    F(vMin); F(vMax); F(chargeTime); F(sweetMin); F(sweetMax); F(spinSide); F(spinVert); F(knuckleRadius);
    F(knuckleMinPower); F(knuckleSpin); F(baseError); F(missErrorGain); F(overpowerLift); F(drivenSpeed); F(drivenSpin);
    F(lowTargetY); F(lowTopspin); F(lowSpeed); F(lowElevation); F(solverIterations); F(runupTime); F(dummyDelay);
    F(runupAngleDefault); F(runupAngleMax); F(runupDistDefault); F(runupDistMin); F(runupDistMax); F(runupAngleRate);
    F(runupDistRate); F(powerStraight); F(powerWide); F(curlStraight); F(curlWide); F(distPower); F(knuckleMaxAngle);
    F(aimSpeedX); F(aimSpeedY); F(resultTime);
}

void from(const json& j, WallAi& o) {
    F(jumpChance); F(reactionMin); F(reactionMax); F(jumpHeight); F(jumpHeightJitter); F(jumpDuration); F(afterHitWait);
    F(jumpMinBallHeight); F(jumpOnTargetMargin); F(lyingManChance); F(lyingManDistance); F(lyingManHalfLength);
    F(lyingManRadius); F(encroachChance); F(encroachPlayers); F(creepSpeed); F(creepMax); F(encroachThreshold);
    F(rushSpeed); F(rushMax); F(shuffleAmp); F(shuffleFreq); F(dummyJumpChance); F(dummyJumpReactionMax);
}

void from(const json& j, KeeperAi& o) {
    F(lineZ); F(shadeMax); F(reactionSlow); F(reactionFast); F(reactionJitter); F(reachX); F(reachY); F(reachCenterY);
    F(bodyLength); F(standHeight); F(radius); F(armSpan); F(armRadius); F(armY); F(diveSpeed); F(minDiveTime);
    F(maxDiveTime); F(spinReadMin); F(spinReadMax); F(predictNoise); F(ignoreMargin); F(holdSpeedMin); F(holdSpeedMax);
    F(handsT); F(knuckleHoldFactor); F(afterTouchWait); F(earlyGuessChance); F(reboundMaxSpeed); F(reboundBoxX);
    F(reboundZMin); F(reboundZMax); F(maxRebounds); F(dummyBite); F(sinkTime);
}

#undef F

json readJson(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    return json::parse(f);
}

Frequency freq(const json& v) {
    Frequency f;
    if (!parseFrequency(v.get<std::string>().c_str(), f)) throw std::runtime_error("bad frequency: " + v.dump());
    return f;
}
KeeperType ktype(const json& v) {
    KeeperType t;
    if (!parseKeeperType(v.get<std::string>().c_str(), t)) throw std::runtime_error("bad keeperType: " + v.dump());
    return t;
}

Taker parseTaker(const json& j) {
    Taker t;
    t.name = j.at("name").get<std::string>();
    if (j.contains("style")) t.style = j.at("style").get<std::string>();
    t.foot = j.at("foot").get<std::string>().at(0);
    t.accuracy = j.at("accuracy");
    t.curve = j.at("curve");
    t.power = j.at("power");
    return t;
}

ChallengeDef parseChallenge(const json& j) {
    ChallengeDef d;
    d.id = j.at("id");
    d.name = j.at("name");
    d.desc = j.at("desc");
    d.shots = j.at("shots");
    d.spot = {j.at("spot").at("x").get<double>(), j.at("spot").at("z").get<double>()};
    d.settings = parseSettingsPatch(j.at("settings"));
    for (const auto& t : j.at("targets")) d.targets.push_back({t.at("x").get<double>(), t.at("y").get<double>(), t.at("r").get<double>(), t.at("points").get<double>()});
    d.goalPoints = j.at("goalPoints");
    d.medals = {j.at("medals").at("bronze").get<double>(), j.at("medals").at("silver").get<double>(), j.at("medals").at("gold").get<double>()};
    return d;
}

}  // namespace

SettingsPatch parseSettingsPatch(const json& j) {
    SettingsPatch p;
    auto b = [&](const char* k, std::optional<bool>& out) { if (j.contains(k)) out = j.at(k).get<bool>(); };
    auto n = [&](const char* k, std::optional<double>& out) { if (j.contains(k)) out = j.at(k).get<double>(); };
    b("wallEnabled", p.wallEnabled);
    n("wallHeight", p.wallHeight);
    n("wallCount", p.wallCount);
    b("wallJumps", p.wallJumps);
    if (j.contains("lyingMan")) p.lyingMan = freq(j.at("lyingMan"));
    if (j.contains("encroachment")) p.encroachment = freq(j.at("encroachment"));
    b("rebounds", p.rebounds);
    if (j.contains("keeperType")) p.keeperType = ktype(j.at("keeperType"));
    b("keeperPresent", p.keeperPresent);
    n("keeperSkill", p.keeperSkill);
    b("windEnabled", p.windEnabled);
    n("windSpeed", p.windSpeed);
    n("windDirection", p.windDirection);
    return p;
}

SimSettings parseSettings(const json& j) {
    SimSettings s = defaultSettings();
    applyPatch(s, parseSettingsPatch(j));
    return s;
}

std::shared_ptr<SimConfig> loadConfig(const std::string& dir) {
    auto cfg = std::make_shared<SimConfig>();
    from(readJson(dir + "/physics.json"), cfg->physics);
    from(readJson(dir + "/shot.json"), cfg->shot);

    const json ai = readJson(dir + "/ai.json");
    from(ai.at("wall"), cfg->ai.wall);
    from(ai.at("keeper"), cfg->ai.keeper);
    // effective keeper tuning per type: { ...ai.keeper, ...ai.keeperPresets[type] }
    for (int t = 0; t < 4; t++) {
        json merged = ai.at("keeper");
        merged.update(ai.at("keeperPresets").at(str(static_cast<KeeperType>(t))), /*merge_objects=*/false);
        from(merged, cfg->ai.effectiveKeeper[t]);
    }

    for (const auto& t : readJson(dir + "/takers.json")) cfg->takers.push_back(parseTaker(t));
    for (const auto& c : readJson(dir + "/challenges.json")) cfg->challenges.push_back(parseChallenge(c));

    const json presets = readJson(dir + "/presets.json");
    for (size_t i = 0; i < DIFFICULTY_NAMES.size(); i++) {
        const json& p = presets.at(DIFFICULTY_NAMES[i]);
        cfg->presets[i].label = p.at("label");
        cfg->presets[i].accuracyBonus = p.at("accuracyBonus");
        cfg->presets[i].settings = parseSettingsPatch(p.at("settings"));
    }
    return cfg;
}

}  // namespace sim
