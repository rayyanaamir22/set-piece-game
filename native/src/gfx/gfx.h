// Thin GL 4.1 core wrapper: shaders, static/dynamic meshes, a few state helpers.
// This is the only layer that talks to GL directly (so a Metal backend can replace it later).
#pragma once
#include <glad/gl.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace gfx {

class Shader {
public:
    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc, const char* name = "shader");
    Shader(Shader&& o) noexcept { *this = std::move(o); }
    Shader& operator=(Shader&& o) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    ~Shader();

    void use() const { glUseProgram(prog_); }
    GLint loc(const char* name) const;
    void set(const char* name, float v) const { glUniform1f(loc(name), v); }
    void set(const char* name, int v) const { glUniform1i(loc(name), v); }
    void set(const char* name, const glm::vec2& v) const { glUniform2fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::vec3& v) const { glUniform3fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::vec4& v) const { glUniform4fv(loc(name), 1, &v.x); }
    void set(const char* name, const glm::mat3& m) const { glUniformMatrix3fv(loc(name), 1, GL_FALSE, &m[0][0]); }
    void set(const char* name, const glm::mat4& m) const { glUniformMatrix4fv(loc(name), 1, GL_FALSE, &m[0][0]); }

private:
    GLuint prog_ = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
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

}  // namespace gfx
