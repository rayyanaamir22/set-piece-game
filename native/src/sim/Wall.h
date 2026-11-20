// Port of src/sim/Wall.ts.
#pragma once
#include <optional>
#include <vector>

#include "sim/BallPhysics.h"
#include "sim/ai.h"
#include "sim/math/rng.h"
#include "sim/settings.h"

namespace sim {

inline constexpr double WALL_PLAYER_RADIUS = 0.23;
inline constexpr double WALL_SPACING = 0.47;

struct WallJump {
    std::vector<double> start, height;
    double duration = 0;
};

struct LyingMan {
    V3 a, b;
    double radius = 0;
};

struct WallState {
    std::vector<V3> players;  // current feet positions (y = 0)
    std::vector<V3> base;
    std::vector<double> advance, shuffle;
    V3 axis;
    std::vector<int> encroachers;
    double height = 0, radius = 0;
    V3 facing;
    std::vector<double> lift;
    std::optional<WallJump> jump;
    std::vector<bool> earlyJumped;
    std::optional<LyingMan> lying;
};

struct WallRolls {
    bool lyingMan = false;
    std::optional<double> encroach;  // fraction 0..1 picking the encroaching player
};

inline WallRolls noRolls() { return {}; }

WallRolls rollWall(RngState& rng, const SimSettings& settings, const WallAi& ai);
int autoWallCount(const V3& spot);
// nullopt = no wall (disabled, or the spot is too close to goal).
std::optional<WallState> layoutWall(const V3& spot, const SimSettings& settings, const WallAi* ai = nullptr,
                                    const WallRolls& rolls = {});
void syncWallPositions(WallState& wall);
void updateWallRunup(WallState& wall, const WallAi& ai, double t);
bool wallEncroaching(const WallState& wall, const WallAi& ai);
void updateWallFlight(WallState& wall, const WallAi& ai, double dt);
bool wallShouldJump(const BallState& ball, const V3& spot, const PhysicsParams& P, const WallAi& ai);
void wallDummyJump(WallState& wall, const WallAi& ai, RngState& rng);
void startWallJump(WallState& wall, const WallAi& ai, RngState& rng, bool shouldJump = true, double sinceDummy = 0);
void updateWallLift(WallState& wall, double t);

}  // namespace sim
