// Port of src/sim/presets.ts.
#pragma once
#include <array>
#include <string>

#include "sim/settings.h"

namespace sim {

struct DifficultyPreset {
    std::string label;
    double accuracyBonus = 0;  // added to the taker's accuracy
    SettingsPatch settings;
};

enum class DifficultyName { Easy, Normal, Hard, Pro };
inline constexpr std::array<const char*, 4> DIFFICULTY_NAMES = {"easy", "normal", "hard", "pro"};
using Presets = std::array<DifficultyPreset, 4>;

}  // namespace sim
