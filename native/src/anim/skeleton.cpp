#include "anim/skeleton.h"

#include <glm/gtc/matrix_transform.hpp>

namespace anim {

using glm::mat4;
using glm::quat;
using glm::vec3;

const Skeleton& Skeleton::humanoid() {
    static const Skeleton sk = [] {
        Skeleton s{};
        auto set = [&](Bone b, int parent, vec3 local) {
            s.parent[b] = parent;
            s.restLocal[b] = local;
        };
        set(ROOT, -1, {0, 0, 0});
        set(HIPS, ROOT, {0, 0.95f, 0});
        set(SPINE, HIPS, {0, 0.17f, 0});
        set(CHEST, SPINE, {0, 0.28f, 0});
        set(NECK, CHEST, {0, 0.20f, 0});
        set(HEAD, NECK, {0, 0.08f, 0});
        set(UARM_L, CHEST, {0.22f, 0.10f, 0});
        set(LARM_L, UARM_L, {0, -UARM_LEN, 0});
        set(HAND_L, LARM_L, {0, -LARM_LEN, 0});
        set(UARM_R, CHEST, {-0.22f, 0.10f, 0});
        set(LARM_R, UARM_R, {0, -UARM_LEN, 0});
        set(HAND_R, LARM_R, {0, -LARM_LEN, 0});
        set(THIGH_L, HIPS, {0.10f, 0, 0});
        set(SHIN_L, THIGH_L, {0, -THIGH_LEN, 0});
        set(FOOT_L, SHIN_L, {0, -SHIN_LEN, 0});
        set(THIGH_R, HIPS, {-0.10f, 0, 0});
        set(SHIN_R, THIGH_R, {0, -THIGH_LEN, 0});
        set(FOOT_R, SHIN_R, {0, -SHIN_LEN, 0});
        for (int b = 0; b < BONE_COUNT; b++) s.restWorld[b] = (s.parent[b] < 0 ? vec3(0) : s.restWorld[s.parent[b]]) + s.restLocal[b];
        return s;
    }();
    return sk;
}

quat euler(float x, float y, float z) {
    return glm::angleAxis(x, vec3(1, 0, 0)) * glm::angleAxis(y, vec3(0, 1, 0)) * glm::angleAxis(z, vec3(0, 0, 1));
}
vec3 position(const mat4& m) { return vec3(m[3]); }
quat rotationOf(const mat4& m) { return glm::normalize(glm::quat_cast(glm::mat3(m))); }

void forwardKinematics(const Pose& pose, BoneMats& world) {
    const Skeleton& sk = Skeleton::humanoid();
    for (int b = 0; b < BONE_COUNT; b++) {
        vec3 t = sk.restLocal[b];
        if (b == HIPS) t += pose.hips;
        const mat4 local = glm::translate(mat4(1.f), t) * glm::mat4_cast(pose.rot[b]);
        world[b] = sk.parent[b] < 0 ? local : world[sk.parent[b]] * local;
    }
}

void skinMatrices(const Pose& pose, BoneMats& skin) {
    const Skeleton& sk = Skeleton::humanoid();
    BoneMats w;
    forwardKinematics(pose, w);
    for (int b = 0; b < BONE_COUNT; b++) skin[b] = w[b] * glm::translate(mat4(1.f), -sk.restWorld[b]);
}

Pose blend(const Pose& a, const Pose& b, float w) {
    Pose o;
    for (int i = 0; i < BONE_COUNT; i++) o.rot[i] = glm::normalize(glm::slerp(a.rot[i], b.rot[i], w));
    o.hips = glm::mix(a.hips, b.hips, w);
    return o;
}

}  // namespace anim
