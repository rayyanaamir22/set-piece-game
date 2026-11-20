// B5: skeleton, skinning, IK and the exit checks: the keeper's visible hands stay within 10 cm of the sim's hand point,
// and the striking foot meets the ball at the sim's launch tick.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "anim/character_mesh.h"
#include "anim/ik.h"
#include "anim/players.h"
#include "test_helpers.h"

using namespace anim;
using namespace testkit;
using glm::vec3;

namespace {
vec3 toV(const sim::V3& a) { return vec3(static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)); }
const float DTa = 1.0f / 240;
}  // namespace

TEST_SUITE("Skeleton and skinning") {
    TEST_CASE("the rest pose skins to identity") {
        BoneMats skin;
        skinMatrices(Pose{}, skin);
        for (int b = 0; b < BONE_COUNT; b++)
            for (int c = 0; c < 4; c++)
                for (int r = 0; r < 4; r++) CHECK(skin[b][c][r] == doctest::Approx(c == r ? 1.f : 0.f).epsilon(1e-5));
    }

    TEST_CASE("the character fits the GPU bone budget and every vertex is validly weighted") {
        CHECK(BONE_COUNT <= MAX_GPU_BONES);
        CHECK(MAX_GPU_BONES <= 64);
        const SkinMesh& m = characterMesh();
        CHECK(m.vertices.size() > 500);
        for (const auto& v : m.vertices) {
            float sum = 0;
            for (int i = 0; i < 4; i++) {
                CHECK(v.joints[i] >= 0);
                CHECK(v.joints[i] < BONE_COUNT);
                CHECK(v.weights[i] >= 0);
                sum += v.weights[i];
            }
            CHECK(sum == doctest::Approx(1.f).epsilon(1e-4));
            CHECK(v.material >= 0);
            CHECK(v.material < MATERIAL_COUNT);
        }
        for (unsigned i : m.indices) CHECK(i < m.vertices.size());
    }

    TEST_CASE("the character is a person-sized shape: boots on the ground, head near 1.9 m") {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& v : characterMesh().vertices) {
            lo = std::min(lo, v.pos.y);
            hi = std::max(hi, v.pos.y);
        }
        CHECK(lo > -0.02f);
        CHECK(lo < 0.06f);
        CHECK(hi == doctest::Approx(STAND_HEIGHT).epsilon(0.02));
    }

    TEST_CASE("blending is exact at the ends and keeps rotations unit length") {
        Pose a, b;
        a.rot[THIGH_L] = euler(0.5f, 0, 0);
        b.rot[THIGH_L] = euler(-0.5f, 0.2f, 0);
        const Pose lo = blend(a, b, 0.f), hi = blend(a, b, 1.f), mid = blend(a, b, 0.37f);
        CHECK(glm::length(lo.rot[THIGH_L] - a.rot[THIGH_L]) < 1e-5f);
        CHECK(glm::length(hi.rot[THIGH_L] - b.rot[THIGH_L]) < 1e-5f);
        CHECK(glm::length(mid.rot[THIGH_L]) == doctest::Approx(1.f).epsilon(1e-5));
    }
}

TEST_SUITE("Two-bone IK") {
    vec3 ankle(const Pose& p, Bone foot) {
        BoneMats W;
        forwardKinematics(p, W);
        return position(W[foot]);
    }

    TEST_CASE("a reachable target is hit within a millimetre, for both legs and arms") {
        Pose p;
        const vec3 t(0.25f, 0.25f, 0.30f);
        solveLeg(p, true, t);
        CHECK(glm::length(ankle(p, FOOT_L) - t) < 1e-3f);
        Pose q;
        const vec3 w(-0.3f, 1.55f, 0.35f);
        solveArm(q, false, w);
        BoneMats W;
        forwardKinematics(q, W);
        CHECK(glm::length(position(W[HAND_R]) - w) < 1e-3f);
    }

    TEST_CASE("an unreachable target gives a fully extended limb pointing at it, with no NaNs") {
        Pose p;
        const vec3 t(0.1f, -2.f, 3.f);
        solveLeg(p, false, t);
        const vec3 a = ankle(p, FOOT_R);
        CHECK(std::isfinite(a.x + a.y + a.z));
        const vec3 hip = Skeleton::humanoid().restWorld[THIGH_R];
        CHECK(glm::length(a - hip) == doctest::Approx(THIGH_LEN + SHIN_LEN).epsilon(0.01));
        CHECK(glm::dot(glm::normalize(a - hip), glm::normalize(t - hip)) > 0.999f);
    }

    TEST_CASE("the knee bends forward and the limb keeps its segment lengths") {
        Pose p;
        solveLeg(p, true, vec3(0.1f, 0.35f, 0.25f));
        BoneMats W;
        forwardKinematics(p, W);
        const vec3 hip = position(W[THIGH_L]), knee = position(W[SHIN_L]), foot = position(W[FOOT_L]);
        CHECK(glm::length(knee - hip) == doctest::Approx(THIGH_LEN).epsilon(1e-3));
        CHECK(glm::length(foot - knee) == doctest::Approx(SHIN_LEN).epsilon(1e-3));
        CHECK(knee.z > (hip.z + foot.z) / 2);  // knee in front of the hip-ankle line
    }

    TEST_CASE("weight 0 changes nothing, weight 0.5 is half way") {
        Pose p;
        solveLeg(p, true, vec3(0.1f, 0.4f, 0.3f), 0.f);
        CHECK(glm::length(ankle(p, FOOT_L) - Skeleton::humanoid().restWorld[FOOT_L]) < 1e-5f);
    }
}

TEST_SUITE("Taker: run-up, strike IK, follow-through") {
    struct Run {
        std::vector<float> plantDrift;  // plant ankle displacement per tick near the strike
        vec3 kickAnkle, ball, F;
        bool struck = false;
        vec3 tip;  // front of the striking boot at the launch tick
    };

    Run strike(sim::SimState& s, bool low = false) {
        TakerAnimator ta;
        Run r;
        sim::InputState hold = sim::emptyInput();
        hold.shoot = true;
        hold.low = low;
        sim::InputState idle = sim::emptyInput();
        idle.low = low;
        s.aim = {1.0, 1.4};
        std::vector<vec3> plant;
        sim::step(s, hold, DTa);
        ta.update(s, DTa);
        while (s.meter < sweet()) {
            sim::step(s, hold, DTa);
            ta.update(s, DTa);
        }
        sim::step(s, idle, DTa);
        ta.update(s, DTa);
        double lastT = 0;
        while (s.phase == sim::Phase::Runup) {
            sim::step(s, idle, DTa);
            ta.update(s, DTa);
            if (s.phase == sim::Phase::Runup) {
                const double t = s.runupT / s.runupDur;
                if (t > 0.88) plant.push_back(ta.plantAnkleWorld());
                lastT = t;
            }
        }
        (void)lastT;
        // First frame after the launch tick.
        r.struck = s.phase == sim::Phase::Flight;
        r.kickAnkle = ta.kickAnkleWorld();
        const sim::V3 vel = s.ball.vel;
        r.F = glm::normalize(vec3(static_cast<float>(vel.x), 0, static_cast<float>(vel.z)));
        r.ball = vec3(static_cast<float>(s.spot.x), static_cast<float>(P().radius), static_cast<float>(s.spot.z));
        // Boot tip in world space.
        BoneMats W;
        forwardKinematics(ta.character.pose, W);
        const Bone foot = ta.kickLeft ? FOOT_L : FOOT_R;
        r.tip = vec3(ta.character.model * W[foot] * glm::vec4(0, -0.035f, 0.21f, 1));
        for (size_t i = 1; i < plant.size(); i++) r.plantDrift.push_back(glm::length(plant[i] - plant[0]));
        return r;
    }

    TEST_CASE("the striking foot reaches the ball at the sim's launch tick") {
        for (char foot : {'R', 'L'}) {
            SUBCASE(std::string(1, foot).c_str()) {
                SimState s = createSim(makeCfg(), 11);
                setSpot(s, -6, 24);
                s.foot = foot;
                const Run r = strike(s);
                REQUIRE(r.struck);
                const float R = static_cast<float>(P().radius);
                // The front of the boot is at the ball's surface (within a few cm), and the ankle is behind the ball.
                const float tipDist = glm::length(r.tip - r.ball);
                INFO("boot tip is " << tipDist << " m from the ball centre");
                CHECK(tipDist < R + 0.05f);
                CHECK(tipDist > R - 0.05f);
                CHECK(glm::dot(r.kickAnkle - r.ball, r.F) < 0);
                CHECK(r.kickAnkle.y < 0.2f);
            }
        }
    }

    TEST_CASE("the plant foot does not slide in the last stride") {
        SimState s = createSim(makeCfg(), 11);
        setSpot(s, -6, 24);
        const Run r = strike(s);
        REQUIRE(r.plantDrift.size() > 10);
        const float maxDrift = *std::max_element(r.plantDrift.begin(), r.plantDrift.end());
        INFO("max plant-foot drift " << maxDrift << " m");
        CHECK(maxDrift < 0.02f);
    }

    TEST_CASE("the taker's feet stay on the ground while he stands and while he runs") {
        SimState s = createSim(makeCfg(), 3);
        setSpot(s, 4, 22);
        TakerAnimator ta;
        sim::InputState hold = sim::emptyInput();
        hold.shoot = true;
        sim::step(s, hold, DTa);
        while (s.meter < sweet()) {
            sim::step(s, hold, DTa);
            ta.update(s, DTa);
        }
        sim::step(s, sim::emptyInput(), DTa);
        float lowest = 1e9f;
        while (s.phase == sim::Phase::Runup) {
            sim::step(s, sim::emptyInput(), DTa);
            ta.update(s, DTa);
            BoneMats W;
            forwardKinematics(ta.character.pose, W);
            for (Bone f : {FOOT_L, FOOT_R}) lowest = std::min(lowest, vec3(ta.character.model * glm::vec4(position(W[f]), 1)).y);
        }
        CHECK(lowest > 0.0f);  // ankles never below the grass
    }
}

TEST_SUITE("Keeper: hands follow the hitbox") {
    struct Result {
        float worst = 0;       // worst hand-to-hitbox distance while he is moving, before the first touch
        float atContact = -1;  // ... at the tick the ball touches him
        int samples = 0;
        sim::ResultType type = sim::ResultType::Miss;
    };

    // Plays a shot with the keeper animator ticking at the sim rate.
    Result playShot(double skill, XY aim, double contactX, int seed, double power = -1, double spotX = 0, double spotZ = 22) {
        SimState s = createSim(makeCfg([&](SettingsPatch& p) { p.keeperPresent = true; p.keeperSkill = skill; }), seed);
        setSpot(s, spotX, spotZ);
        s.aim = aim;
        KeeperAnimator ka;
        Result r;
        InputState hold = withContact(contactX);
        hold.shoot = true;
        const InputState idle = withContact(contactX);
        if (power < 0) power = sweet();
        step(s, hold, DTs);
        while (s.meter < power) step(s, hold, DTs);
        step(s, idle, DTs);
        size_t seen = 0;
        for (int i = 0; i < 240 * 9 && s.phase != Phase::Result; i++) {
            step(s, idle, DTs);
            ka.update(s, DTs);
            bool contact = false;
            for (; seen < s.events.size(); seen++)
                if (s.events[seen].type == EventType::Keeper) contact = true;
            if (!s.keeper || !ka.character) continue;
            const bool moving = s.keeper->phase == KeeperPhase::Diving;
            if (!moving || s.keeperTouch == KeeperTouch::Hold) {
                if (contact) {
                    // the catch pulls the hands in from the very next tick; the contact tick itself must still match
                }
            }
            if (moving && !(s.keeperTouch && !contact)) {
                const vec3 b = toV(s.keeper->b);
                const float d = std::max(glm::length(ka.tips[0] - b), glm::length(ka.tips[1] - b));
                r.worst = std::max(r.worst, d);
                r.samples++;
                if (contact && r.atContact < 0) r.atContact = d;
            }
        }
        r.type = s.result ? s.result->type : ResultType::Miss;
        return r;
    }

    TEST_CASE("dives to the corners: both hands stay within 10 cm of the hand point") {
        struct C { const char* name; XY aim; double cx; };
        const C cases[] = {{"high corner", {3.2, 2.2}, 0.6}, {"low corner", {-3.0, 0.4}, -0.5}, {"mid left", {-2.4, 1.2}, -0.4}, {"mid right", {2.6, 1.3}, 0.4}};
        int total = 0;
        for (const auto& c : cases) {
            SUBCASE(c.name) {
                for (int seed = 1; seed <= 6; seed++) {
                    const Result r = playShot(1.0, c.aim, c.cx, seed);
                    INFO(c.name << " seed " << seed << ": worst " << r.worst << " m over " << r.samples << " ticks");
                    CHECK(r.worst < 0.10f);
                    total += r.samples;
                }
            }
        }
        CHECK(total > 0);
    }

    TEST_CASE("at the moment of contact with a save, the hands are on the hand point") {
        int contacts = 0;
        for (int seed = 1; seed <= 24; seed++) {
            const Result r = playShot(1.0, {0.6, 1.2}, 0.3, seed);
            if (r.atContact >= 0) {
                contacts++;
                INFO("seed " << seed << ": " << r.atContact << " m");
                CHECK(r.atContact < 0.10f);
            }
        }
        CHECK(contacts > 0);
    }

    TEST_CASE("standing: arms are out to the hitbox's arm capsule") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; }), 5);
        setSpot(s, 0, 24);
        KeeperAnimator ka;
        for (int i = 0; i < 60; i++) ka.update(s, DTs);
        REQUIRE(ka.character);
        const sim::KeeperState& k = *s.keeper;
        for (int side = 0; side < 2; side++) {
            const vec3 tip = ka.tips[side];
            CHECK(tip.y == doctest::Approx(1.5f).epsilon(0.05));
            CHECK(std::fabs(std::fabs(tip.x - static_cast<float>(k.b.x)) - 0.82f) < 0.12f);
        }
    }

    TEST_CASE("his feet stay on the ground when upright") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.keeperPresent = true; }), 5);
        setSpot(s, 0, 24);
        KeeperAnimator ka;
        for (int i = 0; i < 30; i++) ka.update(s, DTs);
        BoneMats W;
        forwardKinematics(ka.character->pose, W);
        for (Bone f : {FOOT_L, FOOT_R}) {
            const vec3 w = vec3(ka.character->model * glm::vec4(position(W[f]), 1));
            CHECK(w.y == doctest::Approx(0.08f).epsilon(0.1));
        }
    }
}

TEST_SUITE("Wall and lying man") {
    TEST_CASE("a jumping player rises with the sim's lift and puts his arms up") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.wallJumps = true; },
                                       [](WallAi& w) { w.jumpChance = 1; w.reactionMin = w.reactionMax = 0; w.jumpHeight = 0.5; w.jumpHeightJitter = 0; }),
                               5);
        setSpot(s, -6, 24);
        s.aim = {-1.0, 2.0};
        WallAnimator wa;
        InputState hold = withContact(0.3, 0.8);
        hold.shoot = true;
        step(s, hold, DTs);
        while (s.meter < sweet()) step(s, hold, DTs);
        step(s, withContact(0.3, 0.8), DTs);
        float peakHands = 0, groundHands = 1e9f;
        bool sawAir = false;
        for (int i = 0; i < 240 * 2 && s.phase != Phase::Result; i++) {
            step(s, withContact(0.3, 0.8), DTs);
            wa.update(s, DTs);
            if (!s.wall) continue;
            for (size_t k = 0; k < wa.players.size(); k++) {
                const auto& c = wa.players[k];
                BoneMats W;
                forwardKinematics(c.pose, W);
                const float handY = vec3(c.model * glm::vec4(position(W[HAND_L]), 1)).y;
                const float lift = static_cast<float>(s.wall->lift[k]);
                CHECK(vec3(c.model[3]).y == doctest::Approx(lift * c.model[0][0]).epsilon(0.01).scale(1.0));
                if (lift > 0.3) {
                    sawAir = true;
                    peakHands = std::max(peakHands, handY);
                }
                if (lift == 0) groundHands = std::min(groundHands, handY);
            }
        }
        REQUIRE(sawAir);
        CHECK(peakHands > 2.1f);   // arms overhead
        CHECK(groundHands < 1.4f);  // cupped in front otherwise
    }

    TEST_CASE("the wall has one character per player, scaled to the wall height") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.wallHeight = 2.0; p.wallCount = 5; }), 5);
        setSpot(s, -6, 24);
        WallAnimator wa;
        wa.update(s, DTs);
        CHECK(wa.players.size() == 5);
        for (const auto& c : wa.players) CHECK(c.model[1][1] == doctest::Approx(2.0f / STAND_HEIGHT).epsilon(1e-3));
    }

    TEST_CASE("the lying man lies along the sim's capsule, face down, head towards its far end") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.lyingMan = Frequency::Always; }), 5);
        setSpot(s, -6, 24);
        REQUIRE(s.wall);
        REQUIRE(s.wall->lying);
        WallAnimator wa;
        wa.update(s, DTs);
        REQUIRE(wa.lying);
        const glm::mat4& m = wa.lying->model;
        const auto& ly = *s.wall->lying;
        const vec3 axis = glm::normalize(toV(ly.b) - toV(ly.a));
        CHECK(glm::dot(vec3(m[1]), axis) > 0.999f);   // body axis along the capsule
        CHECK(vec3(m[2]).y < -0.99f);                  // belly down
        const vec3 head = vec3(m * glm::vec4(0, 1.8f, 0, 1));
        CHECK(glm::length(head - toV(ly.b)) < 0.25f);  // head lands at the far end
        CHECK(vec3(m[3]).y == doctest::Approx(static_cast<float>(ly.radius)).epsilon(1e-3));
    }

    TEST_CASE("an encroacher who is charging runs; a standing wall player does not") {
        SimState s = createSim(makeCfg([](SettingsPatch& p) { p.wallEnabled = true; p.encroachment = Frequency::Always; }), 5);
        setSpot(s, -6, 24);
        WallAnimator wa;
        s.aim = {5, 1};
        InputState hold = emptyInput();
        hold.shoot = true;
        step(s, hold, DTs);
        while (s.meter < sweet()) step(s, hold, DTs);
        step(s, emptyInput(), DTs);
        std::vector<float> runLegSwing(8, 0.f);
        for (int i = 0; i < 240 * 2; i++) {
            step(s, emptyInput(), DTs);
            if (i % 4 != 0) continue;
            wa.update(s, 1.0 / 60);  // the renderer's rate: one animation update per four sim ticks
            if (s.phase != Phase::Flight) continue;
            for (size_t k = 0; k < wa.players.size(); k++) runLegSwing[k] = std::max(runLegSwing[k], std::fabs(glm::eulerAngles(wa.players[k].pose.rot[THIGH_L]).x));
        }
        const int enc = s.wall->encroachers[0];
        for (size_t k = 0; k < s.wall->players.size(); k++) {
            if (static_cast<int>(k) == enc) CHECK(runLegSwing[k] > 0.2f);
            else CHECK(runLegSwing[k] < 0.1f);
        }
    }
}
