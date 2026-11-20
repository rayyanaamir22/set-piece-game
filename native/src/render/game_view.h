// Graybox scene (port of render/GameView.ts): pitch, goal and net, players, ball, aim aids, spray, target rings.
// Reads the sim and never writes to it.
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>

#include "gfx/gfx.h"
#include "render/camera_rig.h"
#include "render/figures.h"
#include "render/look.h"
#include "render/shapes.h"
#include "sim/Sim.h"

namespace render {

struct ScreenPoint {
    glm::vec2 px;
    bool visible;
};

class GameView {
public:
    explicit GameView(double ballRadius);

    CameraRig rig;

    void setTimeOfDay(TimeOfDay t);
    // Impact events from the sim that need a visual reaction.
    void onSimEvent(const sim::SimEvent& e);
    // `alpha` interpolates the ball between prevBallPos and pos. Draws into the current framebuffer.
    void render(const sim::SimState& s, double alpha, double dt, int fbW, int fbH);

    ScreenPoint project(const glm::vec3& world) const;
    // World x/z on the grass under a normalised screen position (0..1, y down).
    std::optional<glm::vec2> pickGround(double nx, double ny) const;
    glm::vec3 ballPosition() const { return ballPos_; }

private:
    void buildPitch();
    void buildGoal();
    void updateNet(double dt);
    void drawAids(const sim::SimState& s);
    void drawRings(const sim::SimState& s, double dt);
    void drawSpray(const sim::SimState& s);
    void unlit(const gfx::Mesh& m, const glm::mat4& model, const glm::vec3& srgb, float alpha, bool depthTest, bool doubleSided = false);

    double radius_;
    Look look_;
    gfx::Shader litShader_, unlitShader_;
    LitPainter lit_;
    PlayerViews players_;

    gfx::Mesh stripeDark_, stripeLight_, pitchLines_, post_, bar_, stanchion_, ballMesh_, ballShadow_;
    gfx::Mesh reticleRing_, reticleDot_, reticleTick_, arrow_, runupRing_, sprayRing_, sprayLine_;
    gfx::DynLines net_, runupLine_, pathLine_;
    std::vector<glm::vec3> netRest_, netLive_;

    glm::quat ballRot_{1, 0, 0, 0};
    glm::vec3 ballPos_{0};
    glm::mat4 view_{1}, proj_{1}, viewProj_{1};
    int fbW_ = 1, fbH_ = 1;

    // Net ripple state.
    bool rippling_ = false;
    glm::vec3 rippleAt_{0};
    double rippleAmp_ = 0, rippleAge_ = 0;
    bool goalRipple_ = false;

    // Challenge ring cache.
    int ringsKey_ = -1;
    struct Ring { gfx::Mesh ring, fill; float x, y, r; double points; };
    std::vector<Ring> rings_;
    double ringT_ = 0;

public:
    // Challenge ring labels for the HUD overlay: world position + text, filled during render().
    struct RingLabel { glm::vec3 pos; double points; float size; bool hit; };
    std::vector<RingLabel> ringLabels;
};

}  // namespace render
