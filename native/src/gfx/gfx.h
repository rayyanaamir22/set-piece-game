// Thin GL 4.1 core wrapper: shaders (loaded from files, hot-reloadable), meshes, render targets.
// This is the only layer that talks to GL directly (so a Metal backend can replace it later).
#pragma once
#include <glad/gl.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace gfx {

class Shader {
public:
    Shader() = default;
    // Loads `<dir>/<vs>` and `<dir>/<fs>`; `#include "x"` lines are resolved relative to `dir`.
    // Throws on the first compile error; later hot reloads keep the old program on error.
    Shader(const std::string& dir, const std::string& vs, const std::string& fs);
    Shader(Shader&& o) noexcept { *this = std::move(o); }
    Shader& operator=(Shader&& o) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    ~Shader();

    void use() const { glUseProgram(prog_); }
    // Rebuilds the program when any source file changed on disk. Returns true if it did.
    bool reloadIfChanged();

    GLint loc(const char* name) const;
    void set(const char* name, float v) const { glUniform1f(loc(name), v); }
    void set(const char* name, int v) const { glUniform1i(loc(name), v); }
    void set(const char* name, const glm::vec2& v) const { glUniform2fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::vec3& v) const { glUniform3fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::vec4& v) const { glUniform4fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::mat3& m) const { glUniformMatrix3fv(loc(name), 1, GL_FALSE, &m[0][0]); }
    void set(const char* name, const glm::mat4& m) const { glUniformMatrix4fv(loc(name), 1, GL_FALSE, &m[0][0]); }
    void setArray(const char* name, const glm::mat4* m, int n) const { glUniformMatrix4fv(loc(name), n, GL_FALSE, &(*m)[0][0]); }
    void setArray(const char* name, const glm::vec3* v, int n) const { glUniform3fv(loc(name), n, &v[0].x); }
    void setArray(const char* name, const float* v, int n) const { glUniform1fv(loc(name), n, v); }

private:
    GLuint build(std::vector<std::filesystem::path>& deps, std::string* error) const;
    GLuint prog_ = 0;
    std::string dir_, vs_, fs_;
    std::vector<std::filesystem::path> deps_;
    std::vector<std::filesystem::file_time_type> times_;
    mutable std::unordered_map<std::string, GLint> locs_;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

// Skinned vertex: position, normal, material slot, four joint indices and weights.
struct SkinnedVertex {
    glm::vec3 pos, normal;
    float material;
    int joints[4];
    float weights[4];
};

class SkinnedMesh {
public:
    SkinnedMesh() = default;
    SkinnedMesh(const std::vector<SkinnedVertex>& v, const std::vector<unsigned>& idx);
    SkinnedMesh(SkinnedMesh&& o) noexcept { *this = std::move(o); }
    SkinnedMesh& operator=(SkinnedMesh&& o) noexcept;
    SkinnedMesh(const SkinnedMesh&) = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;
    ~SkinnedMesh();
    void draw() const;

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

// Indexed triangle mesh (static) of position + normal.
class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& v, const std::vector<unsigned>& idx);
    Mesh(Mesh&& o) noexcept { *this = std::move(o); }
    Mesh& operator=(Mesh&& o) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh();
    void draw() const;
    bool valid() const { return vao_ != 0; }

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

// Dynamic position-only line / triangle list, re-uploaded every frame it changes.
class DynLines {
public:
    DynLines();
    DynLines(DynLines&& o) noexcept { *this = std::move(o); }
    DynLines& operator=(DynLines&& o) noexcept;
    DynLines(const DynLines&) = delete;
    DynLines& operator=(const DynLines&) = delete;
    ~DynLines();
    void upload(const std::vector<glm::vec3>& pts);
    void draw(GLenum mode = GL_LINES) const;
    size_t size() const { return n_; }

private:
    GLuint vao_ = 0, vbo_ = 0;
    size_t n_ = 0, cap_ = 0;
};

// An empty VAO for attribute-less fullscreen-triangle draws.
class FullscreenTriangle {
public:
    FullscreenTriangle() { glGenVertexArrays(1, &vao_); }
    ~FullscreenTriangle() { if (vao_) glDeleteVertexArrays(1, &vao_); }
    FullscreenTriangle(const FullscreenTriangle&) = delete;
    FullscreenTriangle& operator=(const FullscreenTriangle&) = delete;
    void draw() const {
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

private:
    GLuint vao_ = 0;
};

// Scene render target: a multisampled HDR colour + depth, a resolved HDR texture for post-processing, and a
// multisampled LDR target that shares the depth buffer (so world-space aids can be drawn after tone mapping).
class SceneTarget {
public:
    ~SceneTarget() { destroy(); }
    void create(int w, int h, int samples);
    void destroy();
    bool matches(int w, int h, int samples) const { return w == w_ && h == h_ && samples == samples_ && fboHdr_; }
    void bindHdr() const;       // draw the lit scene (RGBA16F, MSAA)
    void resolveHdr() const;    // MSAA -> resolveTex()
    void bindLdr() const;       // draw into the LDR target (RGBA8, MSAA, same depth)
    void presentTo(GLuint fbo) const;  // resolve the LDR target into `fbo` (0 = window)
    GLuint resolveTex() const { return resolveTex_; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    int w_ = 0, h_ = 0, samples_ = 0;
    GLuint fboHdr_ = 0, fboResolve_ = 0, fboLdr_ = 0;
    GLuint hdrRb_ = 0, ldrRb_ = 0, depthRb_ = 0, resolveTex_ = 0;
};

// Cascaded shadow map storage: a depth texture array with one framebuffer per layer.
class ShadowArray {
public:
    static constexpr int LAYERS = 3;
    ~ShadowArray() { destroy(); }
    void create(int size);
    void destroy();
    int size() const { return size_; }
    void bindLayer(int i) const;
    GLuint tex() const { return tex_; }

private:
    int size_ = 0;
    GLuint tex_ = 0;
    GLuint fbo_[LAYERS] = {0, 0, 0};
};

// A mip-mapped RGBA16F cube map.
class CubeMap {
public:
    CubeMap() = default;
    CubeMap(CubeMap&& o) noexcept { *this = std::move(o); }
    CubeMap& operator=(CubeMap&& o) noexcept;
    CubeMap(const CubeMap&) = delete;
    CubeMap& operator=(const CubeMap&) = delete;
    ~CubeMap() { destroy(); }
    void create(int size, int mips);
    void destroy();
    GLuint tex() const { return tex_; }
    int size() const { return size_; }
    int mips() const { return mips_; }
    // Render into a face / mip of this cube map using a temporary framebuffer.
    void bindFace(int face, int mip) const;

private:
    GLuint tex_ = 0, fbo_ = 0;
    int size_ = 0, mips_ = 0;
};

// A 2D RG16F texture usable as a render target (the BRDF lookup).
class Texture2D {
public:
    Texture2D() = default;
    Texture2D(Texture2D&& o) noexcept { *this = std::move(o); }
    Texture2D& operator=(Texture2D&& o) noexcept;
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    ~Texture2D() { destroy(); }
    void create(int w, int h);
    void destroy();
    GLuint tex() const { return tex_; }
    void bind() const;
    int width() const { return w_; }

private:
    GLuint tex_ = 0, fbo_ = 0;
    int w_ = 0, h_ = 0;
};

// Clamp a requested MSAA sample count to what the GPU supports.
int clampSamples(int wanted);

// GPU timer query pair (rolling), for the in-game frame-time overlay.
class GpuTimer {
public:
    GpuTimer();
    ~GpuTimer();
    void begin();
    void end();
    double lastMs() const { return lastMs_; }

private:
    GLuint q_[2] = {0, 0};
    int cur_ = 0;
    bool started_ = false, havePrev_ = false;
    double lastMs_ = 0;
};

}  // namespace gfx
