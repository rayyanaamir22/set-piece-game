// Analytic two-bone IK (legs and arms) on a Pose, in model space.
#pragma once
#include "anim/skeleton.h"

namespace anim {

// Moves the end joint (the head of `end`) of the chain upper -> lower -> end towards `target`, bending the middle
// joint towards `pole`. `weight` blends the solved rotations in. Unreachable targets are clamped to full extension.
void solveTwoBone(Pose& pose, Bone upper, Bone lower, Bone end, const glm::vec3& target, const glm::vec3& pole, float weight = 1.f);

inline void solveLeg(Pose& p, bool left, const glm::vec3& ankle, float w = 1.f) {
    solveTwoBone(p, left ? THIGH_L : THIGH_R, left ? SHIN_L : SHIN_R, left ? FOOT_L : FOOT_R, ankle, glm::vec3(0, 0, 1), w);  // knee forward
}
inline void solveArm(Pose& p, bool left, const glm::vec3& wrist, const glm::vec3& pole = glm::vec3(0, -0.3f, -1), float w = 1.f) {
    solveTwoBone(p, left ? UARM_L : UARM_R, left ? LARM_L : LARM_R, left ? HAND_L : HAND_R, wrist, pole, w);  // elbow back and down
}

// Rotates a foot so its sole is level in model space (toes pitched up by `pitch` radians), blended by `weight`.
// Call after the leg IK so the boot lies flat on the grass / strikes the ball with a level instep.
void alignFoot(Pose& pose, bool left, float pitch, float weight = 1.f);

}  // namespace anim
