// A small humanoid skeleton, poses and forward kinematics (pure maths, no GL).
// Model space: origin on the ground between the feet, +y up, the character faces +z.
// Bones named L sit at +x (the character's left), R at -x.
#pragma once
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace anim {

enum Bone : int {
    ROOT, HIPS, SPINE, CHEST, NECK, HEAD,
    UARM_L, LARM_L, HAND_L,
    UARM_R, LARM_R, HAND_R,
    THIGH_L, SHIN_L, FOOT_L,
    THIGH_R, SHIN_R, FOOT_R,
    BONE_COUNT
};
inline constexpr int MAX_GPU_BONES = 24;  // size of the shader's bone array (<= 64 per the plan)

// Local rotations (identity = the rest pose: standing, arms hanging) plus a translation of the hips.
struct Pose {
    std::array<glm::quat, BONE_COUNT> rot;
    glm::vec3 hips{0.f};
    Pose() { rot.fill(glm::quat(1.f, 0.f, 0.f, 0.f)); }
};

using BoneMats = std::array<glm::mat4, BONE_COUNT>;

struct Skeleton {
    int parent[BONE_COUNT];
    glm::vec3 restLocal[BONE_COUNT];  // offset of each joint from its parent's joint
    glm::vec3 restWorld[BONE_COUNT];  // joint position in the rest pose
    static const Skeleton& humanoid();
};

// Natural lengths of the limb segments (rest pose), used by IK and tests.
inline constexpr float THIGH_LEN = 0.45f, SHIN_LEN = 0.42f, UARM_LEN = 0.28f, LARM_LEN = 0.27f, HAND_LEN = 0.10f;
inline constexpr float STAND_HEIGHT = 1.92f;
// Feet to fingertips with the arms straight overhead.
inline constexpr float STRETCH_LEN = 1.5f + UARM_LEN + LARM_LEN + HAND_LEN;

glm::quat euler(float x, float y, float z);  // XYZ intrinsic (three.js Euler order)
glm::vec3 position(const glm::mat4& m);
glm::quat rotationOf(const glm::mat4& m);

// Joint transforms in model space.
void forwardKinematics(const Pose& pose, BoneMats& world);
// Skinning matrices (world pose * inverse bind) for the GPU.
void skinMatrices(const Pose& pose, BoneMats& skin);
// Per-bone slerp, hips lerp.
Pose blend(const Pose& a, const Pose& b, float w);

}  // namespace anim
