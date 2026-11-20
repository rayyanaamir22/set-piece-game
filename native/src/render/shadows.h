// Cascaded shadow map fitting: three bounding spheres around slices of the camera frustum, texel-snapped
// so the shadow edges do not swim as the camera moves.
#pragma once
#include <glm/glm.hpp>

namespace render {

struct CascadeSet {
    glm::mat4 viewProj[3];  // world -> light clip space, per cascade
    glm::vec3 splits;       // far view-depth of each cascade (metres)
    glm::vec3 texel;        // world size of one shadow texel, per cascade
};

// `splitFar` = the far plane of each cascade; the first one starts at the camera near plane.
CascadeSet fitCascades(const glm::mat4& view, float fovYRad, float aspect, float camNear, const glm::vec3& towardsSun, int shadowSize,
                       const glm::vec3& splitFar, float casterReach);

}  // namespace render
