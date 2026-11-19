// Port of src/sim/settings.ts.
#pragma once
#include <optional>

#include "sim/math/vec3.h"

namespace sim {

enum class Frequency { Off, Sometimes, Always };
enum class KeeperType { Balanced, Quick, Tall, Strong };

inline constexpr double WALL_HEIGHT_MIN = 1.6, WALL_HEIGHT_MAX = 2.1;
inline constexpr double WALL_COUNT_MAX = 6;
inline constexpr double WIND_SPEED_MAX = 15;

struct SimSettings {
    bool wallEnabled = true;
    double wallHeight = 1.85;
    double wallCount = 0;  // 0 = automatic (a JS number, kept as double)
    bool wallJumps = true;
    Frequency lyingMan = Frequency::Sometimes;
    Frequency encroachment = Frequency::Sometimes;
    bool rebounds = true;
    KeeperType keeperType = KeeperType::Balanced;
    bool keeperPresent = true;
    double keeperSkill = 0.7;
    bool windEnabled = false;
    double windSpeed = 4;
    double windDirection = 90;
};

// Partial<SimSettings>: only the fields that are set override.
struct SettingsPatch {
    std::optional<bool> wallEnabled;
    std::optional<double> wallHeight;
    std::optional<double> wallCount;
    std::optional<bool> wallJumps;
    std::optional<Frequency> lyingMan;
    std::optional<Frequency> encroachment;
    std::optional<bool> rebounds;
    std::optional<KeeperType> keeperType;
    std::optional<bool> keeperPresent;
    std::optional<double> keeperSkill;
    std::optional<bool> windEnabled;
    std::optional<double> windSpeed;
    std::optional<double> windDirection;
};

SimSettings defaultSettings();
// Object.assign / spread: overwrite the fields the patch sets.
void applyPatch(SimSettings& s, const SettingsPatch& p);
// Clamp every field into its legal range.
SimSettings sanitizeSettings(const SimSettings& raw);
// Wind (m/s): direction 0 = towards goal (-z), 90 = towards +x. Zero when switched off.
V3 windVector(const SimSettings& s);

const char* str(Frequency f);
const char* str(KeeperType t);
bool parseFrequency(const char* s, Frequency& out);
bool parseKeeperType(const char* s, KeeperType& out);

}  // namespace sim
