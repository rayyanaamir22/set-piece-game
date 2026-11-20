#include "render/shadows.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace render {

CascadeSet fitCascades(const glm::mat4& view, float fovY, float aspect, float camNear, const glm::vec3& L, int size, const glm::vec3& splitFar,
                       float casterReach) {
    CascadeSet out{};
    out.splits = splitFar;
    const glm::vec3 up = std::fabs(L.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::mat4 rot = glm::lookAt(glm::vec3(0), -L, up);  // rotation only: world -> light orientation
    const glm::mat4 invRot = glm::inverse(rot);

    for (int c = 0; c < 3; c++) {
        const float n = c == 0 ? camNear : splitFar[c - 1], f = splitFar[c];
        const glm::mat4 inv = glm::inverse(glm::perspective(fovY, aspect, n, f) * view);
        glm::vec3 corners[8];
        glm::vec3 centre(0);
        int k = 0;
        for (int x = 0; x < 2; x++)
            for (int y = 0; y < 2; y++)
                for (int z = 0; z < 2; z++) {
                    const glm::vec4 p = inv * glm::vec4(x ? 1.f : -1.f, y ? 1.f : -1.f, z ? 1.f : -1.f, 1.f);
                    corners[k] = glm::vec3(p) / p.w;
                    centre += corners[k++];
                }
        centre /= 8.f;
        float radius = 0;
        for (const auto& p : corners) radius = std::max(radius, glm::length(p - centre));
        radius = std::ceil(radius * 16.f) / 16.f;  // stable size

        // Snap the centre to the shadow texel grid in light space.
        const float texel = 2.f * radius / static_cast<float>(size);
        glm::vec3 ls = glm::vec3(rot * glm::vec4(centre, 1));
        ls.x = std::floor(ls.x / texel + 0.5f) * texel;
        ls.y = std::floor(ls.y / texel + 0.5f) * texel;
        centre = glm::vec3(invRot * glm::vec4(ls, 1));

        const glm::vec3 eye = centre + L * (radius + casterReach);
        const glm::mat4 lv = glm::lookAt(eye, centre, up);
        const glm::mat4 lp = glm::ortho(-radius, radius, -radius, radius, 0.f, 2.f * radius + casterReach + radius);
        out.viewProj[c] = lp * lv;
        out.texel[c] = texel;
    }
    return out;
}

}  // namespace render
