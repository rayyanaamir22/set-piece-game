// Image-based lighting baked at load from the procedural sky: environment cube, diffuse irradiance cube,
// GGX pre-filtered specular cube (5 roughness mips) and the split-sum BRDF lookup.
#pragma once
#include <string>

#include "gfx/gfx.h"
#include "render/look.h"

namespace render {

// Sets the sky uniforms (colours, sun) used by sky.glsl on a shader that includes it.
void setSkyUniforms(const gfx::Shader& s, const Look& look);

class Ibl {
public:
    explicit Ibl(const std::string& shaderDir);
    void bake(const Look& look);  // re-bake for a new time of day
    void reloadShaders();         // hot reload
    const gfx::CubeMap& irradiance() const { return irr_; }
    const gfx::CubeMap& prefiltered() const { return pref_; }
    const gfx::Texture2D& brdf() const { return brdf_; }
    int prefilterMips() const { return pref_.mips(); }

private:
    void bakeBrdf();
    gfx::Shader env_, irrShader_, prefShader_, brdfShader_;
    gfx::CubeMap envMap_, irr_, pref_;
    gfx::Texture2D brdf_;
    gfx::FullscreenTriangle tri_;
};

}  // namespace render
