// Ported from tests/runup.test.ts and tests/modes.test.ts.
#include <set>
#include "sim/layout.h"
#include "sim/ShotModel.h"
#include "sim/Wall.h"
#include "test_helpers.h"

using namespace testkit;

namespace {
const InputState IDLE = emptyInput();
RunupSetting D() { return {SP().runupAngleDefault, SP().runupDistDefault}; }
RunupSetting with(double angle, double dist) { return {angle, dist}; }
void waitResult(SimState& s, double seconds = 6) {
    for (int i = 0; i < seconds * 240 && s.phase == Phase::Result; i++) step(s, IDLE, DTs);
}
}  // namespace

TEST_SUITE("runupEffect") {
    TEST_CASE("the default run-up (and an omitted one) is exactly neutral") {
        for (const RunupEffect& fx : {runupEffect(D(), SP()), runupEffect(std::nullopt, SP())}) {
            CLOSE(fx.power, 1.0, 10);
            CLOSE(fx.curl, 1.0, 10);
            CHECK(fx.knuckleOk);
        }
    }

    TEST_CASE("straighter = more power and less curl; wider = less power and more curl") {
        const auto straight = runupEffect(with(0, D().dist), SP());
        const auto wide = runupEffect(with(SP().runupAngleMax, D().dist), SP());
        CHECK(straight.power > 1);
        CHECK(straight.curl < 1);
        CHECK(wide.power < 1);
        CHECK(wide.curl > 1);
    }

    TEST_CASE("is monotonic in the angle") {
        auto prev = runupEffect(with(0, D().dist), SP());
        for (double a = 5; a <= SP().runupAngleMax; a += 5) {
            const auto fx = runupEffect(with(a, D().dist), SP());
            CHECK(fx.power < prev.power);
            CHECK(fx.curl > prev.curl);
            prev = fx;
        }
    }

    TEST_CASE("a longer run-up adds a little power, bounded by distPower") {
        const double shortP = runupEffect(with(D().angle, SP().runupDistMin), SP()).power;
        const double longP = runupEffect(with(D().angle, SP().runupDistMax), SP()).power;
        CLOSE(shortP, 1 - SP().distPower, 6);
        CLOSE(longP, 1 + SP().distPower, 6);
        CLOSE(runupEffect(with(D().angle, 50), SP()).power, 1 + SP().distPower, 6);
    }

    TEST_CASE("a very wide approach rules out the knuckleball") {
        CHECK(runupEffect(with(SP().knuckleMaxAngle, D().dist), SP()).knuckleOk);
        CHECK_FALSE(runupEffect(with(SP().knuckleMaxAngle + 1, D().dist), SP()).knuckleOk);
    }
}

TEST_SUITE("Run-up in the shot model") {
    ShotRequest req(std::optional<RunupSetting> runup, XY contact = {0.6, 0}) {
        ShotRequest r;
        r.spot = v3(-6, 0, 24);
        r.target = {1.5, 1.4};
        r.contact = contact;
        r.power = sweet();
        r.taker = repoConfig()->takers[0];
        r.runup = runup;
        return r;
    }

    TEST_CASE("the default run-up reproduces the previous behaviour exactly") {
        const Shot a = computeShot(req(D()), P(), SP()), b = computeShot(req(std::nullopt), P(), SP());
        CHECK(a.vel.x == b.vel.x);
        CHECK(a.vel.y == b.vel.y);
        CHECK(a.vel.z == b.vel.z);
    }

    TEST_CASE("straight run-up: faster and less spin; wide run-up: slower and more spin") {
        const Shot straight = computeShot(req(with(0, D().dist)), P(), SP());
        const Shot def = computeShot(req(D()), P(), SP());
        const Shot wide = computeShot(req(with(SP().runupAngleMax, D().dist)), P(), SP());
        CHECK(straight.speed > def.speed);
        CHECK(wide.speed < def.speed);
        CHECK(std::fabs(straight.spin.y) < std::fabs(def.spin.y));
        CHECK(std::fabs(wide.spin.y) > std::fabs(def.spin.y));
    }

    TEST_CASE("a wide approach with a centre strike is not a knuckleball") {
        CHECK(computeShot(req(D(), {0, 0}), P(), SP()).knuckle);
        CHECK_FALSE(computeShot(req(with(70, D().dist), {0, 0}), P(), SP()).knuckle);
    }

    TEST_CASE("the solver still lands on the reticle") {
        const double cases[][2] = {{0, 2}, {0, 6}, {75, 2}, {75, 6}, {35, 3.5}};
        for (const auto& c : cases) {
            SUBCASE((std::to_string(c[0]) + "/" + std::to_string(c[1])).c_str()) {
                const ShotRequest r = req(with(c[0], c[1]));
                const Shot shot = computeShot(r, P(), SP());
                const auto end = fly({-6, P().radius, 24}, shot.vel, shot.spin).ball.pos;
                CHECK(std::fabs(end.x - 1.5) < 0.15);
                CHECK(std::fabs(end.y - 1.4) < 0.15);
            }
        }
    }
}

TEST_SUITE("Run-up geometry") {
    const V3 spot = {-8, 0, 22};

    TEST_CASE("the start point is exactly `dist` from the ball, on the taker's side") {
        for (char foot : {'R', 'L'}) {
            const V3 f = dirToGoal(spot);
            const V3 r = rightOf(f);
            const double side = foot == 'R' ? -1 : 1;
            for (double angle : {0.0, 20.0, 60.0, 75.0}) {
                const V3 start = kickerPositions(spot, foot, angle, 4).runupStart;
                const double dx = start.x - spot.x, dz = start.z - spot.z;
                CLOSE(jhypot(dx, dz), 4.0, 6);
                const double lateral = dx * r.x + dz * r.z;
                const double back = -(dx * f.x + dz * f.z);
                CHECK(back > 0);
                if (angle == 0) CLOSE(lateral, 0.0, 6);
                else CHECK(jsign(lateral) == side);
            }
        }
    }

    TEST_CASE("a wider angle swings the start point further sideways and less far back") {
        const V3 f = dirToGoal(spot), r = rightOf(f);
        auto at = [&](double angle) {
            const V3 p = kickerPositions(spot, 'R', angle, 4).runupStart;
            return std::pair{std::fabs((p.x - spot.x) * r.x + (p.z - spot.z) * r.z), -((p.x - spot.x) * f.x + (p.z - spot.z) * f.z)};
        };
        CHECK(at(60).first > at(20).first);
        CHECK(at(60).second < at(20).second);
    }
}

TEST_SUITE("Run-up in the sim") {
    SimState fresh() {
        SimState s = createSim(makeCfg(), 1);
        setSpot(s, -6, 24);
        return s;
    }
    void hold(SimState& s, const InputState& in, double seconds) {
        for (int i = 0; i < seconds * 240; i++) step(s, in, DTs);
    }

    TEST_CASE("starts at the default") {
        const SimState s = fresh();
        CHECK(s.runup.angle == D().angle);
        CHECK(s.runup.dist == D().dist);
    }

    TEST_CASE("the angle and distance axes move it at the configured rates") {
        SimState s = fresh();
        InputState in = emptyInput();
        in.runupAngle = 1;
        hold(s, in, 0.5);
        CLOSE(s.runup.angle, D().angle + SP().runupAngleRate * 0.5, 1);
        in = emptyInput();
        in.runupDist = 1;
        hold(s, in, 0.5);
        CLOSE(s.runup.dist, D().dist + SP().runupDistRate * 0.5, 1);
        in.runupDist = -1;
        hold(s, in, 0.25);
        CLOSE(s.runup.dist, D().dist + SP().runupDistRate * 0.25, 1);
    }

    TEST_CASE("is clamped to the legal range") {
        SimState s = fresh();
        InputState in = emptyInput();
        in.runupAngle = 1;
        in.runupDist = 1;
        hold(s, in, 5);
        CHECK(s.runup.angle == SP().runupAngleMax);
        CHECK(s.runup.dist == SP().runupDistMax);
        in.runupAngle = -1;
        in.runupDist = -1;
        hold(s, in, 10);
        CHECK(s.runup.angle == 0);
        CHECK(s.runup.dist == SP().runupDistMin);
    }

    TEST_CASE("changing it changes the aim preview (power and curl move the launch)") {
        SimState s = fresh();
        s.contact = {0.7, 0};
        hold(s, IDLE, 0.05);
        const double before = s.preview->azimuth;
        InputState in = emptyInput();
        in.runupAngle = 1;
        hold(s, in, 0.6);
        CHECK(s.preview->azimuth != before);
    }

    TEST_CASE("a longer run-up takes longer") {
        auto dur = [&](double dist) {
            SimState s = fresh();
            s.runup.dist = dist;
            InputState h = emptyInput();
            h.shoot = true;
            step(s, h, DTs);
            while (s.meter < sweet()) step(s, h, DTs);
            step(s, IDLE, DTs);
            return s.runupDur;
        };
        CLOSE(dur(6), SP().runupTime * (6 / SP().runupDistDefault), 6);
        CHECK(dur(2) < dur(6));
    }

    TEST_CASE("is locked once the shot is on its way") {
        SimState s = fresh();
        s.aim = {1, 1.2};
        InputState h = emptyInput();
        h.shoot = true;
        step(s, h, DTs);
        while (s.meter < sweet()) step(s, h, DTs);
        step(s, IDLE, DTs);
        const RunupSetting locked = s.runup;
        InputState in = emptyInput();
        in.runupAngle = 1;
        in.runupDist = 1;
        hold(s, in, 1);
        CHECK(s.runup.angle == locked.angle);
        CHECK(s.runup.dist == locked.dist);
    }
}

TEST_SUITE("Goal-plane crossing") {
    TEST_CASE("records where the ball crossed z = 0, close to the reticle for a clean shot") {
        SimState s = createSim(makeCfg(), 1);
        setSpot(s, -6, 24);
        s.aim = {2.0, 1.5};
        takeShot(s, sweet(), withContact(0.4));
        CHECK(s.result->type == ResultType::Goal);
        REQUIRE(s.crossing);
        CHECK(std::fabs(s.crossing->x - 2.0) < 0.6);
        CHECK(std::fabs(s.crossing->y - 1.5) < 0.6);
        CHECK(s.result->crossing->x == s.crossing->x);
        CHECK(s.result->crossing->y == s.crossing->y);
    }
    TEST_CASE("is reset for each attempt") {
        SimState s = createSim(makeCfg(), 1);
        setSpot(s, -6, 24);
        s.aim = {0.5, 1.0};
        takeShot(s, sweet());
        CHECK(s.crossing.has_value());
        setSpot(s, 0, 24);
        CHECK_FALSE(s.crossing.has_value());
    }
}

TEST_SUITE("Challenge scoring") {
    const ChallengeDef& def() { return repoConfig()->challenges[0]; }

    TEST_CASE("only goals score; the best ring crossed wins; a goal outside every ring gets goalPoints") {
        const auto& t = def().targets[0];
        auto a = scoreShot(def(), XY{t.x, t.y}, ResultType::Saved);
        CHECK(a.points == 0);
        CHECK(a.ring == -1);
        auto b = scoreShot(def(), XY{t.x + 0.1, t.y}, ResultType::Goal);
        CHECK(b.points == t.points);
        CHECK(b.ring == 0);
        auto c = scoreShot(def(), XY{0, 0.3}, ResultType::Goal);
        CHECK(c.points == def().goalPoints);
        CHECK(c.ring == -1);
        CHECK(scoreShot(def(), std::nullopt, ResultType::Goal).points == def().goalPoints);
    }

    TEST_CASE("picks the higher-valued ring where rings overlap") {
        ChallengeDef d = def();
        d.targets = {{0, 1, 1, 2}, {0, 1, 0.3, 7}};
        auto a = scoreShot(d, XY{0, 1}, ResultType::Goal);
        CHECK(a.points == 7);
        CHECK(a.ring == 1);
        auto b = scoreShot(d, XY{0.6, 1}, ResultType::Goal);
        CHECK(b.points == 2);
        CHECK(b.ring == 0);
    }

    TEST_CASE("medals follow the thresholds") {
        const auto& m = def().medals;
        CHECK_FALSE(medalFor(def(), m.bronze - 1).has_value());
        CHECK(medalFor(def(), m.bronze) == Medal::Bronze);
        CHECK(medalFor(def(), m.silver) == Medal::Silver);
        CHECK(medalFor(def(), m.gold + 5) == Medal::Gold);
    }

    TEST_CASE("every shipped challenge is sane") {
        for (const auto& c : repoConfig()->challenges) {
            CHECK(c.shots > 0);
            CHECK(c.medals.bronze < c.medals.silver);
            CHECK(c.medals.silver < c.medals.gold);
            CHECK(jhypot(c.spot.x, c.spot.z) > 17);
            double best = 0;
            for (const auto& t : c.targets) {
                CHECK(std::fabs(t.x) + t.r < 3.66);
                CHECK(t.y + t.r < 2.44);
                CHECK(t.y - t.r >= 0);
                best = jmax(best, t.points);
            }
            CHECK(c.shots * best >= c.medals.gold);
        }
    }
}

TEST_SUITE("Challenge mode") {
    const auto& CH() { return repoConfig()->challenges; }

    TEST_CASE("applies the challenge rules, sets the spot, and restores your settings afterwards") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.keeperPresent = true; p.windEnabled = false; }), 1);
        const SimSettings before = s.settings;
        startChallenge(s, 0);
        CHECK(s.mode == Mode::Challenge);
        CHECK_FALSE(s.settings.wallEnabled);
        CHECK_FALSE(s.settings.keeperPresent);
        CHECK(s.spot.x == CH()[0].spot.x);
        CHECK(s.spot.z == CH()[0].spot.z);
        startChallenge(s, 4);
        CHECK(s.settings.windEnabled);
        CHECK(s.settings.windSpeed == 8);
        setMode(s, Mode::Free);
        CHECK_FALSE(s.challenge.has_value());
        CHECK(s.settings.wallEnabled == before.wallEnabled);
        CHECK(s.settings.keeperPresent == before.keeperPresent);
        CHECK(s.settings.windEnabled == before.windEnabled);
        CHECK(s.settings.windSpeed == before.windSpeed);
        CHECK(s.settings.keeperSkill == before.keeperSkill);
    }

    TEST_CASE("settings from one challenge do not leak into the next") {
        SimState s = createSim(makeCfg(), 1);
        startChallenge(s, 3);
        CHECK(s.settings.keeperPresent);
        startChallenge(s, 0);
        CHECK_FALSE(s.settings.keeperPresent);
        CHECK(s.settings.keeperSkill == defaultSettings().keeperSkill);
    }

    TEST_CASE("counts shots and points, stays on the same spot, and finishes with a medal") {
        SimState s = createSim(makeCfg(), 7);
        startChallenge(s, 0);
        const ChallengeDef def = s.challenge->def;
        const V3 spot = s.spot;
        for (int n = 1; n <= def.shots; n++) {
            s.aim = {def.targets[0].x, def.targets[0].y};
            takeShot(s, sweet(), withContact(0.3));
            CHECK(s.challenge->taken == n);
            if (n < def.shots) {
                waitResult(s);
                CHECK(s.phase == Phase::Aim);
                CHECK(s.spot.x == spot.x);
                CHECK(s.spot.z == spot.z);
            }
        }
        const auto& ch = *s.challenge;
        CHECK(ch.done);
        CHECK(ch.log.size() == static_cast<size_t>(def.shots));
        double sum = 0;
        for (const auto& l : ch.log) sum += l.points;
        CHECK(ch.points == sum);
        CHECK(ch.medal == medalFor(def, ch.points));
        stepIdle(s, 240 * 10);
        CHECK(s.phase == Phase::Result);
    }

    TEST_CASE("R restarts the run and N moves to the next challenge when finished") {
        SimState s = createSim(makeCfg(), 7);
        startChallenge(s, 0);
        s.challenge->done = true;
        s.phase = Phase::Result;
        InputState in = emptyInput();
        in.retry = true;
        step(s, in, DTs);
        CHECK(s.challenge->index == 0);
        CHECK(s.challenge->taken == 0);
        CHECK(s.phase == Phase::Aim);
        s.challenge->done = true;
        s.phase = Phase::Result;
        in = emptyInput();
        in.newSpot = true;
        step(s, in, DTs);
        CHECK(s.challenge->index == 1);
    }

    TEST_CASE("wraps around at both ends of the list") {
        SimState s = createSim(makeCfg(), 1);
        startChallenge(s, static_cast<int>(CH().size()));
        CHECK(s.challenge->index == 0);
        startChallenge(s, -1);
        CHECK(s.challenge->index == static_cast<int>(CH().size()) - 1);
    }

    TEST_CASE("N does not move the ball in a challenge") {
        SimState s = createSim(makeCfg(), 7);
        startChallenge(s, 0);
        const V3 spot = s.spot;
        InputState in = emptyInput();
        in.newSpot = true;
        step(s, in, DTs);
        CHECK(s.spot.x == spot.x);
        CHECK(s.spot.z == spot.z);
        CHECK(s.cfg->ai.keeper.maxRebounds > 0);
    }
}

TEST_SUITE("Practice mode") {
    TEST_CASE("stays on the same spot after each attempt (instant retry)") {
        SimState s = createSim(makeCfg(), 3);
        setMode(s, Mode::Practice);
        setSpot(s, 3, 22);
        s.aim = {-1, 1.4};
        takeShot(s, sweet());
        waitResult(s);
        CHECK(s.phase == Phase::Aim);
        CHECK(s.spot.x == 3);
        CHECK(s.spot.z == 22);
    }
    TEST_CASE("N still gives a new random spot") {
        SimState s = createSim(makeCfg(), 3);
        setMode(s, Mode::Practice);
        setSpot(s, 3, 22);
        InputState in = emptyInput();
        in.newSpot = true;
        step(s, in, DTs);
        CHECK((s.spot.x != 3 || s.spot.z != 22));
    }
    TEST_CASE("free-kick mode still moves to a new spot") {
        SimState s = createSim(makeCfg(), 3);
        setSpot(s, 3, 22);
        s.aim = {-1, 1.4};
        takeShot(s, sweet());
        waitResult(s);
        CHECK((s.spot.x != 3 || s.spot.z != 22));
    }
}

TEST_CASE("There is no wall when the ball is too close for one to fit") {
    CHECK_FALSE(layoutWall({0, 0, 9}, defaultSettings(), &AI().wall).has_value());
    CHECK(layoutWall({0, 0, 12}, defaultSettings(), &AI().wall).has_value());
}

TEST_SUITE("Difficulty presets") {
    const Presets& PR() { return repoConfig()->presets; }

    TEST_CASE("ship with valid ranges") {
        for (const auto& p : PR()) {
            SimSettings a = defaultSettings();
            applyPatch(a, p.settings);
            const SimSettings b = sanitizeSettings(a);
            if (p.settings.keeperSkill) CHECK(b.keeperSkill == *p.settings.keeperSkill);
            if (p.settings.wallHeight) CHECK(b.wallHeight == *p.settings.wallHeight);
            if (p.settings.windSpeed) CHECK(b.windSpeed == *p.settings.windSpeed);
            if (p.settings.windDirection) CHECK(b.windDirection == *p.settings.windDirection);
            CHECK(std::fabs(p.accuracyBonus) <= 0.2);
        }
    }

    TEST_CASE("get monotonically harder: keeper skill up, accuracy bonus down") {
        for (size_t i = 1; i < PR().size(); i++) {
            CHECK(*PR()[i].settings.keeperSkill > *PR()[i - 1].settings.keeperSkill);
            CHECK(PR()[i].accuracyBonus <= PR()[i - 1].accuracyBonus);
        }
    }

    TEST_CASE("applying one changes the settings and records the name") {
        SimState s = createSim(makeCfg(), 1);
        applyPreset(s, DifficultyName::Pro);
        CHECK(s.settings.keeperSkill == *PR()[3].settings.keeperSkill);
        CHECK(s.settings.lyingMan == Frequency::Always);
        CHECK(s.difficulty == "pro");
        CLOSE(s.accuracyBonus, -0.1, 9);
        applyPreset(s, DifficultyName::Easy);
        CHECK_FALSE(s.settings.wallJumps);
        CLOSE(s.accuracyBonus, 0.1, 9);
    }

    TEST_CASE("a positive accuracy bonus shrinks the shot error") {
        auto err = [&](double bonus) {
            SimState s = createSim(makeCfg(), 21);
            setSpot(s, -6, 24);
            s.accuracyBonus = bonus;
            s.taker.accuracy = 0.5;
            s.aim = {1, 1.3};
            takeShot(s, sweet());
            return jhypot(s.pending->error.x, s.pending->error.y);
        };
        CHECK(err(0.4) < err(-0.4));
    }
}

TEST_SUITE("Taker roster") {
    TEST_CASE("has distinct fictional takers with valid stats") {
        const auto& takers = repoConfig()->takers;
        CHECK(takers.size() >= 5);
        std::set<std::string> names;
        for (const auto& t : takers) {
            names.insert(t.name);
            CHECK((t.foot == 'R' || t.foot == 'L'));
            CHECK(t.accuracy > 0);
            CHECK(t.curve > 0);
            CHECK(t.power > 0);
            CHECK(t.accuracy <= 1);
        }
        CHECK(names.size() == takers.size());
    }

    TEST_CASE("selecting a taker switches stats and foot, and wraps") {
        SimState s = createSim(makeCfg(), 1);
        const auto& takers = s.cfg->takers;
        selectTaker(s, 1);
        CHECK(s.taker.name == takers[1].name);
        CHECK(s.foot == takers[1].foot);
        selectTaker(s, static_cast<int>(takers.size()));
        CHECK(s.takerIndex == 0);
        selectTaker(s, -1);
        CHECK(s.takerIndex == static_cast<int>(takers.size()) - 1);
    }

    TEST_CASE("different takers shoot differently (cannon is faster, curler curls more)") {
        auto shot = [&](int i) {
            SimState s = createSim(makeCfg(), 4);
            selectTaker(s, i);
            s.foot = 'R';
            setSpot(s, -6, 24);
            s.aim = {1, 1.4};
            s.contact = {0.7, 0};
            takeShot(s, sweet(), withContact(0.7));
            return *s.pending;
        };
        const Shot cannon = shot(2), bender = shot(5);
        CHECK(cannon.speed > bender.speed);
        CHECK(std::fabs(bender.spin.y) > std::fabs(cannon.spin.y));
    }
}

TEST_SUITE("Mouse aim deltas") {
    TEST_CASE("move the reticle once and clamp to the frame") {
        SimState s = createSim(makeCfg(), 1);
        s.aim = {0, 1.2};
        InputState in = emptyInput();
        in.aimDeltaX = 1.5;
        in.aimDeltaY = 0.4;
        step(s, in, DTs);
        CLOSE(s.aim.x, 1.5, 6);
        CLOSE(s.aim.y, 1.6, 6);
        in.aimDeltaX = 100;
        in.aimDeltaY = -100;
        step(s, in, DTs);
        CHECK(s.aim.x == 7);
        CHECK(s.aim.y == 0.1);
    }
    TEST_CASE("are ignored once the shot is on its way") {
        SimState s = createSim(makeCfg(), 1);
        setSpot(s, -6, 24);
        s.aim = {1, 1.2};
        InputState h = emptyInput();
        h.shoot = true;
        step(s, h, DTs);
        while (s.meter < sweet()) step(s, h, DTs);
        step(s, IDLE, DTs);
        const XY aim = s.aim;
        InputState in = emptyInput();
        in.aimDeltaX = 3;
        step(s, in, DTs);
        CHECK(s.aim.x == aim.x);
        CHECK(s.aim.y == aim.y);
    }
}

TEST_SUITE("Dummy run-up") {
    SimState play(bool dummy, int seed = 5, const PatchFn& extra = nullptr) {
        SimState s = createSim(makeCfg(
                                   [&](SettingsPatch& p) {
                                       p.wallEnabled = true;
                                       p.wallJumps = true;
                                       p.keeperPresent = true;
                                       if (extra) extra(p);
                                   },
                                   [](WallAi& w) {
                                       w.jumpChance = 1;
                                       w.reactionMin = w.reactionMax = 0;
                                       w.dummyJumpChance = 1;
                                       w.dummyJumpReactionMax = 0;
                                   }),
                               seed);
        setSpot(s, -6, 24);
        s.aim = {-1.0, 2.0};
        InputState h = withContact(0.3, 0.8);
        h.shoot = true;
        step(s, h, DTs);
        while (s.meter < sweet()) step(s, h, DTs);
        InputState rel = withContact(0.3, 0.8);
        rel.dummy = dummy;
        step(s, rel, DTs);
        return s;
    }

    TEST_CASE("adds a beat to the run-up") {
        CLOSE(play(true).runupDur, play(false).runupDur + SP().dummyDelay, 6);
        CHECK(play(true).dummy);
        CHECK_FALSE(play(false).dummy);
    }

    TEST_CASE("the wall jumps for the decoy, long before the ball is struck") {
        SimState s = play(true);
        double maxLift = 0;
        while (s.phase == Phase::Runup) {
            step(s, IDLE, DTs);
            if (s.phase == Phase::Runup && s.wall) maxLift = jmax(maxLift, *std::max_element(s.wall->lift.begin(), s.wall->lift.end()));
        }
        CHECK(maxLift > 0.2);
        for (bool b : s.wall->earlyJumped) CHECK(b);
    }

    TEST_CASE("wall players do not jump twice, and are back down by the time the ball arrives") {
        SimState s = play(true);
        while (s.phase == Phase::Runup) step(s, IDLE, DTs);
        while (s.flightT < 0.3) step(s, IDLE, DTs);
        CHECK(*std::max_element(s.wall->lift.begin(), s.wall->lift.end()) < 0.05);
    }

    TEST_CASE("a dip that is blocked by a jumping wall gets through a wall that jumped for the decoy") {
        auto run = [&](bool dummy) {
            SimState s = play(dummy);
            for (int i = 0; i < 240 * 9 && s.phase != Phase::Result; i++) step(s, IDLE, DTs);
            return *s.result;
        };
        CHECK(run(false).wall);
        CHECK_FALSE(run(true).wall);
    }

    TEST_CASE("makes a weak keeper bite more often") {
        auto guesses = [&](bool dummy) {
            int n = 0;
            for (int seed = 1; seed <= 60; seed++) {
                SimState s = play(dummy, seed * 3, [](SettingsPatch& p) { p.keeperSkill = 0.2; });
                while (s.phase == Phase::Runup) step(s, IDLE, DTs);
                if (s.keeper->guess != 0) n++;
            }
            return n;
        };
        CHECK(guesses(true) > guesses(false));
    }

    TEST_CASE("without the modifier nothing changes") {
        SimState s = play(false);
        CHECK_FALSE(s.dummyDone);
        while (s.phase == Phase::Runup) step(s, IDLE, DTs);
        for (bool b : s.wall->earlyJumped) CHECK_FALSE(b);
    }
}
