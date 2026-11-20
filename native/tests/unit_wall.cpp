// Ported from tests/sim.test.ts, wall.test.ts, wallExtras.test.ts, jump.test.ts.
#include <algorithm>

#include "sim/Wall.h"
#include "test_helpers.h"

using namespace testkit;

namespace {
const InputState IDLE = emptyInput();

SimState fresh(const std::shared_ptr<SimConfig>& cfg, double x, double z, int seed = 5) {
    SimState s = createSim(cfg, seed);
    setSpot(s, x, z);
    return s;
}
double maxOf(const std::vector<double>& v) { return *std::max_element(v.begin(), v.end()); }
double minOf(const std::vector<double>& v) { return *std::min_element(v.begin(), v.end()); }
}  // namespace

TEST_SUITE("Sim") {
    TEST_CASE("a well-timed shot at the top corner scores") {
        SimState s = fresh(makeCfg(), -6, 24);
        s.aim = {3.0, 2.0};
        const auto& r = takeShot(s, sweet(), withContact(0.6));
        REQUIRE(r);
        CHECK(r->type == ResultType::Goal);
        CHECK(s.stats.attempts == 1);
        CHECK(s.stats.goals == 1);
    }

    TEST_CASE("aiming outside the frame is wide / over") {
        SimState w = fresh(makeCfg(), 0, 22);
        w.aim = {6.5, 1};
        CHECK(takeShot(w, sweet())->type == ResultType::Wide);
        SimState o = fresh(makeCfg(), 0, 22);
        o.aim = {0, 3.8};
        CHECK(takeShot(o, sweet())->type == ResultType::Over);
    }

    TEST_CASE("timing matters: a mistimed release scores far less often than a sweet-spot one") {
        auto rate = [&](double power) {
            int goals = 0;
            for (int seed = 1; seed <= 40; seed++) {
                SimState s = createSim(makeCfg(), seed);
                setSpot(s, -6, 24);
                s.aim = {3.0, 2.0};
                if (takeShot(s, power, withContact(0.6))->type == ResultType::Goal) goals++;
            }
            return goals / 40.0;
        };
        CHECK(rate(sweet()) > 0.8);
        CHECK(rate(0.3) < 0.3);
    }

    TEST_CASE("hitting the post is recorded consistently") {
        SimState s = fresh(makeCfg(), 0, 22);
        s.aim = {3.62, 1.0};
        const auto t = takeShot(s, sweet())->type;
        CHECK((t == ResultType::Goal || t == ResultType::Wide || t == ResultType::Over || t == ResultType::Miss));
    }

    TEST_CASE("auto-advances to a new spot outside the box after the result") {
        SimState s = fresh(makeCfg(), 0, 22);
        s.aim = {1, 1};
        takeShot(s, sweet());
        const int id = s.spotId;
        stepIdle(s, 240 * 5);
        CHECK(s.spotId > id);
        CHECK(s.phase == Phase::Aim);
        CHECK(!(s.spot.z < 16.5 && std::fabs(s.spot.x) < 20.16));
    }

    TEST_CASE("is deterministic: same seed + same inputs = same outcome") {
        auto run = [&] {
            SimState s = fresh(makeCfg(), 5, 25);
            s.aim = {-2, 1.5};
            takeShot(s, 0.7, withContact(-0.5));
            return s.ball.pos;
        };
        const V3 a = run(), b = run();
        CHECK((a.x == b.x && a.y == b.y && a.z == b.z));
    }
}

TEST_SUITE("Wall layout") {
    TEST_CASE("stands 9.15 m from the ball, shoulder to shoulder, facing it") {
        const auto w = layoutWall({-8, 0, 22}, defaultSettings());
        REQUIRE(w);
        double cx = 0, cz = 0;
        for (const auto& p : w->players) {
            cx += p.x / w->players.size();
            cz += p.z / w->players.size();
        }
        const double d = jhypot(cx + 8, cz - 22);
        CHECK(d > 9.15);
        CHECK(d < 9.8);
        for (size_t i = 1; i < w->players.size(); i++)
            CLOSE(jhypot(w->players[i].x - w->players[i - 1].x, w->players[i].z - w->players[i - 1].z), WALL_SPACING, 5);
        CHECK(w->facing.z > 0.5);
    }

    TEST_CASE("covers the near post: the end player's edge sits on the ball -> near post line") {
        const V3 spot = {10, 0, 20};
        SimSettings st = defaultSettings();
        st.wallCount = 4;
        const auto w = layoutWall(spot, st);
        REQUIRE(w);
        const double px = 3.66, pz = 0;
        const double n = jhypot(px - spot.x, pz - spot.z);
        const double ux = (px - spot.x) / n, uz = (pz - spot.z) / n;
        std::vector<double> dists;
        for (const auto& p : w->players) dists.push_back(std::fabs((p.x - spot.x) * uz - (p.z - spot.z) * ux));
        CLOSE(minOf(dists), w->radius, 2);
        for (const auto& p : w->players)
            if (std::fabs((p.x - spot.x) * uz - (p.z - spot.z) * ux) > 0.1) CHECK(p.x < spot.x);
    }

    TEST_CASE("central free kick gets a wall centred on the goal line of sight") {
        SimSettings st = defaultSettings();
        st.wallCount = 3;
        const auto w = layoutWall({0, 0, 24}, st);
        REQUIRE(w);
        CLOSE(w->players[1].x, 0.0, 5);
        CLOSE(w->players[0].x, -w->players[2].x, 5);
    }

    TEST_CASE("auto count: closer and more central = more players") {
        CHECK(autoWallCount({0, 0, 19}) > autoWallCount({0, 0, 31}));
        CHECK(autoWallCount({0, 0, 25}) >= autoWallCount({22, 0, 25}));
    }

    TEST_CASE("honours an explicit count and can be switched off") {
        SimSettings st = defaultSettings();
        st.wallCount = 6;
        CHECK(layoutWall({3, 0, 24}, st)->players.size() == 6);
        st = defaultSettings();
        st.wallEnabled = false;
        CHECK_FALSE(layoutWall({3, 0, 24}, st).has_value());
    }

    TEST_CASE("settings are clamped into legal ranges") {
        SimSettings a = defaultSettings();
        a.wallHeight = 9;
        a.wallCount = 99;
        const SimSettings s = sanitizeSettings(a);
        CHECK(s.wallHeight == WALL_HEIGHT_MAX);
        CHECK(s.wallCount == WALL_COUNT_MAX);
        SimSettings b = defaultSettings();
        b.wallHeight = 0.2;
        CHECK(sanitizeSettings(b).wallHeight == WALL_HEIGHT_MIN);
        b.wallHeight = std::nan("");
        CHECK(sanitizeSettings(b).wallHeight == defaultSettings().wallHeight);
    }

    TEST_CASE("changing settings re-lays the wall while aiming") {
        SimState s = fresh(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; }), -6, 24);
        const double before = s.wall->height;
        s.settings.wallHeight = 2.05;
        applySettings(s);
        CHECK(s.wall->height > before);
        s.settings.wallEnabled = false;
        applySettings(s);
        CHECK_FALSE(s.wall.has_value());
    }

    TEST_CASE("unknown frequency / keeper type names are rejected") {
        Frequency f;
        KeeperType t;
        CHECK_FALSE(parseFrequency("maybe", f));
        CHECK_FALSE(parseKeeperType("bogus", t));
        CHECK(parseFrequency("always", f));
        CHECK(f == Frequency::Always);
        CHECK(parseKeeperType("quick", t));
        CHECK(t == KeeperType::Quick);
    }
}

TEST_SUITE("Wall in play") {
    auto wallCfg = [] { return makeCfg([](SettingsPatch& p) { p.wallEnabled = true; }); };

    TEST_CASE("a flat, hard shot is blocked by the wall") {
        SimState s = fresh(wallCfg(), -6, 24);
        s.aim = {-1.0, 1.2};
        const auto& r = takeShot(s, sweet(), withContact(0.6));
        CHECK(r->wall);
        CHECK(r->type != ResultType::Goal);
    }

    TEST_CASE("a dipping shot clears the wall and scores") {
        SimState s = fresh(wallCfg(), -6, 24);
        s.aim = {-1.0, 2.0};
        const auto& r = takeShot(s, sweet(), withContact(0.3, 0.8));
        CHECK_FALSE(r->wall);
        CHECK(r->type == ResultType::Goal);
    }

    TEST_CASE("the same shot that clears a short wall is blocked by the tallest wall") {
        auto play = [&](double h) {
            SimState s = createSim(wallCfg(), 5);
            s.settings.wallHeight = h;
            setSpot(s, -6, 24);
            s.aim = {-1.0, 2.0};
            return takeShot(s, sweet(), withContact(0.3))->wall;
        };
        CHECK_FALSE(play(WALL_HEIGHT_MIN));
        CHECK(play(WALL_HEIGHT_MAX));
    }

    TEST_CASE("shots round the far side of the wall are not blocked") {
        SimState s = fresh(wallCfg(), -6, 24);
        s.aim = {2.8, 1.2};
        CHECK_FALSE(takeShot(s, sweet(), withContact(0.6))->wall);
    }

    TEST_CASE("with the wall disabled the flat shot is not blocked") {
        SimState s = fresh(makeCfg(), -6, 24);
        s.aim = {-1.0, 1.2};
        const auto& r = takeShot(s, sweet(), withContact(0.6));
        CHECK_FALSE(r->wall);
        CHECK(r->type == ResultType::Goal);
    }
}

namespace {
const V3 spotW = {-6, 0, 24};
SimSettings baseW() {
    SimSettings s = defaultSettings();
    s.wallEnabled = true;
    s.wallCount = 4;
    return s;
}
}  // namespace

TEST_SUITE("Lying man") {
    TEST_CASE("lies on the goal side of the wall, along its axis, on the ground") {
        const auto w = layoutWall(spotW, baseW(), &AI().wall, {true, std::nullopt});
        REQUIRE(w);
        REQUIRE(w->lying);
        const auto& l = *w->lying;
        CHECK(l.a.y == l.radius);
        CLOSE(jhypot(l.b.x - l.a.x, l.b.z - l.a.z), 2 * AI().wall.lyingManHalfLength, 5);
        const double cx = (l.a.x + l.b.x) / 2, cz = (l.a.z + l.b.z) / 2;
        double wx = 0, wz = 0;
        for (const auto& p : w->base) {
            wx += p.x / w->base.size();
            wz += p.z / w->base.size();
        }
        CLOSE(jhypot(cx - wx, cz - wz), AI().wall.lyingManDistance, 5);
        CHECK((cx - wx) * w->facing.x + (cz - wz) * w->facing.z < 0);
    }

    TEST_CASE("is absent without the roll, or without the wall") {
        CHECK_FALSE(layoutWall(spotW, baseW(), &AI().wall, {false, std::nullopt})->lying.has_value());
        SimSettings off = baseW();
        off.wallEnabled = false;
        CHECK_FALSE(layoutWall(spotW, off, &AI().wall, {true, std::nullopt}).has_value());
    }

    TEST_CASE("rolls: off never, always always, sometimes about at the configured rate") {
        RngState rng{99};
        int some = 0;
        for (int i = 0; i < 400; i++) {
            SimSettings a = baseW();
            a.lyingMan = Frequency::Off;
            a.encroachment = Frequency::Off;
            CHECK_FALSE(rollWall(rng, a, AI().wall).lyingMan);
            SimSettings b = baseW();
            b.lyingMan = Frequency::Always;
            CHECK(rollWall(rng, b, AI().wall).lyingMan);
            SimSettings c = baseW();
            c.lyingMan = Frequency::Sometimes;
            if (rollWall(rng, c, AI().wall).lyingMan) some++;
        }
        CHECK(some / 400.0 > AI().wall.lyingManChance - 0.08);
        CHECK(some / 400.0 < AI().wall.lyingManChance + 0.08);
    }

    TEST_CASE("stops a low shot that slides under the jumping wall") {
        auto grounder = [&](Frequency lying) {
            SimState s = createSim(makeCfg(
                                       [&](SettingsPatch& p) {
                                           p.wallEnabled = true;
                                           p.wallJumps = true;
                                           p.lyingMan = lying;
                                       },
                                       [](WallAi& w) {
                                           w.jumpChance = 1;
                                           w.reactionMin = 0;
                                           w.reactionMax = 0;
                                           w.jumpHeight = 0.5;
                                           w.jumpHeightJitter = 0;
                                       }),
                                   5);
            setSpot(s, -6, 24);
            s.aim = {-0.5, 0.1};
            InputState in = emptyInput();
            in.low = true;
            takeShot(s, sweet(), in);
            return *s.result;
        };
        const auto without = grounder(Frequency::Off);
        CHECK_FALSE(without.wall);
        CHECK(without.type == ResultType::Goal);
        const auto withMan = grounder(Frequency::Always);
        CHECK(withMan.wall);
        CHECK(withMan.type != ResultType::Goal);
    }
}

TEST_SUITE("Smart jumping") {
    BallState ball(double vx, double vy, double vz) {
        BallState b = newBall({spotW.x, P().radius, spotW.z});
        b.vel = {vx, vy, vz};
        return b;
    }

    TEST_CASE("jumps for a shot on target") { CHECK(wallShouldJump(ball(5.5, 6, -28), spotW, P(), AI().wall)); }
    TEST_CASE("stays down for a shot going well wide") { CHECK_FALSE(wallShouldJump(ball(14, 5, -26), spotW, P(), AI().wall)); }
    TEST_CASE("stays down for a shot sailing well over the bar") { CHECK_FALSE(wallShouldJump(ball(5.5, 17, -22), spotW, P(), AI().wall)); }

    TEST_CASE("with a height criterion, ignores balls hugging the ground") {
        WallAi ai = AI().wall;
        ai.jumpMinBallHeight = 0.9;
        CHECK_FALSE(wallShouldJump(ball(5.5, 0.2, -28), spotW, P(), ai));
        CHECK(wallShouldJump(ball(5.5, 6, -28), spotW, P(), ai));
    }

    TEST_CASE("startWallJump with shouldJump=false keeps everyone on the ground") {
        auto w = layoutWall(spotW, baseW(), &AI().wall);
        WallAi ai = AI().wall;
        ai.jumpChance = 1;
        RngState r{5};
        startWallJump(*w, ai, r, false);
        CHECK(maxOf(w->jump->height) == 0);
    }
}

TEST_SUITE("Encroachment") {
    SimState sim(Frequency f) {
        SimState s = createSim(makeCfg([&](SettingsPatch& p) { p.wallEnabled = true; p.encroachment = f; }), 5);
        setSpot(s, -6, 24);
        return s;
    }
    void toStrike(SimState& s, XY aim) {
        s.aim = aim;
        InputState hold = emptyInput();
        hold.shoot = true;
        step(s, hold, DTs);
        while (s.meter < sweet()) step(s, hold, DTs);
        step(s, IDLE, DTs);
        while (s.phase == Phase::Runup || s.phase == Phase::Charge) step(s, IDLE, DTs);
    }

    TEST_CASE("one player creeps forward during the run-up, the rest stay on the line") {
        SimState s = sim(Frequency::Always);
        REQUIRE(s.wall->encroachers.size() == 1);
        toStrike(s, {5, 1});
        const auto& w = *s.wall;
        const int i = w.encroachers[0];
        CHECK(w.advance[i] >= AI().wall.encroachThreshold);
        CHECK(w.advance[i] <= AI().wall.creepMax + 1e-9);
        for (size_t j = 0; j < w.advance.size(); j++)
            if (static_cast<int>(j) != i) CHECK(w.advance[j] == 0);
        CHECK(wallEncroaching(w, AI().wall));
        auto dist = [&](const V3& p) { return jhypot(p.x - s.spot.x, p.z - s.spot.z); };
        CHECK(dist(w.players[i]) < dist(w.base[i]) - 0.2);
    }

    TEST_CASE("after the strike the encroacher charges at the ball") {
        SimState s = sim(Frequency::Always);
        toStrike(s, {5, 1});
        const int i = s.wall->encroachers[0];
        const double before = s.wall->advance[i];
        stepIdle(s, 120);
        CHECK(s.wall->advance[i] > before + 0.5);
        CHECK(s.wall->advance[i] <= AI().wall.rushMax + 1e-9);
    }

    TEST_CASE("a shot that does not score is a retake: not counted, same spot") {
        SimState s = sim(Frequency::Always);
        const int spotId = s.spotId;
        const auto& r = takeShot(s, sweet());
        CHECK(s.encroached);
        CHECK(r->type == ResultType::Retake);
        CHECK(s.stats.attempts == 0);
        for (int k = 0; k < 240 * 4 && s.phase == Phase::Result; k++) step(s, IDLE, DTs);
        CHECK(s.phase == Phase::Aim);
        CHECK(s.spotId == spotId);
        CHECK(s.spot.x == -6);
        CHECK(s.spot.z == 24);
        CHECK_FALSE(s.encroached);
    }

    TEST_CASE("a goal stands (advantage)") {
        int goals = 0;
        for (int seed = 1; seed <= 20; seed++) {
            SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.encroachment = Frequency::Always; }), seed * 31);
            setSpot(s, -6, 24);
            s.aim = {3.0, 1.8};
            const auto& r = takeShot(s, sweet(), withContact(0.5, 0.2));
            CHECK(s.encroached);
            if (r->type == ResultType::Goal) {
                goals++;
                CHECK(s.stats.goals == 1);
                CHECK(s.stats.attempts == 1);
            } else {
                CHECK(r->type == ResultType::Retake);
            }
        }
        CHECK(goals > 5);
    }

    TEST_CASE("never happens when switched off") {
        SimState s = sim(Frequency::Off);
        CHECK(s.wall->encroachers.empty());
        takeShot(s, sweet());
        CHECK_FALSE(s.encroached);
        CHECK(s.result->type != ResultType::Retake);
    }
}

TEST_CASE("Wall shuffle moves the wall a little during the run-up, never more than the amplitude") {
    SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; }), 5);
    setSpot(s, -6, 24);
    s.aim = {2, 1.5};
    InputState hold = emptyInput();
    hold.shoot = true;
    step(s, hold, DTs);
    while (s.meter < sweet()) step(s, hold, DTs);
    step(s, IDLE, DTs);
    double maxOff = 0;
    while (s.phase == Phase::Runup) {
        step(s, IDLE, DTs);
        for (size_t i = 0; i < s.wall->players.size(); i++)
            maxOff = jmax(maxOff, jhypot(s.wall->players[i].x - s.wall->base[i].x, s.wall->players[i].z - s.wall->base[i].z));
    }
    CHECK(maxOff > 0.02);
    CHECK(maxOff <= AI().wall.shuffleAmp + 1e-9);
}

namespace {
WallState jumpWall() {
    SimSettings st = defaultSettings();
    st.wallCount = 4;
    return *layoutWall({-6, 0, 24}, st);
}
}  // namespace

TEST_SUITE("Wall jump") {
    TEST_CASE("lift is a parabola: zero before the jump, peak at mid-air, zero after landing") {
        WallState w = jumpWall();
        WallAi ai = AI().wall;
        ai.jumpChance = 1;
        ai.reactionMin = ai.reactionMax = 0.1;
        ai.jumpHeight = 0.5;
        ai.jumpHeightJitter = 0;
        ai.jumpDuration = 0.6;
        RngState r{1};
        startWallJump(w, ai, r);
        updateWallLift(w, 0.05);
        for (double l : w.lift) CHECK(l == 0);
        updateWallLift(w, 0.4);
        for (double l : w.lift) CLOSE(l, 0.5, 5);
        updateWallLift(w, 0.5);
        for (double l : w.lift) CHECK(l > 0.4);
        updateWallLift(w, 0.8);
        for (double l : w.lift) CHECK(l == 0);
    }

    TEST_CASE("jumpChance 0: nobody jumps; chance 1: everybody does") {
        WallState none = jumpWall();
        WallAi a = AI().wall;
        a.jumpChance = 0;
        RngState r1{3};
        startWallJump(none, a, r1);
        updateWallLift(none, 0.3);
        CHECK(maxOf(none.lift) == 0);
        WallState all = jumpWall();
        a.jumpChance = 1;
        a.reactionMax = 0;
        RngState r2{3};
        startWallJump(all, a, r2);
        updateWallLift(all, 0.3);
        CHECK(minOf(all.lift) > 0.2);
    }

    TEST_CASE("each jump is randomised but deterministic per seed") {
        WallState a = jumpWall(), b = jumpWall();
        WallAi ai = AI().wall;
        ai.jumpChance = 1;
        RngState r1{42}, r2{42};
        startWallJump(a, ai, r1);
        startWallJump(b, ai, r2);
        CHECK(a.jump->start == b.jump->start);
        CHECK(a.jump->height == b.jump->height);
        std::vector<double> st = a.jump->start;
        std::sort(st.begin(), st.end());
        CHECK(std::unique(st.begin(), st.end()) - st.begin() > 1);
    }

    TEST_CASE("jump heights stay within the configured range") {
        WallState w = jumpWall();
        WallAi ai = AI().wall;
        ai.jumpChance = 1;
        ai.jumpHeight = 0.42;
        ai.jumpHeightJitter = 0.15;
        RngState r{9};
        startWallJump(w, ai, r);
        for (double h : w.jump->height) {
            CHECK(h <= 0.42);
            CHECK(h >= 0.42 * 0.85);
        }
    }

    TEST_CASE("a dip that clears the standing wall is blocked by a jumping one") {
        auto play = [&](bool jumps) {
            SimState s = createSim(makeCfg(
                                       [&](SettingsPatch& p) {
                                           p.wallEnabled = true;
                                           p.wallJumps = jumps;
                                       },
                                       [](WallAi& w) {
                                           w.jumpChance = 1;
                                           w.reactionMin = w.reactionMax = 0;
                                           w.jumpHeight = 0.5;
                                           w.jumpHeightJitter = 0;
                                       }),
                                   5);
            setSpot(s, -6, 24);
            s.aim = {-1.0, 2.0};
            return *takeShot(s, sweet(), withContact(0.3, 0.8));
        };
        const auto standing = play(false);
        CHECK_FALSE(standing.wall);
        CHECK(standing.type == ResultType::Goal);
        CHECK(play(true).wall);
    }

    TEST_CASE("the wall lands again: lift returns to zero after the jump") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.wallJumps = true; }, [](WallAi& w) { w.jumpChance = 1; }), 5);
        setSpot(s, -6, 24);
        s.aim = {3, 1};
        takeShot(s, sweet());
        CHECK(s.phase == Phase::Result);
        for (int i = 0; i < 240 * 1.5 && s.phase == Phase::Result; i++) step(s, IDLE, DTs);
        CHECK(s.phase == Phase::Result);
        REQUIRE(s.wall);
        for (double l : s.wall->lift) CHECK(l == 0);
    }

    TEST_CASE("with wallJumps off nobody leaves the ground") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.wallJumps = false; }, [](WallAi& w) { w.jumpChance = 1; }), 5);
        setSpot(s, -6, 24);
        s.aim = {-1, 1.2};
        double maxLift = 0;
        InputState hold = emptyInput();
        hold.shoot = true;
        step(s, hold, DTs);
        while (s.meter < sweet()) step(s, hold, DTs);
        for (int i = 0; i < 240 * 3; i++) {
            step(s, IDLE, DTs);
            if (s.wall) maxLift = jmax(maxLift, maxOf(s.wall->lift));
        }
        CHECK(maxLift == 0);
    }
}
