#include "render/painter.h"

namespace render {

void LitPainter::draw(const gfx::Mesh& m, const glm::mat4& model, const glm::vec3& srgb, float alpha, int pattern, float rough, float metal, int grass) const {
    shader->set("uModel", model);
    if (!depthOnly) {
        shader->set("uNormalMat", glm::mat3(glm::transpose(glm::inverse(model))));
        shader->set("uColor", glm::vec4(srgb, alpha));
        shader->set("uPattern", pattern);
        shader->set("uRough", rough);
        shader->set("uMetal", metal);
        shader->set("uGrass", grass);
    }
    m.draw();
}

}  // namespace render
