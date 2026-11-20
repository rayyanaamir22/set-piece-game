#include "anim/character_mesh.h"

#include <cmath>

namespace anim {

using glm::vec3;

namespace {
constexpr float PI = 3.14159265f;
float smooth(float e0, float e1, float x) {
    const float t = glm::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

struct Builder {
    SkinMesh mesh;

    // A tapered tube from p0 to p1 with optional hemispherical caps. Skin weights: `own` for most of the tube, blending
    // half-way into `prev` at the start and `next` at the end (so joints bend smoothly without cracks).
    void tube(vec3 p0, vec3 p1, float r0, float r1, int own, int prev, int next, Material m0, Material m1, float splitT, bool capStart, bool capEnd,
              int slices = 12, float blend = 0.45f) {
        const vec3 axis = p1 - p0;
        const float len = glm::length(axis);
        const vec3 a = axis / len;
        // Any basis perpendicular to the axis.
        const vec3 ref = std::fabs(a.y) < 0.9f ? vec3(0, 1, 0) : vec3(0, 0, 1);
        const vec3 u = glm::normalize(glm::cross(a, ref)), v = glm::cross(a, u);

        struct Ring { float s, r; Material m; bool breakBefore; };  // s = distance along the axis from p0 (may be < 0 or > len on caps)
        std::vector<Ring> rings;
        if (capStart)
            for (int i = 0; i < 4; i++) {
                const float phi = (PI / 2) * (1.f - static_cast<float>(i) / 4.f);  // 90deg -> 0
                rings.push_back({-r0 * std::sin(phi), r0 * std::cos(phi), m0, false});
            }
        const int mid = 6;
        bool splitDone = splitT > 1.f || m0 == m1;
        for (int i = 0; i <= mid; i++) {
            const float t = static_cast<float>(i) / mid;
            if (!splitDone && t >= splitT) {
                // Duplicate ring at the material boundary: no triangles join the two, so the edge is clean.
                const float ts = splitT;
                rings.push_back({ts * len, r0 + (r1 - r0) * ts, m0, false});
                rings.push_back({ts * len, r0 + (r1 - r0) * ts, m1, true});
                splitDone = true;
            }
            if (t == splitT && splitDone && m0 != m1 && t < 1.f && false) continue;
            rings.push_back({t * len, r0 + (r1 - r0) * t, splitDone && t >= splitT && m0 != m1 ? m1 : m0, false});
        }
        if (capEnd)
            for (int i = 1; i <= 4; i++) {
                const float phi = (PI / 2) * static_cast<float>(i) / 4.f;
                rings.push_back({len + r1 * std::sin(phi), r1 * std::cos(phi), m1, false});
            }

        const unsigned base = static_cast<unsigned>(mesh.vertices.size());
        for (const Ring& ring : rings) {
            const float t = glm::clamp(ring.s / len, 0.f, 1.f);
            float wNext = next >= 0 ? 0.5f * smooth(1.f - blend, 1.f, t) : 0.f;
            float wPrev = prev >= 0 ? 0.5f * (1.f - smooth(0.f, blend, t)) : 0.f;
            if (ring.s > len) wNext = next >= 0 ? 0.5f : 0.f;
            if (ring.s < 0) wPrev = prev >= 0 ? 0.5f : 0.f;
            const vec3 c = p0 + a * ring.s;
            for (int k = 0; k < slices; k++) {
                const float th = 2 * PI * static_cast<float>(k) / slices;
                const vec3 radial = u * std::cos(th) + v * std::sin(th);
                SkinVertex sv{};
                sv.pos = c + radial * std::max(ring.r, 1e-4f);
                // Caps: normal from the cap centre; the shaft: radial.
                sv.normal = glm::normalize(ring.s < 0 ? sv.pos - p0 : (ring.s > len ? sv.pos - p1 : radial));
                sv.material = static_cast<float>(ring.m);
                sv.joints[0] = own;
                sv.joints[1] = prev >= 0 ? prev : own;
                sv.joints[2] = next >= 0 ? next : own;
                sv.joints[3] = own;
                sv.weights[0] = 1.f - wPrev - wNext;
                sv.weights[1] = wPrev;
                sv.weights[2] = wNext;
                sv.weights[3] = 0.f;
                mesh.vertices.push_back(sv);
            }
        }
        for (size_t i = 0; i + 1 < rings.size(); i++) {
            if (rings[i + 1].breakBefore) continue;
            for (int k = 0; k < slices; k++) {
                const unsigned a0 = base + static_cast<unsigned>(i) * slices + k, a1 = base + static_cast<unsigned>(i) * slices + (k + 1) % slices;
                const unsigned b0 = a0 + slices, b1 = a1 + slices;
                mesh.indices.insert(mesh.indices.end(), {a0, b0, a1, a1, b0, b1});
            }
        }
    }
};

SkinMesh build() {
    const Skeleton& sk = Skeleton::humanoid();
    Builder b;
    auto J = [&](Bone bone) { return sk.restWorld[bone]; };

    // Torso: pelvis (shorts), belly and chest (shirt), shoulders.
    b.tube({0, 0.86f, 0}, {0, 1.06f, 0}, 0.165f, 0.18f, HIPS, -1, SPINE, MAT_SHORTS, MAT_SHORTS, 2.f, true, false, 14);
    b.tube({0, 1.04f, 0}, {0, 1.40f, 0}, 0.18f, 0.195f, SPINE, HIPS, CHEST, MAT_SHIRT, MAT_SHIRT, 2.f, false, false, 14);
    b.tube({0, 1.38f, 0}, {0, 1.60f, 0}, 0.20f, 0.17f, CHEST, SPINE, NECK, MAT_SHIRT, MAT_SHIRT, 2.f, false, true, 14, 0.4f);
    b.tube({-0.20f, 1.50f, 0}, {0.20f, 1.50f, 0}, 0.075f, 0.075f, CHEST, -1, -1, MAT_SHIRT, MAT_SHIRT, 2.f, true, true, 10);
    // Neck and head.
    b.tube({0, 1.58f, 0}, {0, 1.70f, 0}, 0.052f, 0.052f, NECK, CHEST, HEAD, MAT_SKIN, MAT_SKIN, 2.f, false, false, 10);
    b.tube({0, 1.73f, 0}, {0, 1.79f, 0}, 0.108f, 0.108f, HEAD, -1, -1, MAT_SKIN, MAT_SKIN, 2.f, true, true, 14);

    // Arms (sleeves on the upper arm), mirrored.
    for (int side = 0; side < 2; side++) {
        const float sx = side == 0 ? 1.f : -1.f;
        const Bone ua = side == 0 ? UARM_L : UARM_R, la = side == 0 ? LARM_L : LARM_R, ha = side == 0 ? HAND_L : HAND_R;
        const vec3 sh = J(ua), el = J(la), wr = J(ha);
        (void)sx;
        b.tube(sh, el, 0.058f, 0.048f, ua, CHEST, la, MAT_SHIRT, MAT_SKIN, 0.55f, true, false, 10);
        b.tube(el, wr, 0.046f, 0.040f, la, ua, ha, MAT_SKIN, MAT_SKIN, 2.f, false, false, 10);
        b.tube(wr, wr - vec3(0, HAND_LEN, 0), 0.042f, 0.030f, ha, la, -1, MAT_GLOVES, MAT_GLOVES, 2.f, false, true, 10);
    }
    // Legs: shorts to mid-thigh, skin, socks on the shin, boots.
    for (int side = 0; side < 2; side++) {
        const Bone th = side == 0 ? THIGH_L : THIGH_R, sh = side == 0 ? SHIN_L : SHIN_R, ft = side == 0 ? FOOT_L : FOOT_R;
        const vec3 hip = J(th), knee = J(sh), ankle = J(ft);
        b.tube(hip + vec3(0, 0.03f, 0), knee, 0.098f, 0.072f, th, HIPS, sh, MAT_SHORTS, MAT_SKIN, 0.35f, true, false, 12);
        b.tube(knee, ankle + vec3(0, 0.03f, 0), 0.068f, 0.05f, sh, th, ft, MAT_SKIN, MAT_SOCKS, 0.35f, false, false, 10);
        b.tube(ankle + vec3(0, -0.01f, -0.045f), ankle + vec3(0, -0.035f, 0.17f), 0.052f, 0.042f, ft, sh, -1, MAT_BOOTS, MAT_BOOTS, 2.f, true, true, 10);
    }
    return std::move(b.mesh);
}
}  // namespace

const SkinMesh& characterMesh() {
    static const SkinMesh mesh = build();
    return mesh;
}

}  // namespace anim
