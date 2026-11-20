#include "gfx/gfx.h"

#include <cstdio>
#include <stdexcept>

namespace gfx {

namespace {
GLuint compile(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        throw std::runtime_error(std::string(name) + (type == GL_VERTEX_SHADER ? " (vertex): " : " (fragment): ") + log);
    }
    return s;
}
}  // namespace

Shader::Shader(const char* vs, const char* fs, const char* name) {
    GLuint v = compile(GL_VERTEX_SHADER, vs, name);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, name);
    prog_ = glCreateProgram();
    glAttachShader(prog_, v);
    glAttachShader(prog_, f);
    glBindAttribLocation(prog_, 0, "aPos");
    glBindAttribLocation(prog_, 1, "aNormal");
    glLinkProgram(prog_);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog_, sizeof log, nullptr, log);
        throw std::runtime_error(std::string(name) + " link: " + log);
    }
}
Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (prog_) glDeleteProgram(prog_);
        prog_ = o.prog_;
        o.prog_ = 0;
    }
    return *this;
}
Shader::~Shader() {
    if (prog_) glDeleteProgram(prog_);
}
GLint Shader::loc(const char* name) const { return glGetUniformLocation(prog_, name); }

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

}  // namespace gfx
