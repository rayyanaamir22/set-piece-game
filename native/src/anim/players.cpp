#include "anim/players.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "anim/ik.h"
#include "sim/layout.h"

namespace anim {

using glm::mat4;
using glm::quat;
using glm::vec3;
using sim::Phase;

namespace {
constexpr float PI = 3.14159265f;
const vec3 UP(0, 1, 0);

float smooth(float e0, float e1, float x) {
    const float t = glm::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
float lerpAngle(float a, float b, float t) {
    float d = std::fmod(b - a + PI, 2 * PI);
    if (d < 0) d += 2 * PI;
    return a + (d - PI) * t;
}
vec3 v(const sim::V3& a) { return vec3(static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)); }
mat4 placement(vec3 pos, float yaw, float scale = 1.f) {
    return glm::translate(mat4(1.f), pos) * glm::rotate(mat4(1.f), yaw, UP) * glm::scale(mat4(1.f), vec3(scale));
}
vec3 toModel(const mat4& model, vec3 world) { return vec3(glm::inverse(model) * glm::vec4(world, 1.f)); }

// ------------------------------------------------------------------ poses
Pose standing(float time) {
    Pose p;
    const float breathe = std::sin(time * 2.2f);
    p.rot[UARM_L] = euler(0.03f, 0, 0.12f);
    p.rot[UARM_R] = euler(0.03f, 0, -0.12f);
    p.rot[LARM_L] = euler(-0.15f, 0, 0);
    p.rot[LARM_R] = euler(-0.15f, 0, 0);
    p.rot[CHEST] = euler(0.012f * breathe, 0, 0);
    p.rot[HEAD] = euler(-0.01f * breathe, 0, 0);
    p.hips.y = 0.004f * breathe;
    return p;
}

// Run cycle. `phase` in cycles, `amount` 0..1 (0 = standing).
void runCycle(Pose& p, float phase, float amount) {
    if (amount <= 0.001f) return;
    const float th = 2 * PI * phase;
    const float s = std::sin(th), c = std::cos(th);
    const float swing = 0.75f * amount * s;
    // Legs: thigh x < 0 is forward. Knee (shin x > 0) flexes most while the leg recovers forward.
    p.rot[THIGH_L] = glm::slerp(p.rot[THIGH_L], euler(-swing, 0, 0), 1.f);
    p.rot[THIGH_R] = glm::slerp(p.rot[THIGH_R], euler(swing, 0, 0), 1.f);
    p.rot[SHIN_L] = euler(amount * (0.2f + 0.95f * std::max(0.f, c)), 0, 0);
    p.rot[SHIN_R] = euler(amount * (0.2f + 0.95f * std::max(0.f, -c)), 0, 0);
    // Arms counter-swing with the elbows bent.
    p.rot[UARM_L] = euler(0.65f * amount * s, 0, 0.12f);
    p.rot[UARM_R] = euler(-0.65f * amount * s, 0, -0.12f);
    p.rot[LARM_L] = euler(-(0.5f + 0.7f * amount), 0, 0);
    p.rot[LARM_R] = euler(-(0.5f + 0.7f * amount), 0, 0);
    p.rot[SPINE] = euler(0.22f * amount, 0.12f * amount * s, 0);
    p.rot[CHEST] = euler(0.05f * amount, -0.10f * amount * s, 0);
    p.hips.y = -0.03f * amount - 0.025f * amount * std::cos(2 * th);
}

// Wall defenders: hands cupped in front of the body.
void wallStance(Pose& p) {
    p.rot[UARM_L] = euler(-0.15f, 0, 0.08f);
    p.rot[UARM_R] = euler(-0.15f, 0, -0.08f);
    p.rot[LARM_L] = euler(-1.35f, 0, -0.25f);
    p.rot[LARM_R] = euler(-1.35f, 0, 0.25f);
    p.rot[SPINE] = euler(0.05f, 0, 0);
}

void tuck(Pose& p, float w) {  // knees up, as in a jump
    p.rot[THIGH_L] = euler(-0.75f * w, 0, 0);
    p.rot[THIGH_R] = euler(-0.75f * w, 0, 0);
    p.rot[SHIN_L] = euler(1.2f * w, 0, 0);
    p.rot[SHIN_R] = euler(1.2f * w, 0, 0);
}

Pose celebrate(float t) {
    Pose p = standing(t);
    const float hop = std::fabs(std::sin(t * 2 * PI * 1.6f));
    p.hips.y = 0.14f * hop;
    p.rot[UARM_L] = euler(0, 0, 2.55f + 0.1f * std::sin(t * 12));
    p.rot[UARM_R] = euler(0, 0, -2.55f - 0.1f * std::sin(t * 12));
    p.rot[LARM_L] = euler(-0.3f, 0, 0);
    p.rot[LARM_R] = euler(-0.3f, 0, 0);
    p.rot[CHEST] = euler(-0.12f, 0, 0);
    p.rot[HEAD] = euler(-0.18f, 0, 0);
    tuck(p, 0.55f * hop);
    return p;
}

Pose disappointed(float t) {
    Pose p = standing(t);
    p.rot[UARM_L] = euler(-2.35f, 0, 0.45f);
    p.rot[UARM_R] = euler(-2.35f, 0, -0.45f);
    p.rot[LARM_L] = euler(-1.95f, 0, 0);
    p.rot[LARM_R] = euler(-1.95f, 0, 0);
    p.rot[HEAD] = euler(0.28f, 0, 0);
    p.rot[CHEST] = euler(0.12f, 0, 0);
    return p;
}

// Keeper
Pose keeperReady(float time) {
    Pose p = standing(time);
    p.hips.y = -0.07f + 0.004f * std::sin(time * 3);
    p.rot[SPINE] = euler(0.12f, 0, 0);
    p.rot[HEAD] = euler(-0.1f, 0, 0);
    return p;
}
}  // namespace

// ================================================================== taker
namespace {
vec3 ankleWorld(const Character& c, Bone foot) {
    BoneMats W;
    forwardKinematics(c.pose, W);
    return vec3(c.model * glm::vec4(position(W[foot]), 1.f));
}
}  // namespace
vec3 TakerAnimator::kickAnkleWorld() const { return ankleWorld(character, kickLeft ? FOOT_L : FOOT_R); }
vec3 TakerAnimator::plantAnkleWorld() const { return ankleWorld(character, kickLeft ? FOOT_R : FOOT_L); }

void TakerAnimator::update(const sim::SimState& s, double dtD) {
    const float dt = static_cast<float>(dtD);
    const sim::PhysicsParams& P = s.cfg->physics;
    const vec3 spot = v(s.spot);
    const vec3 ball(spot.x, static_cast<float>(P.radius), spot.z);
    kickLeft = s.foot == 'L';

    // Direction of the shot: the real launch once it exists, otherwise the aim preview, otherwise "at goal".
    vec3 F;
    if (s.pending) F = vec3(static_cast<float>(s.pending->vel.x), 0, static_cast<float>(s.pending->vel.z));
    else if (s.preview) F = vec3(std::sin(static_cast<float>(s.preview->azimuth)), 0, -std::cos(static_cast<float>(s.preview->azimuth)));
    else F = v(sim::dirToGoal(s.spot));
    F = glm::length(F) > 1e-4f ? glm::normalize(F) : vec3(0, 0, -1);
    const vec3 R(-F.z, 0, F.x);  // the shooter's right
    const float side = s.foot == 'R' ? -1.f : 1.f;
    const vec3 strikeRoot = spot + R * side * 0.30f - F * 0.40f;
    const auto kp = sim::kickerPositions(s.spot, s.foot, s.runup.angle, s.runup.dist);
    const vec3 start = v(kp.runupStart);
    const float shotYaw = std::atan2(F.x, F.z);

    vec3 pos = start;
    float yaw = 0;
    Pose pose = standing(static_cast<float>(resultT_));
    Kind want = Kind::Play;
    const bool striking = s.phase == Phase::Flight || s.phase == Phase::Result;

    const vec3 contactAnkle = ball - F * (static_cast<float>(P.radius) + 0.21f) + vec3(0, 0.10f - ball.y, 0);
    const vec3 plantAnkle = spot + R * side * 0.33f - F * 0.10f + vec3(0, 0.085f, 0);

    float tIk = 0, wPlant = 0, kickW = 0;  // outputs of the phase logic, applied below
    vec3 kickTarget = contactAnkle;

    if (s.phase == Phase::Aim || s.phase == Phase::Charge) {
        sinceStrike_ = -1;
        runPhase_ = 0;
        speed_ = 0;
        hasLast_ = false;
        pos = start;
        yaw = std::atan2(spot.x - start.x, spot.z - start.z);
        resultT_ += dt;
        pose = standing(static_cast<float>(resultT_));
    } else if (s.phase == Phase::Runup) {
        sinceStrike_ = -1;
        const float t = static_cast<float>(glm::clamp(s.runupT / std::max(s.runupDur, 1e-6), 0.0, 1.0));
        const float e = smooth(0, 1, t);
        pos = glm::mix(start, strikeRoot, e);
        const vec3 dir = strikeRoot - start;
        const float runYaw = std::atan2(dir.x, dir.z);
        yaw = lerpAngle(runYaw, shotYaw, smooth(0.45f, 0.9f, t));
        if (hasLast_ && dt > 1e-5f) {
            const float step = glm::length(pos - lastPos_);
            runPhase_ += step / 1.9f;
            speed_ += (step / dt - speed_) * 0.35f;
        }
        const float amount = glm::clamp(speed_ / 3.2f, 0.f, 1.f) * (1.f - 0.55f * smooth(0.8f, 1.f, t));
        pose = standing(0);
        runCycle(pose, runPhase_, amount);
        // Kicking leg winds back, then IK takes the foot to the ball for the launch tick (t == 1).
        const float wWind = smooth(0.52f, 0.80f, t);
        const bool kl = kickLeft;
        pose.rot[kl ? THIGH_L : THIGH_R] = glm::slerp(pose.rot[kl ? THIGH_L : THIGH_R], euler(0.95f, 0, 0), wWind);
        pose.rot[kl ? SHIN_L : SHIN_R] = glm::slerp(pose.rot[kl ? SHIN_L : SHIN_R], euler(1.35f, 0, 0), wWind);
        pose.rot[SPINE] = glm::slerp(pose.rot[SPINE], euler(-0.12f, 0, 0), wWind * (1.f - smooth(0.8f, 1.f, t)));
        // Balance arm on the plant side swings out.
        pose.rot[kl ? UARM_R : UARM_L] = glm::slerp(pose.rot[kl ? UARM_R : UARM_L], euler(0, 0, (kl ? -1.f : 1.f) * 1.05f), wWind);
        tIk = smooth(0.80f, 1.0f, t);
        wPlant = smooth(0.5f, 0.85f, t);
        kickW = tIk;
        // Sink into the strike: lower hips early enough that the planted foot is within reach of its pin.
        pose.hips.y -= 0.10f * smooth(0.45f, 0.82f, t);
        pose.rot[SPINE] = glm::slerp(pose.rot[SPINE], euler(0.2f, 0, 0), tIk);
    } else if (striking) {
        if (sinceStrike_ < 0) sinceStrike_ = 0;
        sinceStrike_ += dt;
        const float tf = static_cast<float>(sinceStrike_);
        const float slide = smooth(0, 0.6f, tf);
        pos = strikeRoot + F * 0.18f * slide;
        yaw = shotYaw;
        pose = standing(static_cast<float>(sinceStrike_));
        const float amount = std::max(0.f, 1.f - tf / 0.5f) * 0.45f;
        runPhase_ += dt * 1.8f * amount;
        runCycle(pose, runPhase_, amount * 0.5f);
        // The kicking foot follows through up and forward, then relaxes.
        const float s1 = smooth(0, 0.22f, tf);
        kickTarget = contactAnkle + F * 0.55f * s1 + vec3(0, 0.38f * s1, 0);
        kickW = 1.f - smooth(0.4f, 0.95f, tf);
        wPlant = 1.f - smooth(0.25f, 0.7f, tf);
        pose.hips.y -= 0.10f * std::max(kickW, wPlant);
        pose.rot[SPINE] = glm::slerp(standing(0).rot[SPINE], euler(0.25f, 0, 0), kickW);
        // Result: celebrate a goal, hold the head after anything else.
        if (s.phase == Phase::Result && s.result && tf > 1.0f) {
            want = s.result->type == sim::ResultType::Goal ? Kind::Celebrate : Kind::Disappoint;
            resultT_ = tf;
        }
    }

    // ---- foot IK (model space of the final placement)
    const mat4 model = placement(pos, yaw);
    if (kickW > 0.001f) {
        solveLeg(pose, kickLeft, toModel(model, kickTarget), kickW);
        alignFoot(pose, kickLeft, 0.0f, kickW);  // a level instep into the ball
    }
    if (wPlant > 0.001f) {
        solveLeg(pose, !kickLeft, toModel(model, plantAnkle), wPlant);
        alignFoot(pose, !kickLeft, 0.0f, wPlant);  // boot flat on the grass
    }

    // ---- result poses and crossfades
    Pose target = pose;
    if (want == Kind::Celebrate) target = celebrate(static_cast<float>(sinceStrike_ - 1.0));
    else if (want == Kind::Disappoint) target = disappointed(static_cast<float>(sinceStrike_));
    if (want != kind_) {
        fadeFrom_ = out_;
        fadeT_ = 0;
        kind_ = want;
    }
    fadeT_ += dt;
    const float fadeDur = 0.3f;
    out_ = fadeT_ < fadeDur ? blend(fadeFrom_, target, smooth(0, fadeDur, fadeT_)) : target;

    lastPos_ = pos;
    hasLast_ = true;
    character.role = Role::Taker;
    character.model = model;
    character.pose = out_;
}

// ================================================================== decoy
void DecoyAnimator::update(const sim::SimState& s, double dtD) {
    const float dt = static_cast<float>(dtD);
    character.reset();
    if (!(s.dummy && s.phase == Phase::Runup)) {
        has_ = false;
        return;
    }
    const auto start = sim::kickerPositions(s.spot, s.foot == 'R' ? 'L' : 'R', s.runup.angle, s.runup.dist).runupStart;
    const sim::V3 d = sim::dirToGoal(s.spot), r = sim::rightOf(d);
    const double side = s.foot == 'R' ? 1 : -1;
    const double ex = s.spot.x + d.x * 2.6 - r.x * side * 0.9, ez = s.spot.z + d.z * 2.6 - r.z * side * 0.9;
    const double u = std::min(1.0, s.runupT / (s.dummyAt * 1.35));
    const double e = u * u * (3 - 2 * u);
    const vec3 pos(static_cast<float>(start.x + (ex - start.x) * e), 0, static_cast<float>(start.z + (ez - start.z) * e));
    const float yaw = static_cast<float>(std::atan2(ex - start.x, ez - start.z));
    if (has_ && dt > 1e-5f) phase_ += glm::length(pos - last_) / 1.9f;
    const float amount = glm::clamp(glm::length(pos - last_) / std::max(dt, 1e-5f) / 3.2f, 0.f, 1.f);
    Pose p = standing(0);
    runCycle(p, phase_, has_ ? std::max(amount, 0.6f) : 0.f);
    last_ = pos;
    has_ = true;
    Character c;
    c.role = Role::Decoy;
    c.model = placement(pos, yaw);
    c.pose = p;
    character = c;
}

// ================================================================== wall
void WallAnimator::update(const sim::SimState& s, double dtD) {
    const float dt = static_cast<float>(dtD);
    players.clear();
    lying.reset();
    if (!s.wall) {
        st_.clear();
        return;
    }
    const sim::WallState& w = *s.wall;
    st_.resize(w.players.size());
    const float scale = static_cast<float>(w.height) / STAND_HEIGHT;
    const vec3 facing = v(w.facing);
    const float yaw = std::atan2(facing.x, facing.z);

    // How long since the jump clock started (strike jumps count from the strike, the decoy's from its run-up beat).
    bool haveClock = false;
    double tNow = 0;
    if (s.phase == Phase::Flight || s.phase == Phase::Result) {
        haveClock = true;
        tNow = s.flightT;
    } else if (s.phase == Phase::Runup && s.dummyDone) {
        haveClock = true;
        tNow = s.runupT - s.dummyAt;
    }

    for (size_t i = 0; i < w.players.size(); i++) {
        PerPlayer& st = st_[i];
        const vec3 p = v(w.players[i]);
        if (st.has && dt > 1e-5f) {
            const vec3 vel = (p - st.last) / dt;
            st.fwdSpeed += (std::max(0.f, glm::dot(vel, facing)) - st.fwdSpeed) * 0.3f;
            st.phase += std::max(0.f, glm::dot(p - st.last, facing)) / 1.9f;
        }
        st.last = p;
        st.has = true;

        Pose pose = standing(static_cast<float>(i) * 0.7f + static_cast<float>(s.phaseT));
        wallStance(pose);
        const float amount = st.fwdSpeed > 0.25f ? glm::clamp(st.fwdSpeed / 3.4f, 0.25f, 1.f) : 0.f;
        if (amount > 0) {
            Pose run = standing(0);
            runCycle(run, st.phase, amount);
            pose = blend(pose, run, smooth(0.f, 0.5f, amount));
        }
        // Jump: crouch just before take-off, arms up while airborne, knees tucked at the top.
        const float lift = static_cast<float>(w.lift[i]);
        float crouch = 0;
        float peak = 0.4f;
        if (w.jump && i < w.jump->height.size() && w.jump->height[i] > 0) {
            peak = static_cast<float>(w.jump->height[i]);
            if (haveClock) {
                const double u = (tNow - w.jump->start[i]);
                if (lift <= 1e-4 && u > -0.10 && u < 0.0) crouch = static_cast<float>((u + 0.10) / 0.10);
            }
        }
        const float air = glm::clamp(lift / std::max(peak, 0.05f), 0.f, 1.f);
        if (crouch > 0) {
            Pose c = pose;
            c.hips.y -= 0.22f * crouch;
            c.rot[THIGH_L] = euler(-0.95f * crouch, 0, 0);
            c.rot[THIGH_R] = euler(-0.95f * crouch, 0, 0);
            c.rot[SHIN_L] = euler(1.7f * crouch, 0, 0);
            c.rot[SHIN_R] = euler(1.7f * crouch, 0, 0);
            c.rot[SPINE] = euler(0.3f * crouch, 0, 0);
            c.rot[UARM_L] = euler(0.7f * crouch, 0, 0.1f);
            c.rot[UARM_R] = euler(0.7f * crouch, 0, -0.1f);
            pose = c;
        }
        if (lift > 0.005f) {
            const float up = smooth(0.f, 0.3f, air);
            Pose a = pose;
            a.rot[UARM_L] = euler(-2.9f, 0, 0.12f);
            a.rot[UARM_R] = euler(-2.9f, 0, -0.12f);
            a.rot[LARM_L] = euler(-0.15f, 0, 0);
            a.rot[LARM_R] = euler(-0.15f, 0, 0);
            tuck(a, 0.5f * air);
            pose = blend(pose, a, up);
        }
        Character c;
        c.role = Role::Wall;
        c.variant = static_cast<int>(i);
        c.model = placement(vec3(p.x, lift * scale, p.z), yaw, scale);
        c.pose = pose;
        players.push_back(c);
    }

    if (w.lying) {
        const auto& ly = *w.lying;
        vec3 d = v(ly.b) - v(ly.a);
        d.y = 0;
        d = glm::normalize(d);
        // Face down, feet at `a`, head towards `b`: model +y -> d, model +z -> world down.
        const vec3 zax(0, -1, 0);
        const vec3 xax = glm::cross(d, zax);
        mat4 m(1.f);
        m[0] = glm::vec4(xax, 0);
        m[1] = glm::vec4(d, 0);
        m[2] = glm::vec4(zax, 0);
        m[3] = glm::vec4(v(ly.a).x, static_cast<float>(ly.radius), v(ly.a).z, 1);
        Pose p = standing(0);
        p.rot[UARM_L] = euler(2.9f, 0, 0.2f);  // arms stretched ahead of the head
        p.rot[UARM_R] = euler(2.9f, 0, -0.2f);
        p.rot[LARM_L] = euler(0.1f, 0, 0);
        p.rot[LARM_R] = euler(0.1f, 0, 0);
        p.rot[HEAD] = euler(-0.5f, 0, 0);
        Character c;
        c.role = Role::Lying;
        c.variant = 2;
        c.model = m;
        c.pose = p;
        lying = c;
    }
}

// ================================================================== keeper
void KeeperAnimator::update(const sim::SimState& s, double dtD) {
    const float dt = static_cast<float>(dtD);
    character.reset();
    if (!s.keeper) {
        hasLast_ = false;
        wasLying_ = false;
        return;
    }
    const sim::KeeperState& k = *s.keeper;
    const sim::KeeperAi& ai = s.keeperAi;
    const vec3 a = v(k.a), b = v(k.b);
    const float len = glm::length(glm::vec2(b.x - a.x, b.y - a.y));
    (void)ai;
    const bool moving = k.phase == sim::KeeperPhase::Diving;
    const bool dive = moving && k.kind == sim::KeeperMove::Dive;
    const bool step = moving && k.kind == sim::KeeperMove::Step;

    // Touch reactions.
    if (s.keeperTouch != lastTouch_) {
        touchT_ = 0;
        lastTouch_ = s.keeperTouch;
    }
    if (s.keeperTouch) touchT_ += dt;

    // Getting up: the sim resets him to his feet for the next attempt; play a rise from kneeling.
    if (wasLying_ && !dive && !step && k.phase != sim::KeeperPhase::Diving) getUpT_ = 0;
    wasLying_ = dive;
    getUpT_ += dt;

    mat4 model(1.f);
    Pose pose = keeperReady(static_cast<float>(s.phaseT));
    std::array<vec3, 2> tipTargetWorld{};  // left (+x), right (-x)
    bool useTips = false;

    if (dive) {
        const vec3 dir = len > 1e-3f ? glm::normalize(vec3(b.x - a.x, b.y - a.y, 0)) : UP;
        const float u = glm::clamp(static_cast<float>((s.flightT - k.t0) / k.dur), 0.f, 1.f);
        const float side = (k.endB.x - k.startB.x) > 0 ? 1.f : ((k.endB.x - k.startB.x) < 0 ? -1.f : 1.f);
        quat q = glm::rotation(UP, dir) * glm::angleAxis(side * 0.7f * u, UP);
        const float scale = glm::clamp(len / STRETCH_LEN, 0.9f, 1.15f);
        model = glm::translate(mat4(1.f), a) * glm::mat4_cast(q) * glm::scale(mat4(1.f), vec3(scale));
        pose = standing(0);
        pose.rot[THIGH_L] = euler(0.12f, 0, 0.10f);
        pose.rot[THIGH_R] = euler(0.12f, 0, -0.10f);
        pose.rot[SHIN_L] = euler(0.25f, 0, 0);
        pose.rot[SHIN_R] = euler(0.25f, 0, 0);
        pose.rot[UARM_L] = euler(PI, 0, 0.06f);
        pose.rot[UARM_R] = euler(PI, 0, -0.06f);
        pose.rot[LARM_L] = euler(0, 0, 0);
        pose.rot[LARM_R] = euler(0, 0, 0);
        const vec3 bx = q * vec3(1, 0, 0);
        tipTargetWorld = {b + bx * 0.05f, b - bx * 0.05f};
        useTips = true;
    } else if (step) {
        model = glm::translate(mat4(1.f), vec3(a.x, a.y, a.z));
        if (hasLast_ && dt > 1e-5f) stepPhase_ += std::fabs(a.x - lastX_) / 0.9f;
        const float dx = hasLast_ && dt > 1e-5f ? (a.x - lastX_) / dt : 0.f;
        pose = keeperReady(0);
        const float sw = std::sin(2 * PI * stepPhase_);
        pose.rot[THIGH_L] = euler(-0.3f * sw * 0.6f, 0, 0.2f);
        pose.rot[THIGH_R] = euler(0.3f * sw * 0.6f, 0, -0.2f);
        pose.rot[SPINE] = euler(0.12f, 0, -glm::clamp(dx * 0.05f, -0.25f, 0.25f));
        tipTargetWorld = {b + vec3(0.05f, 0, 0), b - vec3(0.05f, 0, 0)};
        useTips = true;
        pose.hips.y = -0.10f;
    } else {
        model = glm::translate(mat4(1.f), vec3(a.x, a.y, a.z));
        const float drop = glm::clamp(1.9f - len, 0.f, 0.5f) * 0.6f;
        pose = keeperReady(static_cast<float>(s.phaseT));
        pose.hips.y -= drop;
        // Arms out: this is the hitbox's arm capsule (x +- 0.8 at 1.5 m).
        tipTargetWorld = {vec3(b.x + 0.82f, 1.5f, b.z), vec3(b.x - 0.82f, 1.5f, b.z)};
        useTips = true;
    }
    lastX_ = a.x;
    hasLast_ = true;

    // Feet: keep them on the ground when upright (the knees bend as the hips drop).
    if (!dive) {
        solveLeg(pose, true, vec3(0.16f, 0.08f, 0.04f), 1.f);
        solveLeg(pose, false, vec3(-0.16f, 0.08f, 0.04f), 1.f);
    }

    // Hands: IK to the sim's hand point (or the arm capsule while standing).
    if (useTips) {
        BoneMats W;
        forwardKinematics(pose, W);
        for (int side = 0; side < 2; side++) {
            vec3 tipM = toModel(model, tipTargetWorld[side]);
            if (s.keeperTouch == sim::KeeperTouch::Hold) {  // catch: pull the ball to the chest
                const vec3 chest(side == 0 ? 0.07f : -0.07f, 1.2f, 0.28f);
                tipM = glm::mix(tipM, chest, smooth(0, 0.25f, touchT_));
            } else if (s.keeperTouch == sim::KeeperTouch::Touch) {  // parry: arms recoil
                tipM += vec3(0, 0.06f, -0.14f) * std::sin(PI * glm::clamp(touchT_ / 0.3f, 0.f, 1.f));
            }
            const vec3 shoulder = position(W[side == 0 ? UARM_L : UARM_R]);
            // The hand point is the fingertip; the IK solves for the wrist. Start from a straight-arm guess and
            // correct by the measured fingertip error (a bent arm points the hand elsewhere).
            vec3 wrist = tipM - glm::normalize(tipM - shoulder) * HAND_LEN;
            const Bone elbowB = side == 0 ? LARM_L : LARM_R, handB = side == 0 ? HAND_L : HAND_R;
            for (int iter = 0; iter < 4; iter++) {
                solveArm(pose, side == 0, wrist, dive ? vec3(0, 0, -1) : vec3(0, -0.4f, -1), 1.f);
                BoneMats W2;
                forwardKinematics(pose, W2);
                const vec3 el = position(W2[elbowB]), wr = position(W2[handB]);
                const vec3 tip = wr + glm::normalize(wr - el) * HAND_LEN;
                const vec3 err = tipM - tip;
                if (glm::length(err) < 0.004f) break;
                wrist += err;
            }
        }
    }

    // Get-up crossfade: from kneeling to the ready stance.
    if (getUpT_ < 0.9f && !dive) {
        Pose kneel = standing(0);
        kneel.hips.y = -0.55f;
        kneel.rot[THIGH_L] = euler(-1.5f, 0, 0.1f);
        kneel.rot[THIGH_R] = euler(-0.4f, 0, -0.1f);
        kneel.rot[SHIN_L] = euler(1.5f, 0, 0);
        kneel.rot[SHIN_R] = euler(1.9f, 0, 0);
        kneel.rot[SPINE] = euler(0.8f, 0, 0);
        kneel.rot[UARM_L] = euler(-0.6f, 0, 0.3f);
        kneel.rot[UARM_R] = euler(-0.6f, 0, -0.3f);
        pose = blend(kneel, pose, smooth(0.f, 0.9f, getUpT_));
    }

    // Fingertips (for the exit check): wrist + forearm direction * hand length.
    {
        BoneMats W;
        forwardKinematics(pose, W);
        for (int side = 0; side < 2; side++) {
            const vec3 el = position(W[side == 0 ? LARM_L : LARM_R]), wr = position(W[side == 0 ? HAND_L : HAND_R]);
            const vec3 tip = wr + glm::normalize(wr - el) * HAND_LEN;
            tips[side] = vec3(model * glm::vec4(tip, 1.f));
        }
    }

    Character c;
    c.role = Role::Keeper;
    c.model = model;
    c.pose = pose;
    character = c;
}

}  // namespace anim
