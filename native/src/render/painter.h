// Draws lit meshes with a model matrix and an sRGB colour through whichever shader is active.
#pragma once
#include <glm/glm.hpp>

#include "gfx/gfx.h"

namespace render {

class LitPainter {
public:
    const gfx::Shader* shader = nullptr;   // static meshes
    const gfx::Shader* skinned = nullptr;  // skinned meshes (the matching pass's skinned program)
    bool depthOnly = false;                // shadow pass: only transforms are needed
    // `pattern`: 1 = ball panels. `grass`: 1 = procedural turf variation.
    void draw(const gfx::Mesh& m, const glm::mat4& model, const glm::vec3& srgb, float alpha = 1.f, int pattern = 0, float rough = 0.7f,
              float metal = 0.f, int grass = 0) const;
};

}  // namespace render
