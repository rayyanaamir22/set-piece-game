// Port of render/CameraRig.ts: third-person set-piece camera behind the ball; chases the ball after the strike.
#pragma once
#include <glm/glm.hpp>

#include "sim/Sim.h"

namespace render {

class CameraRig {
public:
    bool far = false;
    double distanceMul = 1;  // client preference

    void toggle() { far = !far; }
    void update(const sim::SimState& s, const glm::vec3& ballPos, double dt);
    glm::vec3 position() const { return pos_; }
    glm::vec3 target() const { return lookAt_; }
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    static constexpr float FOV_DEG = 42.f, NEAR = 0.1f, FAR = 400.f;

private:
    glm::vec3 pos_{0}, lookAt_{0};
    int lastSpotId_ = -1;
};

}  // namespace render
