#include "render/shapes.h"

#include <cmath>

namespace render {

using gfx::Mesh;
using gfx::Vertex;

namespace {
constexpr float PI = 3.14159265358979f;
}

Mesh makeSphere(float r, int slices, int stacks) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int i = 0; i <= stacks; i++) {
        const float phi = PI * i / stacks;
        for (int j = 0; j <= slices; j++) {
            const float th = 2 * PI * j / slices;
            const glm::vec3 n(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
            v.push_back({n * r, n});
        }
    }
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++) {
            const unsigned a = i * (slices + 1) + j, b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return Mesh(v, idx);
}

Mesh makeCapsule(float r, float length, int slices, int stacks) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const float half = length / 2;
    // Rings from the top pole down to the bottom pole: hemisphere, (cylinder is the gap between the two middle rings).
    std::vector<std::pair<float, float>> rings;  // (y offset of ring centre, polar angle)
    for (int i = 0; i <= stacks; i++) rings.push_back({half, (PI / 2) * i / stacks});
    for (int i = 0; i <= stacks; i++) rings.push_back({-half, PI / 2 + (PI / 2) * i / stacks});
    for (auto [yc, phi] : rings)
        for (int j = 0; j <= slices; j++) {
            const float th = 2 * PI * j / slices;
            const glm::vec3 n(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
            v.push_back({glm::vec3(0, yc, 0) + n * r, n});
        }
    const int rowsN = static_cast<int>(rings.size());
    for (int i = 0; i < rowsN - 1; i++)
        for (int j = 0; j < slices; j++) {
            const unsigned a = i * (slices + 1) + j, b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return Mesh(v, idx);
}

Mesh makeCylinder(float r, float h, int slices) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int j = 0; j <= slices; j++) {
        const float th = 2 * PI * j / slices;
        const glm::vec3 n(std::cos(th), 0, std::sin(th));
        v.push_back({glm::vec3(n.x * r, h / 2, n.z * r), n});
        v.push_back({glm::vec3(n.x * r, -h / 2, n.z * r), n});
    }
    for (int j = 0; j < slices; j++) {
        const unsigned a = j * 2;
        idx.insert(idx.end(), {a, a + 1, a + 2, a + 2, a + 1, a + 3});
    }
    const unsigned topC = static_cast<unsigned>(v.size());
    v.push_back({{0, h / 2, 0}, {0, 1, 0}});
    for (int j = 0; j <= slices; j++) {
        const float th = 2 * PI * j / slices;
        v.push_back({{std::cos(th) * r, h / 2, std::sin(th) * r}, {0, 1, 0}});
    }
    for (int j = 0; j < slices; j++) idx.insert(idx.end(), {topC, topC + 2 + j, topC + 1 + j});
    const unsigned botC = static_cast<unsigned>(v.size());
    v.push_back({{0, -h / 2, 0}, {0, -1, 0}});
    for (int j = 0; j <= slices; j++) {
        const float th = 2 * PI * j / slices;
        v.push_back({{std::cos(th) * r, -h / 2, std::sin(th) * r}, {0, -1, 0}});
    }
    for (int j = 0; j < slices; j++) idx.insert(idx.end(), {botC, botC + 1 + j, botC + 2 + j});
    return Mesh(v, idx);
}

Mesh makeBox(float x, float y, float z) {
    const float hx = x / 2, hy = y / 2, hz = z / 2;
    struct Face { glm::vec3 n, u, w; };
    const Face faces[6] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}, {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
                           {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}, {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    const glm::vec3 h(hx, hy, hz);
    for (const Face& f : faces) {
        const unsigned b = static_cast<unsigned>(v.size());
        for (auto [su, sw] : {std::pair{-1, -1}, {1, -1}, {1, 1}, {-1, 1}})
            v.push_back({(f.n + f.u * static_cast<float>(su) + f.w * static_cast<float>(sw)) * h, f.n});
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    return Mesh(v, idx);
}

Mesh makeDisc(float r, int slices) { return makeRing(0, r, slices); }

Mesh makeRing(float inner, float outer, int slices, float start, float arc) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (int j = 0; j <= slices; j++) {
        const float a = start + arc * j / slices;
        v.push_back({{std::cos(a) * inner, std::sin(a) * inner, 0}, {0, 0, 1}});
        v.push_back({{std::cos(a) * outer, std::sin(a) * outer, 0}, {0, 0, 1}});
    }
    for (int j = 0; j < slices; j++) {
        const unsigned a = j * 2;
        idx.insert(idx.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
    }
    return Mesh(v, idx);
}

Mesh makePlane(float w, float h) {
    std::vector<Vertex> v = {{{-w / 2, -h / 2, 0}, {0, 0, 1}}, {{w / 2, -h / 2, 0}, {0, 0, 1}}, {{w / 2, h / 2, 0}, {0, 0, 1}}, {{-w / 2, h / 2, 0}, {0, 0, 1}}};
    return Mesh(v, {0, 1, 2, 0, 2, 3});
}

Mesh makePolygon(const std::vector<glm::vec2>& pts) {
    std::vector<Vertex> v;
    std::vector<unsigned> idx;
    for (const auto& p : pts) v.push_back({{p.x, p.y, 0}, {0, 0, 1}});
    for (unsigned i = 1; i + 1 < pts.size(); i++) idx.insert(idx.end(), {0, i, i + 1});
    return Mesh(v, idx);
}

}  // namespace render
