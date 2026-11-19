// C++ counterpart of tests/helpers.ts for the ported unit tests.
#pragma once
#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>


#include "golden_driver.h"
#include "sim/Sim.h"

#define CLOSE(x, v, digits) CHECK(std::fabs((x) - (v)) < 0.5 * std::pow(10.0, -(digits)))

// doctest prints operands through operator<<; give the sim enums one.
namespace sim {
inline std::ostream& operator<<(std::ostream& o, ResultType v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, Phase v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, KeeperTouch v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, Woodwork v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, Mode v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, Medal v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, Frequency v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, KeeperType v) { return o << str(v); }
inline std::ostream& operator<<(std::ostream& o, KeeperPhase v) { return o << static_cast<int>(v); }
}  // namespace sim

namespace testkit {

using namespace sim;

constexpr double DTs = 1.0 / 240;
inline const PhysicsParams& P() { return repoConfig()->physics; }
inline const ShotParams& SP() { return repoConfig()->shot; }
inline const AiParams& AI() { return repoConfig()->ai; }
inline double sweet() { return (SP().sweetMin + SP().sweetMax) / 2; }

using PatchFn = std::function<void(SettingsPatch&)>;
using WallMod = std::function<void(WallAi&)>;
using KeeperMod = std::function<void(KeeperAi&)>;

// Defenders are off unless a test asks for them, so each test isolates one thing (tests/helpers.ts makeCfg).
std::shared_ptr<SimConfig> makeCfg(const PatchFn& over = nullptr, const WallMod& wall = nullptr, const KeeperMod& keeper = nullptr);

struct Fly {
    BallState ball;
    std::vector<V3> pts;
    double t = 0;
};
// Free flight (no ground/posts) until z <= 0.
Fly fly(const V3& start, const V3& vel, const V3& spin, const PhysicsParams* params = nullptr, const Knuckle* knuckle = nullptr,
        const V3* wind = nullptr);

// Hold shoot until the meter reaches `power`, release, and play until the attempt has a result.
const std::optional<AttemptResult>& takeShot(SimState& s, double power, const InputState& extra = emptyInput());

InputState withContact(double cx, double cy = 0);
void stepIdle(SimState& s, int n);

}  // namespace testkit
