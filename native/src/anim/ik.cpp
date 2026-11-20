#include "anim/ik.h"

#include <glm/gtx/quaternion.hpp>

namespace anim {

using glm::quat;
using glm::vec3;

void solveTwoBone(Pose& pose, Bone b0, Bone b1, Bone b2, const vec3& target, const vec3& pole, float weight) {
    if (weight <= 0.f) return;
    const Skeleton& sk = Skeleton::humanoid();
    BoneMats W;
    forwardKinematics(pose, W);
    const vec3 p0 = position(W[b0]), p1 = position(W[b1]), p2 = position(W[b2]);
    const float L1 = glm::length(p1 - p0), L2 = glm::length(p2 - p1);
    const vec3 d = target - p0;
    const float dist = glm::length(d);
    if (dist < 1e-5f) return;
    const vec3 dirT = d / dist;
    const float reach = glm::clamp(dist, std::fabs(L1 - L2) * 1.001f + 1e-3f, (L1 + L2) * 0.9995f);
    const float cosA = glm::clamp((L1 * L1 + reach * reach - L2 * L2) / (2.f * L1 * reach), -1.f, 1.f);
    const float sinA = std::sqrt(1.f - cosA * cosA);
    vec3 perp = pole - dirT * glm::dot(pole, dirT);
    if (glm::length(perp) < 1e-4f) perp = std::fabs(dirT.y) < 0.9f ? glm::cross(dirT, vec3(0, 1, 0)) : glm::cross(dirT, vec3(1, 0, 0));
    perp = glm::normalize(perp);
    const vec3 mid = p0 + L1 * (cosA * dirT + sinA * perp);
    const vec3 end = p0 + dirT * reach;
    const vec3 dir1 = glm::normalize(mid - p0), dir2 = glm::normalize(end - mid);

    Pose solved = pose;
    // Rotate the upper bone so it points at the new mid joint.
    const quat parentW = sk.parent[b0] < 0 ? quat(1, 0, 0, 0) : rotationOf(W[sk.parent[b0]]);
    const quat newW0 = glm::rotation(glm::normalize(p1 - p0), dir1) * rotationOf(W[b0]);
    solved.rot[b0] = glm::normalize(glm::inverse(parentW) * newW0);
    // Then the lower bone, from the updated chain.
    BoneMats W2;
    forwardKinematics(solved, W2);
    const vec3 q1 = position(W2[b1]), q2 = position(W2[b2]);
    const quat newW1 = glm::rotation(glm::normalize(q2 - q1), dir2) * rotationOf(W2[b1]);
    solved.rot[b1] = glm::normalize(glm::inverse(newW0) * newW1);

    pose.rot[b0] = glm::normalize(glm::slerp(pose.rot[b0], solved.rot[b0], weight));
    pose.rot[b1] = glm::normalize(glm::slerp(pose.rot[b1], solved.rot[b1], weight));
}

void alignFoot(Pose& pose, bool left, float pitch, float weight) {
    if (weight <= 0.f) return;
    BoneMats W;
    forwardKinematics(pose, W);
    const Bone shin = left ? SHIN_L : SHIN_R, foot = left ? FOOT_L : FOOT_R;
    const quat desired = glm::angleAxis(-pitch, vec3(1, 0, 0));  // identity = level, facing +z
    const quat local = glm::normalize(glm::inverse(rotationOf(W[shin])) * desired);
    pose.rot[foot] = glm::normalize(glm::slerp(pose.rot[foot], local, weight));
}

}  // namespace anim
