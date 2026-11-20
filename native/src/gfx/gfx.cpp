#include "gfx/gfx.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace gfx {

namespace fs = std::filesystem;

namespace {
// Reads a shader file, expanding `#include "file"` lines (relative to the shader directory).
bool readExpanded(const fs::path& dir, const std::string& name, std::string& out, std::vector<fs::path>& deps, int depth = 0) {
    if (depth > 8) return false;
    const fs::path path = dir / name;
    std::ifstream f(path);
    if (!f) return false;
    deps.push_back(path);
    std::string line;
    while (std::getline(f, line)) {
        const auto p = line.find("#include");
        if (p != std::string::npos) {
            const auto a = line.find('"', p), b = line.find('"', a + 1);
            if (a == std::string::npos || b == std::string::npos) return false;
            if (!readExpanded(dir, line.substr(a + 1, b - a - 1), out, deps, depth + 1)) return false;
        } else {
            out += line;
            out += '\n';
        }
    }
    return true;
}

GLuint compile(GLenum type, const std::string& src, const std::string& name, std::string* error) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        *error = name + (type == GL_VERTEX_SHADER ? " (vertex): " : " (fragment): ") + log;
        glDeleteShader(s);
        return 0;
    }
    return s;
}
}  // namespace

GLuint Shader::build(std::vector<fs::path>& deps, std::string* error) const {
    std::string vsrc, fsrc;
    deps.clear();
    if (!readExpanded(dir_, vs_, vsrc, deps) || !readExpanded(dir_, fs_, fsrc, deps)) {
        *error = "cannot read shader source " + vs_ + " / " + fs_ + " in " + dir_;
        return 0;
    }
    GLuint v = compile(GL_VERTEX_SHADER, vsrc, vs_, error);
    if (!v) return 0;
    GLuint f = compile(GL_FRAGMENT_SHADER, fsrc, fs_, error);
    if (!f) {
        glDeleteShader(v);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof log, nullptr, log);
        *error = vs_ + "+" + fs_ + " link: " + log;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

Shader::Shader(const std::string& dir, const std::string& vs, const std::string& fs) : dir_(dir), vs_(vs), fs_(fs) {
    std::string err;
    prog_ = build(deps_, &err);
    if (!prog_) throw std::runtime_error(err);
    for (const auto& d : deps_) times_.push_back(fs::last_write_time(d));
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (prog_) glDeleteProgram(prog_);
        prog_ = o.prog_;
        dir_ = std::move(o.dir_);
        vs_ = std::move(o.vs_);
        fs_ = std::move(o.fs_);
        deps_ = std::move(o.deps_);
        times_ = std::move(o.times_);
        locs_ = std::move(o.locs_);
        o.prog_ = 0;
    }
    return *this;
}
Shader::~Shader() {
    if (prog_) glDeleteProgram(prog_);
}

bool Shader::reloadIfChanged() {
    bool changed = false;
    std::error_code ec;
    for (size_t i = 0; i < deps_.size(); i++) {
        const auto t = fs::last_write_time(deps_[i], ec);
        if (!ec && t != times_[i]) changed = true;
    }
    if (!changed) return false;
    std::string err;
    std::vector<fs::path> deps;
    GLuint p = build(deps, &err);
    if (!p) {
        std::fprintf(stderr, "shader reload failed (keeping the old one): %s\n", err.c_str());
        for (size_t i = 0; i < deps_.size(); i++) times_[i] = fs::last_write_time(deps_[i], ec);  // do not retry until edited again
        return false;
    }
    glDeleteProgram(prog_);
    prog_ = p;
    deps_ = deps;
    times_.clear();
    for (const auto& d : deps_) times_.push_back(fs::last_write_time(d, ec));
    locs_.clear();
    std::fprintf(stderr, "reloaded %s + %s\n", vs_.c_str(), fs_.c_str());
    return true;
}

GLint Shader::loc(const char* name) const {
    auto it = locs_.find(name);
    if (it != locs_.end()) return it->second;
    const GLint l = glGetUniformLocation(prog_, name);
    locs_.emplace(name, l);
    return l;
}

// ---------------------------------------------------------------- meshes
Mesh::Mesh(const std::vector<Vertex>& v, const std::vector<unsigned>& idx) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(Vertex)), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned)), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glBindVertexArray(0);
    count_ = static_cast<GLsizei>(idx.size());
}
Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        this->~Mesh();
        vao_ = o.vao_; vbo_ = o.vbo_; ebo_ = o.ebo_; count_ = o.count_;
        o.vao_ = o.vbo_ = o.ebo_ = 0;
        o.count_ = 0;
    }
    return *this;
}
Mesh::~Mesh() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;
}
void Mesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}

SkinnedMesh::SkinnedMesh(const std::vector<SkinnedVertex>& v, const std::vector<unsigned>& idx) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(SkinnedVertex)), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned)), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, material)));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, joints)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, weights)));
    glBindVertexArray(0);
    count_ = static_cast<GLsizei>(idx.size());
}
SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& o) noexcept {
    if (this != &o) {
        this->~SkinnedMesh();
        vao_ = o.vao_; vbo_ = o.vbo_; ebo_ = o.ebo_; count_ = o.count_;
        o.vao_ = o.vbo_ = o.ebo_ = 0;
        o.count_ = 0;
    }
    return *this;
}
SkinnedMesh::~SkinnedMesh() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;
}
void SkinnedMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}

DynLines::DynLines() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
}
DynLines& DynLines::operator=(DynLines&& o) noexcept {
    if (this != &o) {
        this->~DynLines();
        vao_ = o.vao_; vbo_ = o.vbo_; n_ = o.n_; cap_ = o.cap_;
        o.vao_ = o.vbo_ = 0;
        o.n_ = o.cap_ = 0;
    }
    return *this;
}
DynLines::~DynLines() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    vao_ = vbo_ = 0;
}
void DynLines::upload(const std::vector<glm::vec3>& pts) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    if (pts.size() > cap_) {
        cap_ = pts.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cap_ * sizeof(glm::vec3)), nullptr, GL_DYNAMIC_DRAW);
    }
    if (!pts.empty()) glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(pts.size() * sizeof(glm::vec3)), pts.data());
    n_ = pts.size();
}
void DynLines::draw(GLenum mode) const {
    if (!n_) return;
    glBindVertexArray(vao_);
    glDrawArrays(mode, 0, static_cast<GLsizei>(n_));
}

// ---------------------------------------------------------------- render targets
namespace {
void check(const char* what) {
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error(std::string("incomplete framebuffer: ") + what);
}
}  // namespace

int clampSamples(int wanted) {
    GLint mx = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &mx);
    return std::max(1, std::min<int>(wanted, mx));
}

void SceneTarget::create(int w, int h, int samples) {
    destroy();
    w_ = w;
    h_ = h;
    samples_ = samples;
    glGenRenderbuffers(1, &hdrRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    glGenRenderbuffers(1, &ldrRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, ldrRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h);
    glGenRenderbuffers(1, &depthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT32F, w, h);

    glGenFramebuffers(1, &fboHdr_);
    glBindFramebuffer(GL_FRAMEBUFFER, fboHdr_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, hdrRb_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb_);
    check("scene hdr");

    glGenTextures(1, &resolveTex_);
    glBindTexture(GL_TEXTURE_2D, resolveTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fboResolve_);
    glBindFramebuffer(GL_FRAMEBUFFER, fboResolve_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTex_, 0);
    check("scene resolve");

    glGenFramebuffers(1, &fboLdr_);
    glBindFramebuffer(GL_FRAMEBUFFER, fboLdr_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ldrRb_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb_);
    check("scene ldr");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneTarget::destroy() {
    if (fboHdr_) glDeleteFramebuffers(1, &fboHdr_);
    if (fboResolve_) glDeleteFramebuffers(1, &fboResolve_);
    if (fboLdr_) glDeleteFramebuffers(1, &fboLdr_);
    if (hdrRb_) glDeleteRenderbuffers(1, &hdrRb_);
    if (ldrRb_) glDeleteRenderbuffers(1, &ldrRb_);
    if (depthRb_) glDeleteRenderbuffers(1, &depthRb_);
    if (resolveTex_) glDeleteTextures(1, &resolveTex_);
    fboHdr_ = fboResolve_ = fboLdr_ = hdrRb_ = ldrRb_ = depthRb_ = resolveTex_ = 0;
    w_ = h_ = samples_ = 0;
}

void SceneTarget::bindHdr() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fboHdr_);
    glViewport(0, 0, w_, h_);
}
void SceneTarget::bindLdr() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fboLdr_);
    glViewport(0, 0, w_, h_);
}
void SceneTarget::resolveHdr() const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboHdr_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboResolve_);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}
void SceneTarget::presentTo(GLuint fbo) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboLdr_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void ShadowArray::create(int size) {
    destroy();
    size_ = size;
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size, size, LAYERS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(LAYERS, fbo_);
    for (int i = 0; i < LAYERS; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex_, 0, i);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        check("shadow layer");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void ShadowArray::destroy() {
    if (fbo_[0]) glDeleteFramebuffers(LAYERS, fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_[0] = fbo_[1] = fbo_[2] = 0;
    tex_ = 0;
    size_ = 0;
}
void ShadowArray::bindLayer(int i) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
    glViewport(0, 0, size_, size_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

CubeMap& CubeMap::operator=(CubeMap&& o) noexcept {
    if (this != &o) {
        destroy();
        tex_ = o.tex_; fbo_ = o.fbo_; size_ = o.size_; mips_ = o.mips_;
        o.tex_ = o.fbo_ = 0;
    }
    return *this;
}
void CubeMap::create(int size, int mips) {
    destroy();
    size_ = size;
    mips_ = mips;
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_);
    for (int m = 0; m < mips; m++)
        for (int f = 0; f < 6; f++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, m, GL_RGBA16F, std::max(1, size >> m), std::max(1, size >> m), 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mips - 1);
    glGenFramebuffers(1, &fbo_);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}
void CubeMap::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_ = tex_ = 0;
}
void CubeMap::bindFace(int face, int mip) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex_, mip);
    glViewport(0, 0, std::max(1, size_ >> mip), std::max(1, size_ >> mip));
}

Texture2D& Texture2D::operator=(Texture2D&& o) noexcept {
    if (this != &o) {
        destroy();
        tex_ = o.tex_; fbo_ = o.fbo_; w_ = o.w_; h_ = o.h_;
        o.tex_ = o.fbo_ = 0;
    }
    return *this;
}
void Texture2D::create(int w, int h) {
    destroy();
    w_ = w;
    h_ = h;
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, w, h, 0, GL_RG, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    check("texture2d");
}
void Texture2D::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_ = tex_ = 0;
}
void Texture2D::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w_, h_);
}

GpuTimer::GpuTimer() { glGenQueries(2, q_); }
GpuTimer::~GpuTimer() { glDeleteQueries(2, q_); }
void GpuTimer::begin() {
    glBeginQuery(GL_TIME_ELAPSED, q_[cur_]);
    started_ = true;
}
void GpuTimer::end() {
    if (!started_) return;
    glEndQuery(GL_TIME_ELAPSED);
    // Read the query from the previous frame (it has had time to finish).
    const int prev = 1 - cur_;
    if (havePrev_) {
        GLint avail = 0;
        glGetQueryObjectiv(q_[prev], GL_QUERY_RESULT_AVAILABLE, &avail);
        if (avail) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(q_[prev], GL_QUERY_RESULT, &ns);
            lastMs_ = static_cast<double>(ns) / 1e6;
        }
    }
    havePrev_ = true;
    cur_ = prev;
    started_ = false;
}

}  // namespace gfx
