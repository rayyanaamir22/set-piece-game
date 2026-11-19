// Loads the shared tuning data (data/*.json) into a SimConfig. Missing or mistyped keys throw, so the
// native build cannot silently drift from the TypeScript types.
#pragma once
#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "sim/Sim.h"

namespace sim {

SettingsPatch parseSettingsPatch(const nlohmann::json& j);
SimSettings parseSettings(const nlohmann::json& j);  // a full settings object (e.g. from a golden fixture)
std::shared_ptr<SimConfig> loadConfig(const std::string& dataDir);

}  // namespace sim
