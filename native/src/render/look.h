// Time-of-day looks: the HDR sky, the sun and the exposure (port of LOOKS in render/GameView.ts, now physically based).
#pragma once
#include <cmath>
#include <glm/glm.hpp>

namespace render {

enum class TimeOfDay { Day, Dusk, Night };
enum class Quality { Low, Medium, High };

inline glm::vec3 hex(unsigned h) { return {((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f}; }

struct Look {
    // Linear HDR radiance of the procedural sky.
    glm::vec3 zenith, horizon, ground;
    glm::vec3 sunDir;    // unit vector towards the sun (or moon)
    glm::vec3 sunColor;  // linear colour of the light
    float sunE;          // irradiance on a surface facing the sun
    float sunGlow;       // strength of the aureole / horizon glow
    float sunCos;        // cos of the visible disc's angular radius
    float exposure;
    float iblScale;      // scales the sky's contribution to lighting
    glm::vec3 fog;       // linear
    float fogNear, fogFar;
};

inline Look lookFor(TimeOfDay t) {
    Look l{};
    auto sun = [](glm::vec3 pos) { return glm::normalize(pos - glm::vec3(0, 0, 15)); };
    l.sunCos = std::cos(0.03f);
    switch (t) {
    case TimeOfDay::Day:
        l.zenith = {0.09f, 0.22f, 0.68f};
        l.horizon = {0.58f, 0.72f, 0.90f};
        l.ground = {0.10f, 0.14f, 0.08f};
        l.sunDir = sun({-18, 40, 30});
        l.sunColor = {1.0f, 0.95f, 0.85f};
        l.sunE = 3.4f;
        l.sunGlow = 0.35f;
        l.exposure = 0.78f;
        l.iblScale = 1.0f;
        l.fog = {0.56f, 0.70f, 0.88f};
        l.fogNear = 70;
        l.fogFar = 260;
        break;
    case TimeOfDay::Dusk:
        l.zenith = {0.06f, 0.10f, 0.30f};
        l.horizon = {0.95f, 0.42f, 0.20f};
        l.ground = {0.06f, 0.045f, 0.04f};
        l.sunDir = sun({-34, 16, 24});
        l.sunColor = {1.0f, 0.50f, 0.22f};
        l.sunE = 2.2f;
        l.sunGlow = 1.6f;
        l.exposure = 1.0f;
        l.iblScale = 1.0f;
        l.fog = {0.72f, 0.33f, 0.17f};
        l.fogNear = 50;
        l.fogFar = 220;
        break;
    case TimeOfDay::Night:
        l.zenith = {0.005f, 0.011f, 0.04f};
        l.horizon = {0.018f, 0.028f, 0.065f};
        l.ground = {0.006f, 0.009f, 0.008f};
        l.sunDir = sun({-18, 40, 30});
        l.sunColor = {0.55f, 0.65f, 1.0f};
        l.sunE = 0.40f;
        l.sunGlow = 0.3f;
        l.exposure = 1.3f;
        l.iblScale = 1.0f;
        l.fog = {0.017f, 0.026f, 0.06f};
        l.fogNear = 40;
        l.fogFar = 200;
        break;
    }
    return l;
}

}  // namespace render
