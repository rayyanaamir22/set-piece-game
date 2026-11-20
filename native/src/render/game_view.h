// Scene renderer (port of render/GameView.ts): PBR lighting with an HDR sky, image-based lighting, 3-cascade shadow maps,
// 4x MSAA HDR rendering and ACES tone mapping. Reads the sim and never writes to it.
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

#include "gfx/gfx.h"
#include "render/camera_rig.h"
#include "render/painter.h"
#include "render/players_view.h"
#include "render/ibl.h"
#include "render/look.h"
#include "render/shadows.h"
#include "render/shapes.h"
#include "sim/Sim.h"

namespace render {

struct ScreenPoint {
    glm::vec2 px;
    bool visible;
};

class GameView {
public:
    GameView(double ballRadius, const std::string& shaderDir);

    CameraRig rig;

    void setTimeOfDay(TimeOfDay t);
    void setQuality(Quality q);
    // Developer helper: "taker" / "keeper" frames that character from the side; "" = normal camera.
    void setDebugCamera(const std::string& mode) { debugCam_ = mode; }
    // Impact events from the sim that need a visual reaction.
    void onSimEvent(const sim::SimEvent& e);
    // `alpha` interpolates the ball between prevBallPos and pos. Draws into the window's framebuffer.
    void render(const sim::SimState& s, double alpha, double dt, int fbW, int fbH);

    ScreenPoint project(const glm::vec3& world) const;
    // World x/z on the grass under a normalised screen position (0..1, y down).
    std::optional<glm::vec2> pickGround(double nx, double ny) const;
    glm::vec3 ballPosition() const { return ballPos_; }
    const anim::PlayerAnimators& animators() const { return players_.animators(); }
    double gpuMs() const { return gpuTimer_.lastMs(); }
    bool shadowsOn() const { return shadows_.size() > 0; }
    int msaaSamples() const { return samples_; }

private:
    void buildPitch();
    void buildGoal();
    void updateNet(double dt);
    void drawWorld(const sim::SimState& s, const LitPainter& p, bool shadowPass);
    void drawAids(const sim::SimState& s);
    void drawRings(const sim::SimState& s, double dt);
    void drawSpray(const sim::SimState& s);
    void unlit(const gfx::Mesh& m, const glm::mat4& model, const glm::vec3& srgb, float alpha, bool depthTest, bool doubleSided = false);
    void ensureTargets(int w, int h);
    void bindScene(const glm::vec3& cam);
    void bindLighting(const gfx::Shader& sh, const glm::vec3& cam);

    double radius_;
    std::string shaderDir_;
    Look look_;
    TimeOfDay tod_ = TimeOfDay::Day;
    Quality quality_ = Quality::High;
    bool lookDirty_ = true;
    std::string debugCam_;
    int samples_ = 4;

    gfx::Shader pbr_, depth_, sky_, tonemap_, unlitShader_, pbrSkinned_, skinnedDepth_;
    std::unique_ptr<Ibl> ibl_;
    gfx::SceneTarget target_;
    gfx::ShadowArray shadows_;
    gfx::FullscreenTriangle fullscreen_;
    gfx::GpuTimer gpuTimer_;
    CascadeSet cascades_{};
    LitPainter lit_;
    PlayersView players_;
    double reloadClock_ = 0;

    gfx::Mesh stripeDark_, stripeLight_, outerGround_, pitchLines_, post_, bar_, stanchion_, ballMesh_, ballShadow_;
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
