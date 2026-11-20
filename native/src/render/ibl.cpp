#include "render/ibl.h"

namespace render {

namespace {
struct Face {
    glm::vec3 fwd, right, vup;
};
// OpenGL cube map conventions: dir = fwd + u * right + v * vup, with u, v in [-1, 1] and v = 2t - 1.
const Face FACES[6] = {
    {{1, 0, 0}, {0, 0, -1}, {0, -1, 0}},  {{-1, 0, 0}, {0, 0, 1}, {0, -1, 0}}, {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
    {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, 0, 1}, {1, 0, 0}, {0, -1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, -1, 0}},
};
void setFace(const gfx::Shader& s, int f) {
    s.set("uFwd", FACES[f].fwd);
    s.set("uRight", FACES[f].right);
    s.set("uVUp", FACES[f].vup);
}
constexpr int ENV_SIZE = 128, IRR_SIZE = 32, PREF_SIZE = 128, PREF_MIPS = 5, BRDF_SIZE = 128;
}  // namespace

void setSkyUniforms(const gfx::Shader& s, const Look& l) {
    s.set("uSkyZenith", l.zenith);
    s.set("uSkyHorizon", l.horizon);
    s.set("uSkyGround", l.ground);
    s.set("uSunDir", l.sunDir);
    s.set("uSunRadiance", l.sunColor);
    s.set("uSunCos", l.sunCos);
    s.set("uSunGlow", l.sunGlow);
}

Ibl::Ibl(const std::string& dir)
    : env_(dir, "fullscreen.vert", "bake_env.frag"),
      irrShader_(dir, "fullscreen.vert", "bake_irradiance.frag"),
      prefShader_(dir, "fullscreen.vert", "bake_prefilter.frag"),
      brdfShader_(dir, "fullscreen.vert", "bake_brdf.frag") {
    envMap_.create(ENV_SIZE, 8);
    irr_.create(IRR_SIZE, 1);
    pref_.create(PREF_SIZE, PREF_MIPS);
    brdf_.create(BRDF_SIZE, BRDF_SIZE);
    bakeBrdf();
}

void Ibl::reloadShaders() {
    env_.reloadIfChanged();
    irrShader_.reloadIfChanged();
    prefShader_.reloadIfChanged();
    brdfShader_.reloadIfChanged();
}

void Ibl::bakeBrdf() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    brdf_.bind();
    brdfShader_.use();
    tri_.draw();
}

void Ibl::bake(const Look& look) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    // 1. The sky (without the sun disc) into the environment cube, then its mip chain.
    env_.use();
    setSkyUniforms(env_, look);
    for (int f = 0; f < 6; f++) {
        envMap_.bindFace(f, 0);
        setFace(env_, f);
        tri_.draw();
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, envMap_.tex());
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // 2. Diffuse irradiance (stored as irradiance / PI).
    irrShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envMap_.tex());
    irrShader_.set("uEnv", 0);
    for (int f = 0; f < 6; f++) {
        irr_.bindFace(f, 0);
        setFace(irrShader_, f);
        tri_.draw();
    }

    // 3. GGX pre-filtered specular, one roughness per mip.
    prefShader_.use();
    glBindTexture(GL_TEXTURE_CUBE_MAP, envMap_.tex());
    prefShader_.set("uEnv", 0);
    prefShader_.set("uEnvRes", static_cast<float>(ENV_SIZE));
    for (int m = 0; m < PREF_MIPS; m++) {
        prefShader_.set("uRough", static_cast<float>(m) / (PREF_MIPS - 1));
        for (int f = 0; f < 6; f++) {
            pref_.bindFace(f, m);
            setFace(prefShader_, f);
            tri_.draw();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace render
