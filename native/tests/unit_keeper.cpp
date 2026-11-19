// Ported from tests/keeper.test.ts and keeperExtras.test.ts.
#include <cstring>

#include "test_helpers.h"

using namespace testkit;

namespace {
const InputState IDLE = emptyInput();

SimState keeperSim(double skill, int seed, double x = -6, double z = 24) {
    SimState s = createSim(makeCfg([&](SettingsPatch& p) { p.keeperPresent = true; p.keeperSkill = skill; }), seed * 7919);
    setSpot(s, x, z);
    return s;
}

// Fraction of `n` seeds on which the shot ends as the given result.
double rate(double skill, XY aim, const InputState& extra, ResultType want, int n = 24, double power = -1) {
    if (power < 0) power = sweet();
    int c = 0;
    for (int seed = 1; seed <= n; seed++) {
        SimState s = keeperSim(skill, seed);
        s.aim = aim;
        if (takeShot(s, power, extra)->type == want) c++;
    }
    return static_cast<double>(c) / n;
}

SimSettings keeperSettings(double skill = 0.7) {
    SimSettings s = defaultSettings();
    s.keeperPresent = true;
    s.keeperSkill = skill;
    return s;
}
}  // namespace

TEST_SUITE("Keeper layout") {
    TEST_CASE("stands on the goal line side, covering the far post") {
        const KeeperAi& ai = AI().keeper;
        const auto left = layoutKeeper({-10, 0, 24}, defaultSettings(), ai);
        const auto right = layoutKeeper({10, 0, 24}, defaultSettings(), ai);
        CHECK(left->a.x > 0.5);
        CHECK(right->a.x < -0.5);
        CHECK(std::fabs(left->a.x) <= ai.shadeMax);
        CHECK(left->a.z == ai.lineZ);
    }
    TEST_CASE("a central free kick gets a central keeper") { CHECK(layoutKeeper({0, 0, 24}, defaultSettings(), AI().keeper)->a.x == 0); }
    TEST_CASE("no keeper when switched off") {
        SimSettings s = defaultSettings();
        s.keeperPresent = false;
        CHECK_FALSE(layoutKeeper({0, 0, 24}, s, AI().keeper).has_value());
    }
}

TEST_SUITE("Keeper behaviour") {
    TEST_CASE("does not move before his reaction time") {
        SimState s = keeperSim(0.7, 1);
        s.aim = {3, 1};
        const V3 start = s.keeper->b;
        InputState hold = emptyInput();
        hold.shoot = true;
        step(s, hold, DTs);
        while (s.meter < sweet()) step(s, hold, DTs);
        while (s.phase != Phase::Flight) step(s, IDLE, DTs);
        while (s.flightT < 0.15) step(s, IDLE, DTs);
        CHECK(s.keeper->b.x == start.x);
        CHECK(s.keeper->b.y == start.y);
        CHECK(s.keeper->b.z == start.z);
    }

    TEST_CASE("saves most mid-height central shots") { CHECK(rate(0.7, {0.0, 1.2}, withContact(0.3), ResultType::Saved) > 0.6); }
    TEST_CASE("is beaten by a good top-corner shot (beyond his reach)") {
        CHECK(rate(0.7, {3.2, 2.2}, withContact(0.6, 0.3), ResultType::Goal) > 0.7);
    }
    TEST_CASE("cannot reach the near post side (the wall is supposed to cover it)") {
        CHECK(rate(1, {-3.2, 1.5}, withContact(-0.5), ResultType::Goal) > 0.9);
    }
    TEST_CASE("skill matters: a better keeper saves more") {
        const XY shot = {1.5, 1.2};
        const double bad = rate(0.05, shot, withContact(0.4), ResultType::Saved);
        const double good = rate(1, shot, withContact(0.4), ResultType::Saved);
        CHECK(good > bad + 0.2);
    }

    TEST_CASE("ignores shots going well wide") {
        SimState s = keeperSim(0.7, 3);
        s.aim = {6.5, 1};
        CHECK(takeShot(s, sweet())->type == ResultType::Wide);
        CHECK(s.keeper->phase == KeeperPhase::Watching);
    }

    TEST_CASE("only 'saved' results ever involve a keeper touch") {
        for (int seed = 1; seed <= 12; seed++) {
            SimState s = keeperSim(0.7, seed);
            s.aim = {1, 1.5};
            const auto& r = takeShot(s, sweet(), withContact(0.5));
            if (r->type == ResultType::Saved) CHECK(r->keeper.has_value());
        }
    }

    TEST_CASE("with no keeper nothing is ever saved") {
        SimState s = createSim(makeCfg(), 5);
        setSpot(s, 0, 22);
        s.aim = {0, 1};
        CHECK(takeShot(s, sweet())->type != ResultType::Saved);
    }

    TEST_CASE("is deterministic for a given seed") {
        auto run = [&] {
            SimState s = keeperSim(0.7, 11);
            s.aim = {2, 1.5};
            const auto r = *takeShot(s, sweet(), withContact(0.4));
            return std::vector<double>{static_cast<double>(r.type), s.ball.pos.x, s.ball.pos.y, s.ball.pos.z, s.keeper->b.x, s.keeper->b.y};
        };
        CHECK(run() == run());
    }
}

TEST_SUITE("Keeper contact") {
    // Put a ball in flight straight at a stationary keeper.
    SimState direct(double speed, double y, double skill = 0.7) {
        SimState s = keeperSim(skill, 1, 0, 24);
        s.phase = Phase::Flight;
        s.keeper->phase = KeeperPhase::Watching;
        s.ball.pos = {s.keeper->b.x, y, 2};
        s.ball.vel = {0, 0, -speed};
        s.prevBallPos = s.ball.pos;
        return s;
    }
    SimState& run(SimState& s) {
        for (int i = 0; i < 240 * 5 && s.phase != Phase::Result; i++) step(s, IDLE, DTs);
        return s;
    }

    TEST_CASE("a gentle shot at his chest is held") {
        SimState s = direct(10, 1.3);
        run(s);
        CHECK(s.result->type == ResultType::Saved);
        CHECK(s.result->keeper == KeeperTouch::Hold);
        CHECK(s.ball.pos.z > 0.3);
    }

    TEST_CASE("a hard shot at his chest is parried, not held, and bounces back out") {
        SimState s = direct(28, 1.3);
        bool sawBack = false;
        for (int i = 0; i < 240 * 5 && s.phase != Phase::Result; i++) {
            step(s, IDLE, DTs);
            if (s.ball.vel.z > 1) sawBack = true;
        }
        CHECK(s.result->type == ResultType::Saved);
        CHECK(s.result->keeper == KeeperTouch::Touch);
        CHECK(sawBack);
    }

    TEST_CASE("a hard shot at his feet is blocked rather than caught") {
        SimState s = direct(12, 0.25);
        run(s);
        CHECK(s.result->type == ResultType::Saved);
        CHECK(s.result->keeper == KeeperTouch::Touch);
    }

    TEST_CASE("a better keeper holds faster shots") {
        SimState a = direct(15, 1.3, 1), b = direct(15, 1.3, 0);
        CHECK(run(a).result->keeper == KeeperTouch::Hold);
        CHECK(run(b).result->keeper == KeeperTouch::Touch);
    }

    TEST_CASE("a ball hit through his hands that still crosses the line is a goal") {
        SimState s = keeperSim(0.7, 1, 0, 24);
        s.phase = Phase::Flight;
        s.keeper->phase = KeeperPhase::Watching;
        s.ball.pos = {s.keeper->b.x + 0.2, 1.3, 4};
        s.ball.vel = {0, 0, -60};
        s.prevBallPos = s.ball.pos;
        const auto t = run(s).result->type;
        CHECK((t == ResultType::Goal || t == ResultType::Saved));
    }
}

namespace {
bool sameKeeper(const KeeperAi& a, const KeeperAi& b) { return std::memcmp(&a, &b, sizeof(KeeperAi)) == 0; }  // all-double struct
}  // namespace

TEST_SUITE("Keeper types") {
    TEST_CASE("apply their overrides on top of the base tuning") {
        const KeeperAi& bal = effectiveKeeperAi(AI(), KeeperType::Balanced);
        const KeeperAi& quick = effectiveKeeperAi(AI(), KeeperType::Quick);
        const KeeperAi& tall = effectiveKeeperAi(AI(), KeeperType::Tall);
        const KeeperAi& strong = effectiveKeeperAi(AI(), KeeperType::Strong);
        CHECK(sameKeeper(bal, AI().keeper));
        CHECK(quick.reactionFast < bal.reactionFast);
        CHECK(quick.reachX < bal.reachX);
        CHECK(tall.reachX > bal.reachX);
        CHECK(tall.reactionSlow > bal.reactionSlow);
        CHECK(strong.holdSpeedMax > bal.holdSpeedMax);
        CHECK(strong.spinReadMin < bal.spinReadMin);
    }

    TEST_CASE("the sim picks the type from settings and refreshes it when settings change") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; p.keeperType = KeeperType::Tall; }), 1);
        CHECK(s.keeperAi.reachX == effectiveKeeperAi(AI(), KeeperType::Tall).reachX);
        s.settings.keeperType = KeeperType::Quick;
        applySettings(s);
        CHECK(s.keeperAi.reachX == effectiveKeeperAi(AI(), KeeperType::Quick).reachX);
        CHECK(s.keeperAi.reactionFast == effectiveKeeperAi(AI(), KeeperType::Quick).reactionFast);
    }

    TEST_CASE("a quick keeper reacts earlier than a tall one") {
        const SimSettings settings = keeperSettings();
        auto delay = [&](KeeperType type) {
            KeeperAi ai = effectiveKeeperAi(AI(), type);
            ai.earlyGuessChance = 0;
            auto k = layoutKeeper({-6, 0, 24}, settings, ai);
            RngState r{5};
            keeperOnStrike(*k, ai, r);
            return k->decideAt;
        };
        CHECK(delay(KeeperType::Quick) < delay(KeeperType::Tall) - 0.05);
    }
}

TEST_SUITE("Early guess") {
    KeeperState strike(double guessChance, double skill = 0) {
        KeeperAi ai = AI().keeper;
        ai.earlyGuessChance = guessChance;
        auto k = layoutKeeper({-6, 0, 24}, keeperSettings(skill), ai);
        RngState r{11};
        keeperOnStrike(*k, ai, r);
        return *k;
    }

    TEST_CASE("with chance 1 a weak keeper always commits to a side before reading the shot") {
        const KeeperState k = strike(1, 0);
        CHECK(std::fabs(k.guess) == 1);
        CHECK(k.phase == KeeperPhase::Committed);
        CHECK(k.launchAt < 0.1);
    }

    TEST_CASE("with chance 0, or a perfect keeper, he never guesses") {
        CHECK(strike(0, 0).guess == 0);
        CHECK(strike(1, 1).guess == 0);
        CHECK(strike(0, 0).phase == KeeperPhase::Ready);
    }

    TEST_CASE("guessers are on the move long before a normal keeper has reacted") {
        auto play = [&](double chance) {
            SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; p.keeperSkill = 0; }, nullptr,
                                           [&](KeeperAi& k) { k.earlyGuessChance = chance; }),
                                   3);
            setSpot(s, -6, 24);
            s.aim = {1, 1.2};
            InputState hold = emptyInput();
            hold.shoot = true;
            step(s, hold, DTs);
            while (s.meter < sweet()) step(s, hold, DTs);
            while (s.phase != Phase::Flight) step(s, IDLE, DTs);
            const double start = s.keeper->b.x;
            while (s.flightT < 0.25) step(s, IDLE, DTs);
            return std::fabs(s.keeper->b.x - start);
        };
        CHECK(play(1) > 0.3);
        CHECK(play(0) == 0);
    }

    TEST_CASE("a wrong guess leaves the goal open") {
        int goals = 0;
        for (int seed = 1; seed <= 20; seed++) {
            SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; p.keeperSkill = 0; }, nullptr,
                                           [](KeeperAi& k) { k.earlyGuessChance = 1; }),
                                   seed * 5);
            setSpot(s, -6, 24);
            s.aim = {0, 1.2};
            if (takeShot(s, sweet(), withContact(0.3))->type == ResultType::Goal) goals++;
        }
        CHECK(goals > 8);
    }
}

TEST_SUITE("Rebounds") {
    // A hard shot straight at a standing keeper: he gets a hand to it and spills it back into the box.
    SimState parried(const PatchFn& extra = nullptr, int seed = 1, const std::function<void(SimState&)>& setup = nullptr) {
        SimState s = createSim(makeCfg(
                                   [&](SettingsPatch& p) {
                                       p.keeperPresent = true;
                                       p.rebounds = true;
                                       if (extra) extra(p);
                                   },
                                   nullptr, [](KeeperAi& k) { k.earlyGuessChance = 0; }),
                               seed);
        setSpot(s, 0, 24);
        if (setup) setup(s);
        s.phase = Phase::Flight;
        s.keeper->phase = KeeperPhase::Watching;
        s.ball.pos = {s.keeper->b.x, 1.2, 2};
        s.ball.vel = {0, 0, -28};
        s.prevBallPos = s.ball.pos;
        for (int i = 0; i < 240 * 4 && s.phase != Phase::Result; i++) step(s, IDLE, DTs);
        return s;
    }
    void wait(SimState& s) {
        for (int i = 0; i < 240 * 5 && s.phase == Phase::Result; i++) step(s, IDLE, DTs);
    }

    TEST_CASE("a parry that stays in the box is flagged as a rebound") {
        const SimState s = parried();
        CHECK(s.result->type == ResultType::Saved);
        CHECK(s.result->keeper == KeeperTouch::Touch);
        CHECK(s.result->rebound);
    }

    TEST_CASE("after the result you get a follow-up from where the ball ended up, with no wall") {
        SimState s = parried();
        V3 last = s.ball.pos;
        for (int i = 0; i < 240 * 6 && s.phase == Phase::Result; i++) {
            last = s.ball.pos;
            step(s, IDLE, DTs);
        }
        CHECK(s.phase == Phase::Aim);
        CHECK(s.followUp);
        CHECK(s.reboundCount == 1);
        CHECK_FALSE(s.wall.has_value());
        REQUIRE(s.keeper);
        CHECK(s.keeper->phase == KeeperPhase::Ready);
        CHECK(std::fabs(s.spot.x - last.x) < 0.5);
        CHECK(std::fabs(s.spot.z - last.z) < 0.5);
        CHECK(s.spot.z >= 2.5);
        CHECK(s.spot.z <= AI().keeper.reboundZMax);
    }

    TEST_CASE("the follow-up can be taken, and counts as an attempt") {
        SimState s = parried([](SettingsPatch& p) { p.wallEnabled = true; });
        wait(s);
        const int before = s.stats.attempts;
        s.aim = {2.5, 0.6};
        takeShot(s, sweet(), withContact(0.3));
        CHECK(s.stats.attempts == before + 1);
    }

    TEST_CASE("rebounds can be switched off") {
        SimState s = parried([](SettingsPatch& p) { p.rebounds = false; });
        CHECK_FALSE(s.result->rebound);
        wait(s);
        CHECK_FALSE(s.followUp);
        CHECK(s.reboundCount == 0);
    }

    TEST_CASE("is capped at the configured number of consecutive rebounds") {
        const SimState s = parried(nullptr, 1, [](SimState& x) { x.reboundCount = static_cast<int>(AI().keeper.maxRebounds); });
        CHECK_FALSE(s.result->rebound);
    }

    TEST_CASE("a held ball is not a rebound") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; p.rebounds = true; }), 1);
        setSpot(s, 0, 24);
        s.phase = Phase::Flight;
        s.keeper->phase = KeeperPhase::Watching;
        s.ball.pos = {s.keeper->b.x, 1.3, 2};
        s.ball.vel = {0, 0, -8};
        s.prevBallPos = s.ball.pos;
        for (int i = 0; i < 240 * 3 && s.phase != Phase::Result; i++) step(s, IDLE, DTs);
        CHECK(s.result->keeper == KeeperTouch::Hold);
        CHECK_FALSE(s.result->rebound);
    }

    TEST_CASE("a normal new spot clears the follow-up state") {
        SimState s = parried();
        wait(s);
        CHECK(s.followUp);
        setSpot(s, 5, 25);
        CHECK_FALSE(s.followUp);
        CHECK(s.reboundCount == 0);
    }
}
