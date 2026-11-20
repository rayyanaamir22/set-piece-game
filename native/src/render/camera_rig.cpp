#include "render/camera_rig.h"

#include <glm/gtc/matrix_transform.hpp>

#include "sim/layout.h"

namespace render {

using sim::Phase;

void CameraRig::update(const sim::SimState& s, const glm::vec3& ballPos, double dt) {
    const sim::V3 d = sim::dirToGoal(s.spot);
    const sim::V3 r = sim::rightOf(d);
    const double back = (far ? 11 : 7) * distanceMul;
    const double height = (far ? 4.2 : 2.5) * (0.6 + 0.4 * distanceMul);
    // Shift away from the taker's side so he stays in frame without blocking the ball.
    const double side = (s.foot == 'R' ? 1 : -1) * (far ? 1.6 : 1.0);

    const glm::vec3 home(s.spot.x - d.x * back + r.x * side, height, s.spot.z - d.z * back + r.z * side);

    const float aimMix = 0.35f;
    const glm::vec3 baseLook(s.spot.x + d.x * 12, 1.2, s.spot.z + d.z * 12);
    const glm::vec3 reticle(s.aim.x, s.aim.y, 0);
    const glm::vec3 aimLook = baseLook + (reticle - baseLook) * aimMix;

    glm::vec3 wantLook;
    if (s.phase == Phase::Flight || s.phase == Phase::Result) {
        wantLook = ballPos;
        wantLook.y = std::max(wantLook.y, 0.8f);  // don't stare at the ground
    } else {
        wantLook = aimLook;
    }

    if (lastSpotId_ != s.spotId) {
        pos_ = home;
        lookAt_ = wantLook;
        lastSpotId_ = s.spotId;
    }

    const float kPos = static_cast<float>(1 - std::exp(-5 * dt));
    const float kLook = static_cast<float>(1 - std::exp(-(s.phase == Phase::Flight ? 9 : 6) * dt));
    pos_ += (home - pos_) * kPos;
    lookAt_ += (wantLook - lookAt_) * kLook;
}

glm::mat4 CameraRig::view() const { return glm::lookAt(pos_, lookAt_, glm::vec3(0, 1, 0)); }
glm::mat4 CameraRig::projection(float aspect) const { return glm::perspective(glm::radians(FOV_DEG), aspect, NEAR, FAR); }

}  // namespace render
