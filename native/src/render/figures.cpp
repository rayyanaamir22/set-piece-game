#include "render/figures.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include "render/look.h"
#include "render/shapes.h"
#include "sim/layout.h"

namespace render {

using glm::mat4;
using glm::vec3;

void LitPainter::draw(const gfx::Mesh& m, const mat4& model, const vec3& srgb, float alpha, int pattern) const {
    shader->set("uModel", model);
    shader->set("uNormalMat", glm::mat3(glm::transpose(glm::inverse(model))));
    shader->set("uColor", glm::vec4(srgb, alpha));
    shader->set("uPattern", pattern);
    m.draw();
}

FigureMeshes::FigureMeshes()
    : torso_(makeCapsule(0.22f, 0.45f, 12, 4)),
      head_(makeSphere(0.14f, 12, 10)),
      leg_(makeCapsule(0.09f, 0.62f, 8, 3)),
      foot_(makeBox(0.12f, 0.08f, 0.28f)),
      arm_(makeCapsule(0.06f, 0.4f, 8, 3)),
      glove_(makeSphere(0.095f, 10, 8)) {}

void FigureMeshes::draw(const LitPainter& p, const mat4& model, const FigureColors& c, const FigurePose& pose) const {
    p.draw(torso_, model * glm::translate(mat4(1), vec3(0, 1.25f, 0)), c.shirt);
    p.draw(head_, model * glm::translate(mat4(1), vec3(0, 1.78f, 0)), c.skin);
    const vec3 boot(0.067f, 0.067f, 0.067f);
    const float hipX[2] = {0.12f, -0.12f};
    const float ang[2] = {pose.legA, pose.legB};
    for (int i = 0; i < 2; i++) {
        const mat4 hip = model * glm::translate(mat4(1), vec3(hipX[i], 0.95f, 0)) * glm::rotate(mat4(1), ang[i], vec3(1, 0, 0));
        p.draw(leg_, hip * glm::translate(mat4(1), vec3(0, -0.4f, 0)), c.shorts);
        p.draw(foot_, hip * glm::translate(mat4(1), vec3(0, -0.82f, 0.07f)), boot);
    }
    const vec3 glove(1.0f, 0.353f, 0.122f);
    const vec3 pos[2] = {pose.armAPos, pose.armBPos};
    const vec3 eul[2] = {pose.armAEuler, pose.armBEuler};
    for (int i = 0; i < 2; i++) {
        const mat4 arm = model * glm::translate(mat4(1), pos[i]) * glm::eulerAngleXYZ(eul[i].x, eul[i].y, eul[i].z);
        p.draw(arm_, arm, c.shirt);
        if (pose.gloves) p.draw(glove_, arm * glm::translate(mat4(1), vec3(0, 0.28f, 0)), glove);
    }
}

namespace {
constexpr float PI = 3.14159265358979f;
const FigureColors KICKER{hex(0xd62828), hex(0xf4f4f4), hex(0xc68c63)};
const FigureColors DECOY{hex(0xd62828), hex(0xf4f4f4), hex(0xe8b894)};
const FigureColors KEEPER{hex(0xf2c200), hex(0x1c1c1c), hex(0xe8b894)};
const unsigned SKINS[6] = {0xc68c63, 0x8d5a3b, 0xf2d0b5, 0xe8b894, 0x6b4430, 0xd9a577};
const vec3 UP(0, 1, 0);
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float clampf(float x, float lo, float hi) { return std::min(hi, std::max(lo, x)); }
}  // namespace

void PlayerViews::draw(const LitPainter& p, const sim::SimState& s) const {
    drawKicker(p, s);
    drawDecoy(p, s);
    drawWall(p, s);
    drawKeeper(p, s);
}

void PlayerViews::drawKicker(const LitPainter& p, const sim::SimState& s) const {
    const auto kp = sim::kickerPositions(s.spot, s.foot, s.runup.angle, s.runup.dist);
    const bool right = s.foot == 'R';
    // Local forward = +z: legA is at +x, legB at -x. "Kick" leg is B for a right-footed taker.
    float kick = 0, plant = 0, armL = 0, armR = 0;

    // t: <0 waiting, 0 at start of run-up, 1 at contact, >1 follow-through.
    double t;
    if (s.phase == sim::Phase::Aim || s.phase == sim::Phase::Charge) t = -1;
    else if (s.phase == sim::Phase::Runup) t = std::min(1.0, s.runupT / s.runupDur);
    else t = 1 + std::min(1.0, s.phaseT / 0.5);

    vec3 pos;
    if (t < 0) {
        pos = vec3(kp.runupStart.x, 0, kp.runupStart.z);
    } else if (t <= 1) {
        const double e = t * t * (3 - 2 * t);
        pos = vec3(kp.runupStart.x + (kp.strike.x - kp.runupStart.x) * e, 0, kp.runupStart.z + (kp.strike.z - kp.runupStart.z) * e);
        const double stride = std::sin(t * PI * 3.2);
        plant = static_cast<float>(stride * 0.7 * (1 - t));
        kick = t < 0.75 ? static_cast<float>(-stride * 0.7 * (1 - t)) : lerpf(0.9f, -1.15f, static_cast<float>((t - 0.75) / 0.25));
        armL = static_cast<float>(-stride * 0.5);
        armR = static_cast<float>(stride * 0.5);
    } else {
        pos = vec3(kp.strike.x, 0, kp.strike.z);
        kick = lerpf(-1.15f, 0, static_cast<float>(t - 1));
    }
    FigurePose pose;
    pose.legA = right ? plant : kick;
    pose.legB = right ? kick : plant;
    pose.armAEuler.x = armL;
    pose.armBEuler.x = armR;

    const float dx = static_cast<float>(s.spot.x) - pos.x, dz = static_cast<float>(s.spot.z) - pos.z;
    float yaw = std::atan2(dx, dz);
    if (std::hypot(dx, dz) < 0.3f) yaw = static_cast<float>(std::atan2(-s.spot.x, -s.spot.z));
    fig_.draw(p, glm::translate(mat4(1), pos) * glm::rotate(mat4(1), yaw, UP), KICKER, pose);
}

void PlayerViews::drawDecoy(const LitPainter& p, const sim::SimState& s) const {
    if (!(s.dummy && s.phase == sim::Phase::Runup)) return;
    const auto start = sim::kickerPositions(s.spot, s.foot == 'R' ? 'L' : 'R', s.runup.angle, s.runup.dist).runupStart;
    const sim::V3 d = sim::dirToGoal(s.spot), r = sim::rightOf(d);
    const double side = s.foot == 'R' ? 1 : -1;
    const double ex = s.spot.x + d.x * 2.6 - r.x * side * 0.9, ez = s.spot.z + d.z * 2.6 - r.z * side * 0.9;
    const double u = std::min(1.0, s.runupT / (s.dummyAt * 1.35));
    const double e = u * u * (3 - 2 * u);
    const vec3 pos(start.x + (ex - start.x) * e, 0, start.z + (ez - start.z) * e);
    const float yaw = static_cast<float>(std::atan2(ex - start.x, ez - start.z));
    const float stride = static_cast<float>(std::sin(s.runupT * 15));
    FigurePose pose;
    pose.legA = stride * 0.8f;
    pose.legB = -stride * 0.8f;
    pose.armAEuler.x = -stride * 0.6f;
    pose.armBEuler.x = stride * 0.6f;
    fig_.draw(p, glm::translate(mat4(1), pos) * glm::rotate(mat4(1), yaw, UP), DECOY, pose);
}

void PlayerViews::drawWall(const LitPainter& p, const sim::SimState& s) const {
    if (!s.wall) return;
    const sim::WallState& w = *s.wall;
    if (w.lying) {
        const auto& ly = *w.lying;
        const double dx = ly.b.x - ly.a.x, dz = ly.b.z - ly.a.z;
        // Lies along the wall axis, feet at `a`, head towards `b`. Euler order YXZ: Ry * Rz(-90deg).
        const mat4 m = glm::translate(mat4(1), vec3(ly.a.x, ly.radius, ly.a.z)) *
                       glm::rotate(mat4(1), static_cast<float>(std::atan2(-dz, dx)), UP) * glm::rotate(mat4(1), -PI / 2, vec3(0, 0, 1));
        fig_.draw(p, m, FigureColors{hex(0x1f4fbf), hex(0x14233f), hex(SKINS[2])}, FigurePose{});
    }
    const float scale = static_cast<float>(w.height) / FIGURE_HEIGHT;
    for (size_t i = 0; i < w.players.size(); i++) {
        const sim::V3& q = w.players[i];
        const float lift = static_cast<float>(w.lift[i]);
        FigurePose pose;
        // Arms go up while airborne, otherwise cupped in front.
        const bool up = lift > 0.02f;
        pose.armAPos = vec3(0.2f, 1.1f, 0.18f);
        pose.armBPos = vec3(-0.2f, 1.1f, 0.18f);
        pose.armAEuler = vec3(up ? -2.7f : -0.9f, 0, 0.25f);
        pose.armBEuler = vec3(up ? -2.7f : -0.9f, 0, -0.25f);
        const mat4 m = glm::translate(mat4(1), vec3(q.x, lift * scale, q.z)) *
                       glm::rotate(mat4(1), static_cast<float>(std::atan2(w.facing.x, w.facing.z)), UP) * glm::scale(mat4(1), vec3(scale));
        fig_.draw(p, m, FigureColors{hex(0x1f4fbf), hex(0x14233f), hex(SKINS[i % 6])}, pose);
    }
}

void PlayerViews::drawKeeper(const LitPainter& p, const sim::SimState& s) const {
    if (!s.keeper) return;
    const sim::KeeperState& k = *s.keeper;
    glm::vec3 dir(static_cast<float>(k.b.x - k.a.x), static_cast<float>(k.b.y - k.a.y), 0);
    const float len = glm::length(dir);
    dir = len > 1e-3f ? dir / len : UP;
    // quaternion from UP to dir (setFromUnitVectors), then roll about his own long axis as he stretches out.
    glm::quat q = glm::rotation(UP, dir);
    const bool dive = k.phase == sim::KeeperPhase::Diving && k.kind == sim::KeeperMove::Dive;
    const float u = dive ? clampf(static_cast<float>((s.flightT - k.t0) / k.dur), 0, 1) : 0;
    const float side = (k.endB.x - k.startB.x) > 0 ? 1.f : ((k.endB.x - k.startB.x) < 0 ? -1.f : 1.f);
    q = q * glm::angleAxis(side * 0.7f * u, UP);
    const mat4 m = glm::translate(mat4(1), vec3(k.a.x, k.a.y, k.a.z)) * glm::mat4_cast(q) *
                   glm::scale(mat4(1), vec3(1, clampf(len / 1.9f, 0.45f, 1.2f), 1));
    FigurePose pose;
    pose.gloves = true;
    if (dive) {
        // Arms straight above the head along the dive line.
        pose.armAPos = vec3(0.16f, 2.02f, 0);
        pose.armBPos = vec3(-0.16f, 2.02f, 0);
    } else {
        // Ready stance: arms out and slightly bent.
        pose.armAPos = vec3(0.46f, 1.4f, 0.05f);
        pose.armBPos = vec3(-0.46f, 1.4f, 0.05f);
        pose.armAEuler = vec3(0, 0, -1.0f);
        pose.armBEuler = vec3(0, 0, 1.0f);
    }
    fig_.draw(p, m, KEEPER, pose);
}

}  // namespace render
