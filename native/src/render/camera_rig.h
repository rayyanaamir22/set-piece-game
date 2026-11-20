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
    glm::vec3 position() const { return override_ ? ovEye_ : pos_; }
    glm::vec3 target() const { return override_ ? ovTarget_ : lookAt_; }
    // Debug / replay: take the camera over completely (clearOverride gives it back).
    void setOverride(const glm::vec3& eye, const glm::vec3& target) {
        override_ = true;
        ovEye_ = eye;
        ovTarget_ = target;
    }
    void clearOverride() { override_ = false; }
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    static constexpr float FOV_DEG = 42.f, NEAR = 0.1f, FAR = 400.f;

private:
    glm::vec3 pos_{0}, lookAt_{0}, ovEye_{0}, ovTarget_{0};
    bool override_ = false;
    int lastSpotId_ = -1;
};

}  // namespace render
