#include "test_helpers.h"

namespace testkit {

std::shared_ptr<SimConfig> makeCfg(const PatchFn& over, const WallMod& wall, const KeeperMod& keeper) {
    auto cfg = std::make_shared<SimConfig>(*repoConfig());
    SettingsPatch p;
    p.wallEnabled = false;
    p.wallJumps = false;
    p.keeperPresent = false;
    p.lyingMan = Frequency::Off;
    p.encroachment = Frequency::Off;
    p.rebounds = false;
    if (over) over(p);
    cfg->settings = p;
    if (wall) wall(cfg->ai.wall);
    if (keeper) {
        // The TS override replaces fields of the base keeper tuning; the type presets sit on top of it.
        // Overrides used by the tests touch fields the presets do not set, so applying it to all four is equivalent.
        keeper(cfg->ai.keeper);
        for (auto& k : cfg->ai.effectiveKeeper) keeper(k);
    }
    return cfg;
}

Fly fly(const V3& start, const V3& vel, const V3& spin, const PhysicsParams* params, const Knuckle* knuckle, const V3* wind) {
    const PhysicsParams& pp = params ? *params : P();
    Fly f;
    f.ball = newBall(start);
    if (wind) f.ball.wind = *wind;
    f.ball.vel = vel;
    f.ball.spin = spin;
    if (knuckle) f.ball.knuckle = *knuckle;
    for (int i = 0; i < 3000 && f.ball.pos.z > 0; i++) {
        stepBall(f.ball, pp, 1.0 / 240);
        f.pts.push_back(f.ball.pos);
    }
    f.t = f.ball.t;
    return f;
}

const std::optional<AttemptResult>& takeShot(SimState& s, double power, const InputState& extra) {
    InputState hold = extra;
    hold.shoot = true;
    InputState idle = extra;
    idle.shoot = false;
    step(s, hold, DTs);
    while (s.meter < power) step(s, hold, DTs);
    step(s, idle, DTs);
    for (int i = 0; i < 240 * 9 && s.phase != Phase::Result; i++) step(s, idle, DTs);
    return s.result;
}

InputState withContact(double cx, double cy) {
    InputState i = emptyInput();
    i.contactX = cx;
    i.contactY = cy;
    return i;
}

void stepIdle(SimState& s, int n) {
    const InputState idle = emptyInput();
    for (int i = 0; i < n; i++) step(s, idle, DTs);
}

}  // namespace testkit
