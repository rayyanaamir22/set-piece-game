#include "render/game_view.h"

#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "sim/constants.h"
#include "sim/layout.h"

namespace render {

using glm::mat4;
using glm::vec3;
using sim::Phase;

namespace {
constexpr float PI = 3.14159265358979f;
// Cascade far planes (metres) and how far towards the sun shadow casters are still included.
const glm::vec3 SPLITS(12.f, 35.f, 100.f);
constexpr float CASTER_REACH = 90.f;

struct MeshBuf {
    std::vector<gfx::Vertex> v;
    std::vector<unsigned> i;
    // A horizontal quad (normal +y) from four XZ corners at height y.
    void quad(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d, float y) {
        const unsigned s = static_cast<unsigned>(v.size());
        for (auto p : {a, b, c, d}) v.push_back({{p.x, y, p.y}, {0, 1, 0}});
        i.insert(i.end(), {s, s + 2, s + 1, s, s + 3, s + 2});
    }
    // A line of width w from (x1,z1) to (x2,z2), extended by w/2 at both ends.
    void line(float x1, float z1, float x2, float z2, float w, float y) {
        glm::vec2 d(x2 - x1, z2 - z1);
        const float L = glm::length(d);
        d /= L;
        const glm::vec2 n(-d.y, d.x), e = d * (w / 2), h = n * (w / 2);
        const glm::vec2 p1(x1, z1), p2(x2, z2);
        quad(p1 - e - h, p2 + e - h, p2 + e + h, p1 - e + h, y);
    }
    // Arc strip around (cx,cz): points at angle a are (r cos a, -r sin a) like a ring rotated -90 deg about x.
    void arc(float cx, float cz, float r, float a0, float a1, float w, float y, int seg = 48) {
        for (int k = 0; k < seg; k++) {
            const float t0 = a0 + (a1 - a0) * k / seg, t1 = a0 + (a1 - a0) * (k + 1) / seg;
            auto pt = [&](float t, float rr) { return glm::vec2(cx + rr * std::cos(t), cz - rr * std::sin(t)); };
            quad(pt(t0, r - w / 2), pt(t0, r + w / 2), pt(t1, r + w / 2), pt(t1, r - w / 2), y);
        }
    }
    gfx::Mesh build() const { return gfx::Mesh(v, i); }
};
}  // namespace

GameView::GameView(double ballRadius, const std::string& dir)
    : radius_(ballRadius),
      shaderDir_(dir),
      look_(lookFor(TimeOfDay::Day)),
      pbr_(dir, "scene.vert", "pbr.frag"),
      depth_(dir, "depth.vert", "depth.frag"),
      sky_(dir, "fullscreen.vert", "sky.frag"),
      tonemap_(dir, "fullscreen.vert", "tonemap.frag"),
      unlitShader_(dir, "scene.vert", "unlit.frag"),
      pbrSkinned_(dir, "skinned.vert", "pbr_skinned.frag"),
      skinnedDepth_(dir, "skinned_depth.vert", "depth.frag"),
      ibl_(std::make_unique<Ibl>(dir)),
      post_(makeCylinder(static_cast<float>(sim::POST_RADIUS), static_cast<float>(sim::GOAL_HEIGHT), 16)),
      bar_(makeCylinder(static_cast<float>(sim::POST_RADIUS), static_cast<float>(sim::GOAL_HALF_WIDTH * 2 + sim::POST_RADIUS * 2), 16)),
      stanchion_(makeCylinder(0.03f, static_cast<float>(sim::GOAL_DEPTH), 8)),
      ballMesh_(makeSphere(static_cast<float>(ballRadius), 32, 20)),
      ballShadow_(makeDisc(static_cast<float>(ballRadius) * 1.5f, 16)),
      reticleRing_(makeRing(0.26f, 0.34f, 40)),
      reticleDot_(makeDisc(0.06f, 16)),
      reticleTick_(makePlane(0.1f, 0.2f)),
      runupRing_(makeRing(0.25f, 0.34f, 28)),
      sprayRing_(makeRing(0.18f, 0.34f, 28)),
      sprayLine_(makePlane(3.4f, 0.16f)) {
    buildPitch();
    buildGoal();

    // Aim arrow: tip at -z, lies on the grass (XZ plane).
    {
        const float L = 5;
        const glm::vec2 pts[7] = {{-0.12f, 0}, {0.12f, 0}, {0.12f, L - 0.9f}, {0.4f, L - 0.9f}, {0, L}, {-0.4f, L - 0.9f}, {-0.12f, L - 0.9f}};
        std::vector<gfx::Vertex> v;
        for (auto p : pts) v.push_back({{p.x, 0, -p.y}, {0, 1, 0}});
        arrow_ = gfx::Mesh(v, {0, 1, 2, 0, 2, 6, 6, 2, 3, 6, 3, 5, 5, 3, 4});
    }
    {
        MeshBuf b;
        b.quad({-400, -400}, {400, -400}, {400, 400}, {-400, 400}, -0.02f);
        outerGround_ = b.build();
    }
    samples_ = gfx::clampSamples(4);
}

void GameView::setTimeOfDay(TimeOfDay t) {
    tod_ = t;
    look_ = lookFor(t);
    lookDirty_ = true;
}

void GameView::setQuality(Quality q) { quality_ = q; }

void GameView::ensureTargets(int w, int h) {
    const int samples = quality_ == Quality::Low ? gfx::clampSamples(2) : gfx::clampSamples(4);
    if (!target_.matches(w, h, samples)) {
        target_.create(w, h, samples);
        samples_ = samples;
    }
    const int shadowSize = quality_ == Quality::Low ? 0 : quality_ == Quality::Medium ? 1024 : 2048;
    if (shadows_.size() != shadowSize) {
        if (shadowSize == 0) shadows_.destroy();
        else shadows_.create(shadowSize);
    }
}

void GameView::buildPitch() {
    // Grass mown in 5.5 m bands parallel to the goal line: two meshes (dark / light bands).
    const float z0 = -8, half = 48;
    MeshBuf dark, light;
    for (int b = 0; b < 12; b++) {
        MeshBuf& m = (b % 2 == 0) ? dark : light;
        const float za = z0 + b * 5.5f, zb = za + 5.5f;
        m.quad({-half, za}, {half, za}, {half, zb}, {-half, zb}, 0.f);
    }
    stripeDark_ = dark.build();
    stripeLight_ = light.build();

    MeshBuf lines;
    const float W = 0.12f, Y = 0.012f, H = static_cast<float>(sim::PITCH_HALF_WIDTH);
    const float box = static_cast<float>(sim::PENALTY_BOX_HALF), boxZ = static_cast<float>(sim::PENALTY_BOX_Z);
    const float six = static_cast<float>(sim::SIX_YARD_HALF), sixZ = static_cast<float>(sim::SIX_YARD_Z);
    lines.line(-H, 0, H, 0, W, Y);
    lines.line(-H, 0, -H, 58, W, Y);
    lines.line(H, 0, H, 58, W, Y);
    lines.line(-box, 0, -box, boxZ, W, Y);
    lines.line(box, 0, box, boxZ, W, Y);
    lines.line(-box, boxZ, box, boxZ, W, Y);
    lines.line(-six, 0, -six, sixZ, W, Y);
    lines.line(six, 0, six, sixZ, W, Y);
    lines.line(-six, sixZ, six, sixZ, W, Y);
    const float spotZ = static_cast<float>(sim::PENALTY_SPOT_Z);
    lines.arc(0, spotZ, 0.075f, 0, 2 * PI, 0.15f, Y, 12);
    const float R = static_cast<float>(sim::CENTRE_ARC_RADIUS);
    const float phi = std::acos((boxZ - spotZ) / R);
    lines.arc(0, spotZ, R, -PI / 2 - phi, -PI / 2 + phi, W, Y, 64);
    lines.arc(-H, 0, 1, -PI / 2, 0, W, Y, 16);
    lines.arc(H, 0, 1, -PI, -PI / 2, W, Y, 16);
    pitchLines_ = lines.build();
}

void GameView::buildGoal() {
    // Net: grid lines on back, top and the two sides (port of render/Goal.ts).
    const float STEP = 0.18f;
    const float W = static_cast<float>(sim::GOAL_HALF_WIDTH), H = static_cast<float>(sim::GOAL_HEIGHT), D = static_cast<float>(sim::GOAL_DEPTH);
    auto seg = [&](vec3 a, vec3 b) { netRest_.push_back(a); netRest_.push_back(b); };
    const int nx = static_cast<int>(std::round(W * 2 / STEP)), ny = static_cast<int>(std::round(H / STEP)), nz = static_cast<int>(std::round(D / STEP));
    for (int i = 0; i <= nx; i++) seg({-W + i * 2 * W / nx, 0, -D}, {-W + i * 2 * W / nx, H, -D});
    for (int j = 0; j <= ny; j++) seg({-W, j * H / ny, -D}, {W, j * H / ny, -D});
    for (int i = 0; i <= nx; i++) seg({-W + i * 2 * W / nx, H, 0}, {-W + i * 2 * W / nx, H, -D});
    for (int k = 0; k <= nz; k++) seg({-W, H, -k * D / nz}, {W, H, -k * D / nz});
    for (float sx : {-1.f, 1.f}) {
        for (int j = 0; j <= ny; j++) seg({sx * W, j * H / ny, 0}, {sx * W, j * H / ny, -D});
        for (int k = 0; k <= nz; k++) seg({sx * W, 0, -k * D / nz}, {sx * W, H, -k * D / nz});
    }
    netLive_ = netRest_;
    net_.upload(netLive_);
}

void GameView::onSimEvent(const sim::SimEvent& e) {
    auto ripple = [&](vec3 pos, double strength) {
        rippling_ = true;
        rippleAt_ = pos;
        rippleAmp_ = std::min(0.9, strength * 0.04);
        rippleAge_ = 0;
    };
    if (e.type == sim::EventType::Net) ripple(vec3(e.pos.x, e.pos.y, e.pos.z), e.speed);
    else if (e.type == sim::EventType::Goal) goalRipple_ = true;  // billow the net as the ball crosses the line
}

void GameView::updateNet(double dt) {
    if (!rippling_ || rippleAmp_ < 0.002) return;
    rippleAge_ += dt;
    const double decay = std::exp(-2.6 * rippleAge_);
    for (size_t i = 0; i < netRest_.size(); i++) {
        const vec3 d = netRest_[i] - rippleAt_;
        const double dist = glm::length(d);
        const double w = std::exp(-dist * dist * 0.6) * std::cos(rippleAge_ * 22 - dist * 4) * decay * rippleAmp_;
        netLive_[i] = netRest_[i] + vec3(0, 0, -1) * static_cast<float>(w);
    }
    net_.upload(netLive_);
    if (decay < 0.01) {
        netLive_ = netRest_;
        net_.upload(netLive_);
        rippling_ = false;
    }
}

void GameView::unlit(const gfx::Mesh& m, const mat4& model, const vec3& c, float alpha, bool depthTest, bool) {
    unlitShader_.set("uModel", model);
    unlitShader_.set("uNormalMat", glm::mat3(1.f));
    unlitShader_.set("uColor", glm::vec4(c, alpha));
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    m.draw();
}

// Everything that lies on or stands on the pitch. Receivers (grass, lines) are skipped in the shadow pass.
void GameView::drawWorld(const sim::SimState&, const LitPainter& p, bool shadowPass) {
    if (!shadowPass) {
        p.draw(stripeLight_, mat4(1), hex(0x378a3a), 1.f, 0, 0.92f, 0.f, 1);
        p.draw(stripeDark_, mat4(1), hex(0x2f7d32), 1.f, 0, 0.92f, 0.f, 1);
        p.draw(outerGround_, mat4(1), hex(0x2a6a2e), 1.f, 0, 0.95f, 0.f, 1);
        p.draw(pitchLines_, mat4(1), hex(0xf4f4f0), 1.f, 0, 0.75f);
    }
    // Goal frame.
    const vec3 white(0.93f);
    const float GW = static_cast<float>(sim::GOAL_HALF_WIDTH), GH = static_cast<float>(sim::GOAL_HEIGHT), GD = static_cast<float>(sim::GOAL_DEPTH);
    for (float sx : {-1.f, 1.f}) {
        p.draw(post_, glm::translate(mat4(1), vec3(sx * GW, GH / 2, 0)), white, 1.f, 0, 0.38f);
        p.draw(stanchion_, glm::translate(mat4(1), vec3(sx * GW, GH, -GD / 2)) * glm::rotate(mat4(1), PI / 2, vec3(1, 0, 0)), vec3(0.8f), 1.f, 0, 0.45f, 0.6f);
    }
    p.draw(bar_, glm::translate(mat4(1), vec3(0, GH, 0)) * glm::rotate(mat4(1), PI / 2, vec3(0, 0, 1)), white, 1.f, 0, 0.38f);

    players_.draw(p);
    p.draw(ballMesh_, glm::translate(mat4(1), ballPos_) * glm::mat4_cast(ballRot_), vec3(0.96f), 1.f, 1, 0.42f);
}

void GameView::render(const sim::SimState& s, double alpha, double dt, int fbW, int fbH) {
    fbW_ = fbW;
    fbH_ = fbH;

    // Shader hot reload (a few times a second is plenty).
    reloadClock_ += dt;
    if (reloadClock_ > 0.5) {
        reloadClock_ = 0;
        pbr_.reloadIfChanged();
        depth_.reloadIfChanged();
        sky_.reloadIfChanged();
        tonemap_.reloadIfChanged();
        unlitShader_.reloadIfChanged();
        pbrSkinned_.reloadIfChanged();
        skinnedDepth_.reloadIfChanged();
        ibl_->reloadShaders();
    }

    ensureTargets(fbW, fbH);
    if (lookDirty_) {
        ibl_->bake(look_);
        lookDirty_ = false;
    }

    // Interpolated ball.
    const sim::V3 &p = s.prevBallPos, &q = s.ball.pos;
    ballPos_ = vec3(p.x + (q.x - p.x) * alpha, p.y + (q.y - p.y) * alpha, p.z + (q.z - p.z) * alpha);
    // Visual spin: integrate the angular velocity.
    const sim::V3& w = s.ball.spin;
    const double wl = sim::len(w);
    if (wl > 1e-3) ballRot_ = glm::normalize(glm::angleAxis(static_cast<float>(wl * dt), vec3(w.x / wl, w.y / wl, w.z / wl)) * ballRot_);

    if (goalRipple_) {
        goalRipple_ = false;
        rippling_ = true;
        rippleAt_ = vec3(ballPos_.x, ballPos_.y, -0.3f);
        rippleAmp_ = std::min(0.9, 14 * 0.04);
        rippleAge_ = 0;
    }
    updateNet(dt);
    rig.update(s, ballPos_, dt);
    players_.update(s, dt);
    if (debugCam_ == "taker") {
        const glm::mat4& m = players_.animators().taker.character.model;
        const vec3 c = vec3(m[3]);
        rig.setOverride(c + vec3(m[0]) * 4.2f + vec3(0, 0.7f, 0), c + vec3(0, 0.9f, 0));
    } else if (debugCam_ == "keeper" && players_.animators().keeper.character) {
        const vec3 c = vec3(players_.animators().keeper.character->model[3]);
        rig.setOverride(vec3(c.x, 1.3f, c.z + 5.5f), vec3(c.x, 1.0f, c.z));
    } else {
        rig.clearOverride();
    }

    view_ = rig.view();
    const float aspect = static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));
    proj_ = rig.projection(aspect);
    viewProj_ = proj_ * view_;
    const vec3 cam = rig.position();

    gpuTimer_.begin();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // ---- shadow cascades
    const bool shadowsOn = shadows_.size() > 0;
    if (shadowsOn) {
        cascades_ = fitCascades(view_, glm::radians(CameraRig::FOV_DEG), aspect, CameraRig::NEAR, look_.sunDir, shadows_.size(), SPLITS, CASTER_REACH);
        depth_.use();
        lit_.shader = &depth_;
        lit_.skinned = &skinnedDepth_;
        lit_.depthOnly = true;
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.f);
        for (int i = 0; i < 3; i++) {
            shadows_.bindLayer(i);
            depth_.use();
            depth_.set("uViewProj", cascades_.viewProj[i]);
            skinnedDepth_.use();
            skinnedDepth_.set("uViewProj", cascades_.viewProj[i]);
            depth_.use();
            drawWorld(s, lit_, true);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // ---- HDR scene: sky, then PBR geometry
    target_.bindHdr();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    sky_.use();
    sky_.set("uInvViewProj", glm::inverse(viewProj_));
    sky_.set("uCam", cam);
    setSkyUniforms(sky_, look_);
    fullscreen_.draw();
    glEnable(GL_DEPTH_TEST);

    bindScene(cam);
    lit_.shader = &pbr_;
    lit_.skinned = &pbrSkinned_;
    lit_.depthOnly = false;
    drawWorld(s, lit_, false);

    // ---- resolve, tone map into the LDR target (which shares the scene depth)
    target_.resolveHdr();
    target_.bindLdr();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    tonemap_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, target_.resolveTex());
    tonemap_.set("uHdr", 0);
    tonemap_.set("uExposure", look_.exposure);
    fullscreen_.draw();

    // ---- world-space aids drawn after tone mapping, so their colours stay exact
    unlitShader_.use();
    unlitShader_.set("uViewProj", viewProj_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    const float h = std::max(0.f, ballPos_.y - static_cast<float>(radius_));
    unlit(ballShadow_, glm::translate(mat4(1), vec3(ballPos_.x, 0.02f, ballPos_.z)) * glm::rotate(mat4(1), -PI / 2, vec3(1, 0, 0)) * glm::scale(mat4(1), vec3(1 + h * 0.15f)),
          vec3(0), std::max(0.08f, 0.2f - h * 0.03f), true);

    unlitShader_.set("uModel", mat4(1));
    unlitShader_.set("uColor", glm::vec4(1, 1, 1, 0.55f));
    net_.draw(GL_LINES);

    drawSpray(s);
    drawAids(s);
    drawRings(s, dt);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    target_.presentTo(0);
    gpuTimer_.end();
}

void GameView::bindLighting(const gfx::Shader& sh, const vec3& cam) {
    sh.use();
    sh.set("uViewProj", viewProj_);
    sh.set("uCam", cam);
    sh.set("uCamFwd", glm::normalize(rig.target() - cam));
    sh.set("uSunDir", look_.sunDir);
    sh.set("uSunE", look_.sunColor * look_.sunE);
    sh.set("uIblScale", look_.iblScale);
    sh.set("uFogColor", look_.fog);
    sh.set("uFog", glm::vec2(look_.fogNear, look_.fogFar));
    sh.set("uPrefMaxLod", static_cast<float>(ibl_->prefilterMips() - 1));
    const bool shadowsOn = shadows_.size() > 0;
    sh.set("uShadowOn", shadowsOn ? 1 : 0);
    sh.set("uPcf", quality_ == Quality::High ? 1 : 0);
    if (shadowsOn) {
        sh.setArray("uShadowMat", cascades_.viewProj, 3);
        sh.set("uSplits", cascades_.splits);
        sh.set("uTexel", cascades_.texel);
    }
    sh.set("uShadow", 0);
    sh.set("uIrr", 1);
    sh.set("uPref", 2);
    sh.set("uBrdf", 3);
}

void GameView::bindScene(const vec3& cam) {
    bindLighting(pbrSkinned_, cam);
    bindLighting(pbr_, cam);  // leaves the static program active
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadows_.tex());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ibl_->irradiance().tex());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ibl_->prefiltered().tex());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ibl_->brdf().tex());
    glActiveTexture(GL_TEXTURE0);
}

ScreenPoint GameView::project(const vec3& w) const {
    const glm::vec4 c = viewProj_ * glm::vec4(w, 1);
    if (c.w <= 0.01f) return {{0, 0}, false};
    const glm::vec3 n = glm::vec3(c) / c.w;
    return {{(n.x * 0.5f + 0.5f) * fbW_, (1 - (n.y * 0.5f + 0.5f)) * fbH_}, std::fabs(n.x) < 1.2f && std::fabs(n.y) < 1.2f};
}

std::optional<glm::vec2> GameView::pickGround(double nx, double ny) const {
    const glm::vec4 a = glm::inverse(viewProj_) * glm::vec4(static_cast<float>(nx * 2 - 1), static_cast<float>(1 - ny * 2), -1, 1);
    const glm::vec4 b = glm::inverse(viewProj_) * glm::vec4(static_cast<float>(nx * 2 - 1), static_cast<float>(1 - ny * 2), 1, 1);
    const vec3 p0 = vec3(a) / a.w, p1 = vec3(b) / b.w;
    const vec3 dir = p1 - p0;
    if (std::fabs(dir.y) < 1e-6f) return std::nullopt;
    const float t = -p0.y / dir.y;
    if (t < 0) return std::nullopt;
    const vec3 hit = p0 + dir * t;
    return glm::vec2(hit.x, hit.z);
}

void GameView::drawSpray(const sim::SimState& s) {
    // Fully visible until the strike, then gone within ~3.5 s.
    const double fade = s.phase == Phase::Flight ? std::max(0.0, 1 - s.flightT / 3.5) : s.phase == Phase::Result ? 0 : 1;
    if (fade <= 0.01) return;
    const float a = static_cast<float>(0.9 * fade);
    const vec3 c = hex(0xf4f9ff);
    const mat4 flat = glm::rotate(mat4(1), -PI / 2, vec3(1, 0, 0));
    unlit(sprayRing_, glm::translate(mat4(1), vec3(s.spot.x, 0.02f, s.spot.z)) * flat, c, a, true);

    const double dist = sim::jhypot(s.spot.x, s.spot.z);
    if (dist <= sim::WALL_DISTANCE + 1.5) return;
    const sim::V3 d = sim::dirToGoal(s.spot);
    double cx = s.spot.x + d.x * sim::WALL_DISTANCE, cz = s.spot.z + d.z * sim::WALL_DISTANCE;
    double ax = sim::rightOf(d).x, az = sim::rightOf(d).z;
    if (s.wall) {
        cx = cz = 0;
        for (const auto& b : s.wall->base) { cx += b.x; cz += b.z; }
        cx /= s.wall->base.size();
        cz /= s.wall->base.size();
        ax = s.wall->axis.x;
        az = s.wall->axis.z;
        cx += s.wall->facing.x * 0.45;  // the line sits on the ball side of the players' feet
        cz += s.wall->facing.z * 0.45;
    }
    // three.js: rotation.set(-pi/2, 0, -atan2(az, ax), 'XYZ') = Rx(-pi/2) * Ry(0) * Rz(angle)
    unlit(sprayLine_, glm::translate(mat4(1), vec3(cx, 0.02f, cz)) * flat * glm::rotate(mat4(1), static_cast<float>(-std::atan2(az, ax)), vec3(0, 0, 1)), c, a, true);
}

void GameView::drawAids(const sim::SimState& s) {
    const bool aiming = s.phase == Phase::Aim || s.phase == Phase::Charge;
    if (!(aiming || s.phase == Phase::Runup)) return;
    const vec3 camPos = rig.position();
    const vec3 gold = (s.preview && s.preview->knuckle) ? hex(0xff6b6b) : hex(0xffd23f);

    // Reticle: constant apparent size, facing the camera.
    {
        const vec3 pos(s.aim.x, s.aim.y, 0.08f);
        const float dist = glm::distance(camPos, pos);
        const float sc = std::max(1.f, dist / 22);
        const vec3 zax = glm::normalize(camPos - pos);
        const vec3 xax = glm::normalize(glm::cross(vec3(0, 1, 0), zax));
        const vec3 yax = glm::cross(zax, xax);
        const mat4 base = glm::translate(mat4(1), pos) * mat4(glm::mat3(xax, yax, zax)) * glm::scale(mat4(1), vec3(sc));
        unlit(reticleRing_, base, gold, 0.95f, false);
        unlit(reticleDot_, base, gold, 0.95f, false);
        for (int i = 0; i < 4; i++) {
            const float a = i * PI / 2;
            unlit(reticleTick_, base * glm::translate(mat4(1), vec3(std::cos(a) * 0.46f, std::sin(a) * 0.46f, 0)) * glm::rotate(mat4(1), a + PI / 2, vec3(0, 0, 1)), gold, 0.95f, false);
        }
    }

    const mat4 flat = glm::rotate(mat4(1), -PI / 2, vec3(1, 0, 0));
    if (aiming) {
        const auto kp = sim::kickerPositions(s.spot, s.foot, s.runup.angle, s.runup.dist);
        // Dashed run-up line from the start to the strike point.
        std::vector<vec3> pts;
        const vec3 a(kp.runupStart.x, 0.04f, kp.runupStart.z), b(kp.strike.x, 0.04f, kp.strike.z);
        const float L = glm::distance(a, b);
        const vec3 dir = (b - a) / std::max(L, 1e-4f);
        for (float t = 0; t < L; t += 0.52f) {
            pts.push_back(a + dir * t);
            pts.push_back(a + dir * std::min(L, t + 0.3f));
        }
        runupLine_.upload(pts);
        unlitShader_.set("uModel", mat4(1));
        unlitShader_.set("uColor", glm::vec4(1, 1, 1, 0.8f));
        glEnable(GL_DEPTH_TEST);
        runupLine_.draw(GL_LINES);
        unlit(runupRing_, glm::translate(mat4(1), vec3(kp.runupStart.x, 0.045f, kp.runupStart.z)) * flat, vec3(1), 0.85f, true);

        // Launch arrow along the grass.
        if (s.preview) {
            const float sc = 0.6f + 0.4f * static_cast<float>(s.phase == Phase::Charge ? s.meter : 0.5);
            unlit(arrow_, glm::translate(mat4(1), vec3(s.spot.x, 0.035f, s.spot.z)) * glm::rotate(mat4(1), static_cast<float>(-s.preview->azimuth), vec3(0, 1, 0)) * glm::scale(mat4(1), vec3(1, 1, sc)),
                  hex(0xffd23f), 0.8f, true);
            if (s.showPath && s.preview->path) {
                std::vector<vec3> path;
                const auto& pp = *s.preview->path;
                for (size_t i = 0; i + 1 < pp.size(); i++) {
                    path.push_back(vec3(pp[i].x, pp[i].y, pp[i].z));
                    path.push_back(vec3(pp[i + 1].x, pp[i + 1].y, pp[i + 1].z));
                }
                pathLine_.upload(path);
                unlitShader_.set("uModel", mat4(1));
                unlitShader_.set("uColor", glm::vec4(hex(0x4ee1ff), 1.f));
                glEnable(GL_DEPTH_TEST);
                pathLine_.draw(GL_LINES);
            }
        }
    }
}

void GameView::drawRings(const sim::SimState& s, double dt) {
    ringLabels.clear();
    const auto& ch = s.challenge;
    if (!ch) {
        ringsKey_ = -1;
        return;
    }
    if (ringsKey_ != ch->index) {
        ringsKey_ = ch->index;
        rings_.clear();
        for (const auto& t : ch->def.targets)
            rings_.push_back({makeRing(static_cast<float>(t.r) * 0.8f, static_cast<float>(t.r), 48), makeDisc(static_cast<float>(t.r) * 0.8f, 40), static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.r), t.points});
    }
    ringT_ += dt;
    std::vector<double> values;
    for (const auto& t : ch->def.targets)
        if (std::find(values.begin(), values.end(), t.points) == values.end()) values.push_back(t.points);
    std::sort(values.begin(), values.end());
    const unsigned COLORS[4] = {0xffffff, 0xffd23f, 0xff8a3d, 0xff4d6d};
    const int hit = (s.phase == Phase::Result && s.result) ? s.result->ring : -1;
    for (size_t i = 0; i < rings_.size(); i++) {
        const size_t idx = std::min<size_t>(3, std::find(values.begin(), values.end(), rings_[i].points) - values.begin());
        const vec3 col = static_cast<int>(i) == hit ? hex(0x46d369) : hex(COLORS[idx]);
        const float pulse = static_cast<int>(i) == hit ? 1.f + 0.18f * static_cast<float>(std::sin(ringT_ * 14)) : 1.f;
        const mat4 m = glm::translate(mat4(1), vec3(rings_[i].x, rings_[i].y, 0.15f)) * glm::scale(mat4(1), vec3(pulse));
        unlit(rings_[i].fill, m, col, 0.12f, false);
        unlit(rings_[i].ring, m, col, 0.9f, false);
        ringLabels.push_back({vec3(rings_[i].x, rings_[i].y, 0.15f), rings_[i].points, std::min(0.9f, rings_[i].r * 1.1f), static_cast<int>(i) == hit});
    }
}

}  // namespace render
