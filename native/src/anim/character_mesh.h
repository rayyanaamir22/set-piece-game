// A procedurally generated low-poly humanoid skinned to the skeleton (pure CPU data, testable).
#pragma once
#include <vector>

#include "anim/skeleton.h"

namespace anim {

// Material slots: the renderer maps them to team colours.
enum Material : int { MAT_SHIRT, MAT_SHORTS, MAT_SKIN, MAT_BOOTS, MAT_GLOVES, MAT_SOCKS, MATERIAL_COUNT };

struct SkinVertex {
    glm::vec3 pos, normal;  // rest pose, model space
    float material;
    int joints[4];
    float weights[4];
};
struct SkinMesh {
    std::vector<SkinVertex> vertices;
    std::vector<unsigned> indices;
};

const SkinMesh& characterMesh();

}  // namespace anim
