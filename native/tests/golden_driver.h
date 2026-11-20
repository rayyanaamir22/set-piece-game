// C++ port of tests/goldenRunner.ts: replays a golden fixture's scenario through sim::step and
// produces the same JSON shape, plus the tolerance-aware comparison from docs/porting-contract.md.
#pragma once
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "sim/Sim.h"

namespace testkit {

constexpr int SAMPLE_EVERY = 24;  // steps (= 10 Hz at 240 Hz)
constexpr double DT = 1.0 / 240;

// Replay `fixture["scenario"]` (with `fixture["settings"]`) and return {scenario, settings, inputs, samples, final}.
nlohmann::json runScenario(const nlohmann::json& fixture, std::shared_ptr<const sim::SimConfig> cfg);

// First difference between `got` and `want`, or nullopt. Integers, strings, booleans and the RNG state must match
// exactly; positions within 1 cm and velocities/spins within 1e-3 relative (the porting-contract tolerances).
std::optional<std::string> firstDiff(const nlohmann::json& got, const nlohmann::json& want);

// A SimConfig loaded from the repository's data/ directory (cached).
std::shared_ptr<const sim::SimConfig> repoConfig();

std::string repoRoot();

}  // namespace testkit
