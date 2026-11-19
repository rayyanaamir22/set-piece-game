// Ported from tests/physics.test.ts, shot.test.ts, lowShot.test.ts, wind.test.ts.
#include "test_helpers.h"

#include "sim/Prediction.h"
#include "sim/ShotModel.h"

using namespace testkit;

namespace {
PhysicsParams noAir() {
    PhysicsParams p = P();
    p.cdHigh = 0;
    p.cdLow = 0;
    p.clGain = 0;
    p.spinDecay = 0;
    return p;
}
}  // namespace

TEST_SUITE("BallPhysics") {
    TEST_CASE("no drag, no spin: follows the analytic parabola") {
        BallState b = newBall(v3(0, 1, 0));
        b.vel = v3(3, 8, -10);
        const PhysicsParams na = noAir();
        for (int i = 0; i < 240; i++) stepBall(b, na, 1.0 / 240);
        CLOSE(b.pos.x, 3.0, 3);
        CLOSE(b.pos.z, -10.0, 3);
        CLOSE(b.pos.y, 1 + 8 - 0.5 * 9.81, 3);
    }

    TEST_CASE("drag slows the ball") {
        BallState b = newBall(v3(0, 1, 30));
        b.vel = v3(0, 0, -28);
        for (int i = 0; i < 240; i++) stepBall(b, P(), 1.0 / 240);
        CHECK(-b.vel.z < 24);
        CHECK(-b.vel.z > 15);
    }

    TEST_CASE("sidespin about +y curls the ball towards -x, and -y towards +x") {
        const auto a = fly(v3(0, 1, 25), v3(0, 3, -28), v3(0, 40, 0));
        const auto c = fly(v3(0, 1, 25), v3(0, 3, -28), v3(0, -40, 0));
        CHECK(a.ball.pos.x < -0.5);
        CHECK(c.ball.pos.x > 0.5);
    }

    TEST_CASE("typical curler deflects roughly 1.5-3.5 m over 25 m") {
        const auto a = fly(v3(0, 0.11, 25), v3(0, 3, -28), v3(0, 40, 0));
        CHECK(std::fabs(a.ball.pos.x) > 1.5);
        CHECK(std::fabs(a.ball.pos.x) < 3.5);
    }

    TEST_CASE("topspin drops the ball earlier than no spin; backspin keeps it up") {
        const auto none = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(0, 0, 0));
        const auto top = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(-40, 0, 0));
        const auto back = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(40, 0, 0));
        CHECK(top.ball.pos.y < none.ball.pos.y - 0.2);
        CHECK(back.ball.pos.y > none.ball.pos.y + 0.2);
    }

    TEST_CASE("knuckle wobble is bounded and deterministic") {
        Knuckle k;
        k.amp = repoConfig()->physics.knuckleAccel;
        k.phase = {0.3, 1.1, 2.0, 4.0, 5.1, 0.7};
        const auto a = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0), nullptr, &k);
        const auto base = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0));
        const auto again = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0), nullptr, &k);
        const double dev = jhypot(a.ball.pos.x - base.ball.pos.x, a.ball.pos.y - base.ball.pos.y);
        CHECK(dev > 0.02);
        CHECK(dev < 2);
        CHECK(again.ball.pos.x == a.ball.pos.x);
    }

    TEST_CASE("bounces with restitution and eventually rolls to rest") {
        BallState b = newBall(v3(0, 5, 0));
        double maxAfter = 0;
        bool bounced = false;
        for (int i = 0; i < 240 * 20; i++) {
            stepBall(b, P(), 1.0 / 240);
            const double imp = groundContact(b, P(), 1.0 / 240);
            if (imp > 0 && !bounced) bounced = true;
            else if (bounced && b.vel.y > 0) maxAfter = jmax(maxAfter, b.pos.y);
        }
        CHECK(maxAfter > 1.2);
        CHECK(maxAfter < 2.4);
        CLOSE(b.pos.y, P().radius, 2);
        CHECK(std::fabs(b.vel.y) < 0.1);
    }

    TEST_CASE("backspin checks the ball on landing") {
        const PhysicsParams na = noAir();
        auto run = [&](double wx) {
            BallState b = newBall(v3(0, 2, 0));
            b.vel = v3(0, 0, -10);
            b.spin = v3(wx, 0, 0);
            for (int i = 0; i < 240; i++) {
                stepBall(b, na, 1.0 / 240);
                groundContact(b, na, 1.0 / 240);
            }
            return -b.vel.z;
        };
        CHECK(run(60) < run(-60));
    }
}

namespace {
ShotRequest baseReq() {
    ShotRequest r;
    r.spot = v3(-6, 0, 24);
    r.target = {2.8, 1.9};
    r.contact = {0, 0};
    r.power = sweet();
    r.taker = repoConfig()->takers[0];
    return r;
}
}  // namespace

TEST_SUITE("ShotModel") {
    TEST_CASE("solver lands the ball on the reticle") {
        struct C { const char* n; XY contact; };
        const C cases[] = {{"plain", {0, 0}}, {"curl right-contact", {0.8, 0}}, {"curl left-contact", {-0.8, 0}}, {"dip", {0.3, 0.7}}, {"lift", {0, -0.7}}};
        for (const auto& c : cases) {
            SUBCASE(c.n) {
                ShotRequest r = baseReq();
                r.contact = c.contact;
                const Shot s = computeShot(r, P(), SP());
                const auto f = fly({r.spot.x, P().radius, r.spot.z}, s.vel, s.spin);
                CHECK(std::fabs(f.ball.pos.x - 2.8) < 0.1);
                CHECK(std::fabs(f.ball.pos.y - 1.9) < 0.1);
            }
        }
    }

    TEST_CASE("contact point changes the path shape, not the end point") {
        const Shot plain = computeShot(baseReq(), P(), SP());
        ShotRequest r = baseReq();
        r.contact = {0.9, 0};
        const Shot curl = computeShot(r, P(), SP());
        CHECK(curl.azimuth > plain.azimuth + 0.01);
    }

    TEST_CASE("dip contact needs a higher launch than lift contact") {
        ShotRequest a = baseReq(), b = baseReq();
        a.contact = {0, 0.9};
        b.contact = {0, -0.9};
        CHECK(computeShot(a, P(), SP()).elevation > computeShot(b, P(), SP()).elevation);
    }

    TEST_CASE("centre contact at high power is a knuckleball; side contact is not") {
        ShotRequest r = baseReq();
        r.power = 0.82;
        CHECK(computeShot(r, P(), SP()).knuckle);
        r.contact = {0.5, 0};
        CHECK_FALSE(computeShot(r, P(), SP()).knuckle);
        r = baseReq();
        r.power = 0.3;
        CHECK_FALSE(computeShot(r, P(), SP()).knuckle);
    }

    TEST_CASE("driven shots are faster with less spin") {
        ShotRequest r = baseReq();
        r.contact = {0.8, 0};
        const Shot n = computeShot(r, P(), SP());
        r.driven = true;
        const Shot d = computeShot(r, P(), SP());
        CHECK(d.speed > n.speed);
        CHECK(std::fabs(d.spin.y) < std::fabs(n.spin.y));
    }

    TEST_CASE("missing the sweet spot adds error, hitting it adds almost none") {
        RngState rng{7};
        ShotRequest r = baseReq();
        r.rng = &rng;
        const Shot perfect = computeShot(r, P(), SP());
        r.power = 0.4;
        const Shot off = computeShot(r, P(), SP());
        CHECK(jhypot(perfect.error.x, perfect.error.y) < 0.3);
        CHECK(jhypot(off.error.x, off.error.y) > 3);
    }

    TEST_CASE("overcharging lifts the shot") {
        RngState rng{3};
        ShotRequest r = baseReq();
        r.rng = &rng;
        r.power = 1;
        CHECK(computeShot(r, P(), SP()).error.y > 1);
    }

    TEST_CASE("is deterministic for a given rng seed") {
        RngState r1{99}, r2{99};
        ShotRequest a = baseReq(), b = baseReq();
        a.rng = &r1;
        b.rng = &r2;
        a.power = b.power = 0.6;
        const Shot x = computeShot(a, P(), SP()), y = computeShot(b, P(), SP());
        CHECK(x.vel.x == y.vel.x);
        CHECK(x.vel.y == y.vel.y);
        CHECK(x.vel.z == y.vel.z);
    }
}

TEST_SUITE("Low drive") {
    ShotRequest req(bool low, double x = 1.0, double y = 1.5) {
        ShotRequest r = baseReq();
        r.target = {x, y};
        r.low = low;
        return r;
    }

    TEST_CASE("is launched along the ground, not lobbed to the target") {
        const Shot normal = computeShot(req(false), P(), SP());
        const Shot low = computeShot(req(true), P(), SP());
        CLOSE((low.elevation * 180) / PI, SP().lowElevation, 3);
        CHECK(low.elevation < normal.elevation / 5);
    }

    TEST_CASE("still points at the reticle sideways") {
        const Shot left = computeShot(req(true, -2), P(), SP());
        const Shot right = computeShot(req(true, 2), P(), SP());
        CHECK(right.azimuth > left.azimuth + 0.05);
    }

    TEST_CASE("skids along the grass at ball height all the way to goal") {
        SimState s = createSim(makeCfg(), 1);
        setSpot(s, -6, 24);
        s.aim = {1.0, 1.5};
        InputState in = emptyInput();
        in.low = true;
        takeShot(s, sweet(), in);
        CHECK(s.ball.pos.y < 1.0);
        REQUIRE(s.result);
        CHECK(s.result->type == ResultType::Goal);
        CHECK(std::fabs(s.ball.pos.x - 1.0) < 2.5);
    }
}

namespace {
const V3 wStart = v3(0, 0.11, 25);
const V3 wVel = v3(0, 3, -28);
SimSettings windy(double dir) {
    SimSettings s = defaultSettings();
    s.windEnabled = true;
    s.windSpeed = 5;
    s.windDirection = dir;
    return s;
}
}  // namespace

TEST_SUITE("Wind") {
    TEST_CASE("windVector: direction 0 = towards goal, 90 = towards +x, 180 = towards the pitch") {
        const V3 w0 = windVector(windy(0));
        CLOSE(w0.z, -5.0, 6);
        CLOSE(w0.x, 0.0, 6);
        const V3 w90 = windVector(windy(90));
        CLOSE(w90.x, 5.0, 6);
        CLOSE(w90.z, 0.0, 6);
        CLOSE(windVector(windy(180)).z, 5.0, 6);
    }

    TEST_CASE("windVector is zero when off or with no speed") {
        SimSettings s = windy(90);
        s.windEnabled = false;
        CHECK(windVector(s).x == 0);
        s = windy(90);
        s.windSpeed = 0;
        const V3 w = windVector(s);
        CHECK((w.x == 0 && w.y == 0 && w.z == 0));
    }

    TEST_CASE("zero wind changes nothing") {
        const V3 zero = v3(0, 0, 0);
        const auto a = fly(wStart, wVel, v3());
        const auto b = fly(wStart, wVel, v3(), nullptr, nullptr, &zero);
        CHECK(b.ball.pos.x == a.ball.pos.x);
        CHECK(b.ball.pos.y == a.ball.pos.y);
        CHECK(b.ball.pos.z == a.ball.pos.z);
    }

    TEST_CASE("a crosswind pushes the ball the way it blows, symmetrically") {
        const double base = fly(wStart, wVel, v3()).ball.pos.x;
        const V3 r = v3(8, 0, 0), l = v3(-8, 0, 0);
        const double right = fly(wStart, wVel, v3(), nullptr, nullptr, &r).ball.pos.x - base;
        const double left = fly(wStart, wVel, v3(), nullptr, nullptr, &l).ball.pos.x - base;
        CHECK(right > 0.5);
        CHECK(left < -0.5);
        CLOSE(right, -left, 1);
    }

    TEST_CASE("the deflection grows with wind speed") {
        auto dx = [&](double w) {
            const V3 wind = v3(w, 0, 0);
            return fly(wStart, wVel, v3(), nullptr, nullptr, &wind).ball.pos.x;
        };
        CHECK(dx(3) < dx(6));
        CHECK(dx(6) < dx(10));
    }

    TEST_CASE("a headwind slows the ball and a tailwind speeds it up") {
        const V3 tw = v3(0, 0, -8), hw = v3(0, 0, 8);
        const auto tail = fly(wStart, wVel, v3(), nullptr, nullptr, &tw);
        const auto head = fly(wStart, wVel, v3(), nullptr, nullptr, &hw);
        const auto calm = fly(wStart, wVel, v3());
        CHECK(tail.t < calm.t);
        CHECK(head.t > calm.t);
        CHECK(head.t - tail.t > 0.04);
        CHECK(len(head.ball.vel) < len(tail.ball.vel));
    }

    ShotRequest wreq() {
        ShotRequest r = baseReq();
        r.target = {0.5, 1.2};
        r.contact = {0.3, 0};
        return r;
    }

    TEST_CASE("the solver ignores wind, so a crosswind makes the ball miss the reticle") {
        const Shot shot = computeShot(wreq(), P(), SP());
        const V3 w = v3(9, 0, 0);
        const auto calm = fly({-6, P().radius, 24}, shot.vel, shot.spin);
        const auto windyF = fly({-6, P().radius, 24}, shot.vel, shot.spin, nullptr, nullptr, &w);
        CHECK(std::fabs(calm.ball.pos.x - 0.5) < 0.15);
        CHECK(windyF.ball.pos.x - calm.ball.pos.x > 0.6);
    }

    TEST_CASE("the launch is identical with or without wind; only the predicted-path aid shows the drift") {
        const Shot plain = computeShot(wreq(), P(), SP(), true);
        ShotRequest r = wreq();
        r.wind = v3(9, 0, 0);
        const Shot wy = computeShot(r, P(), SP(), true);
        CHECK(wy.vel.x == plain.vel.x);
        CHECK(wy.vel.y == plain.vel.y);
        CHECK(wy.vel.z == plain.vel.z);
        REQUIRE((plain.path && wy.path));
        CHECK(wy.path->back().x - plain.path->back().x > 0.6);
    }

    TEST_CASE("the keeper's prediction includes wind (he can see it)") {
        const V3 wind = v3(8, 0, 0), spin = v3(0, 20, 0);
        const auto truth = fly(wStart, wVel, spin, nullptr, nullptr, &wind);
        const auto withWind = predictAtPlane(wStart, wVel, spin, P(), 0, wind);
        const auto without = predictAtPlane(wStart, wVel, spin, P(), 0);
        REQUIRE((withWind && without));
        CHECK(std::fabs(withWind->x - truth.ball.pos.x) < 0.1);
        CHECK(std::fabs(without->x - truth.ball.pos.x) > 0.5);
    }

    TEST_CASE("in the sim: puts the wind on the ball and pushes the shot sideways") {
        auto playAt = [&](bool windOn) {
            SimState s = createSim(makeCfg([&](SettingsPatch& p) { p.windEnabled = windOn; p.windSpeed = 10; p.windDirection = 90; }), 3);
            setSpot(s, 0, 24);
            s.aim = {0, 1.2};
            takeShot(s, sweet(), withContact(0.3));
            return s;
        };
        const SimState w = playAt(true), c = playAt(false);
        CLOSE(w.ball.wind.x, 10.0, 6);
        CHECK(c.ball.wind.x == 0);
        CHECK(w.ball.pos.x - c.ball.pos.x > 0.5);
    }

    TEST_CASE("in the sim: switching wind on in settings reaches the ball while lining up") {
        SimState s = createSim(makeCfg(), 3);
        s.settings.windEnabled = true;
        s.settings.windSpeed = 6;
        s.settings.windDirection = 0;
        applySettings(s);
        CLOSE(s.ball.wind.z, -6.0, 6);
    }
}
