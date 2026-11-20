// B4: the pure maths behind the lighting (cascade fitting and the time-of-day looks). No GL needed.
#include <doctest/doctest.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "render/look.h"
#include "render/shadows.h"

using namespace render;
using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace {
const float FOV = glm::radians(42.f), ASPECT = 16.f / 9.f, NEARP = 0.1f;
const vec3 SPLITS(12.f, 35.f, 100.f);
constexpr int SIZE = 2048;
vec3 sunDir() { return glm::normalize(vec3(-18, 40, 15)); }
mat4 camView(vec3 eye) { return glm::lookAt(eye, eye + vec3(0.1f, -0.05f, -1.f), vec3(0, 1, 0)); }
}  // namespace

TEST_SUITE("Cascaded shadow maps") {
    TEST_CASE("every cascade contains the corners of its slice of the camera frustum") {
        const mat4 view = camView(vec3(3, 2.5f, 31));
        const CascadeSet cs = fitCascades(view, FOV, ASPECT, NEARP, sunDir(), SIZE, SPLITS, 90.f);
        for (int c = 0; c < 3; c++) {
            const float n = c == 0 ? NEARP : SPLITS[c - 1], f = SPLITS[c];
            const mat4 inv = glm::inverse(glm::perspective(FOV, ASPECT, n, f) * view);
            for (int i = 0; i < 8; i++) {
                const vec4 w = inv * vec4(i & 1 ? 1.f : -1.f, i & 2 ? 1.f : -1.f, i & 4 ? 1.f : -1.f, 1.f);
                const vec4 clip = cs.viewProj[c] * vec4(vec3(w) / w.w, 1.f);
                INFO("cascade " << c << " corner " << i);
                CHECK(std::fabs(clip.x) <= 1.001f);
                CHECK(std::fabs(clip.y) <= 1.001f);
                CHECK(std::fabs(clip.z) <= 1.001f);
            }
        }
    }

    TEST_CASE("casters between the frustum and the sun are inside the depth range") {
        const mat4 view = camView(vec3(0, 2.5f, 31));
        const CascadeSet cs = fitCascades(view, FOV, ASPECT, NEARP, sunDir(), SIZE, SPLITS, 90.f);
        // A tall object 40 m towards the sun from a point on the pitch must not be clipped by the near plane.
        const vec3 onPitch(0, 0, 20);
        const vec3 caster = onPitch + sunDir() * 40.f;
        const vec4 clip = cs.viewProj[0] * vec4(caster, 1.f);
        CHECK(clip.z >= -1.f);
        CHECK(clip.z <= 1.f);
    }

    TEST_CASE("texel snapping: a moving camera shifts the shadow map by whole texels only") {
        const vec3 p(1.7f, 0.3f, 22.f);  // a fixed world point
        auto texelPos = [&](vec3 eye, int c) {
            const CascadeSet cs = fitCascades(camView(eye), FOV, ASPECT, NEARP, sunDir(), SIZE, SPLITS, 90.f);
            const vec4 clip = cs.viewProj[c] * vec4(p, 1.f);
            return glm::vec2(clip.x, clip.y) * 0.5f * static_cast<float>(SIZE);  // position in texels
        };
        for (int c = 0; c < 3; c++) {
            const glm::vec2 a = texelPos(vec3(3.00f, 2.5f, 31.f), c);
            const glm::vec2 b = texelPos(vec3(3.013f, 2.5f, 31.007f), c);
            const glm::vec2 d = b - a;
            INFO("cascade " << c);
            CHECK(std::fabs(d.x - std::round(d.x)) < 0.02f);
            CHECK(std::fabs(d.y - std::round(d.y)) < 0.02f);
        }
    }

    TEST_CASE("texel sizes grow with the cascade and match the ortho extent") {
        const CascadeSet cs = fitCascades(camView(vec3(0, 2.5f, 31)), FOV, ASPECT, NEARP, sunDir(), SIZE, SPLITS, 90.f);
        CHECK(cs.texel.x < cs.texel.y);
        CHECK(cs.texel.y < cs.texel.z);
        CHECK(cs.splits.x == 12.f);
        CHECK(cs.splits.z == 100.f);
    }
}

TEST_SUITE("Time-of-day looks") {
    TEST_CASE("the sun is a unit vector above the horizon and the light has energy") {
        for (TimeOfDay t : {TimeOfDay::Day, TimeOfDay::Dusk, TimeOfDay::Night}) {
            const Look l = lookFor(t);
            CHECK(glm::length(l.sunDir) == doctest::Approx(1.f).epsilon(1e-4));
            CHECK(l.sunDir.y > 0.1f);
            CHECK(l.sunE > 0);
            CHECK(l.exposure > 0);
            CHECK(l.fogNear < l.fogFar);
            CHECK(l.sunCos < 1.f);
            CHECK(l.sunCos > 0.99f);
        }
    }

    TEST_CASE("day is brightest, dusk is warm and low, night is dim and blue") {
        const Look day = lookFor(TimeOfDay::Day), dusk = lookFor(TimeOfDay::Dusk), night = lookFor(TimeOfDay::Night);
        CHECK(day.sunE > dusk.sunE);
        CHECK(dusk.sunE > night.sunE);
        CHECK(dusk.sunDir.y < day.sunDir.y);  // lower sun
        CHECK(dusk.sunColor.r > dusk.sunColor.b);  // warm
        CHECK(night.sunColor.b > night.sunColor.r);  // blue
        CHECK(night.horizon.b < day.horizon.b);
    }
}
