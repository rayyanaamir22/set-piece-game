// Time-of-day looks (port of LOOKS in render/GameView.ts).
#pragma once
#include <glm/glm.hpp>

namespace render {

enum class TimeOfDay { Day, Dusk, Night };

struct Look {
    glm::vec3 bg, fog;  // sRGB
    float fogNear, fogFar;
    glm::vec3 sky, ground;  // hemisphere colours (sRGB)
    float hemi;
    glm::vec3 sun;  // sRGB
    float sunI;
    glm::vec3 sunPos;
};

inline glm::vec3 hex(unsigned h) { return {((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f}; }

inline Look lookFor(TimeOfDay t) {
    switch (t) {
    case TimeOfDay::Day: return {hex(0x9ec9f0), hex(0x9ec9f0), 60, 180, hex(0xdfeaff), hex(0x3b5e2b), 1.0f, hex(0xffffff), 2.2f, {-18, 40, 30}};
    case TimeOfDay::Dusk: return {hex(0xdf8c63), hex(0xd48a68), 50, 170, hex(0xffd2a8), hex(0x2a3320), 0.75f, hex(0xffa868), 1.5f, {-34, 16, 24}};
    case TimeOfDay::Night: return {hex(0x070b1a), hex(0x0a1022), 40, 150, hex(0x6f86c8), hex(0x0c140c), 0.35f, hex(0x8fa6ff), 0.15f, {-18, 40, 30}};
    }
    return lookFor(TimeOfDay::Day);
}

}  // namespace render
