// Port of src/sim/Sim.ts: the state machine and `step`.
// Names, state fields, order of RNG draws and the order of operations in `step` match the TypeScript.
#pragma once
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sim/BallPhysics.h"
#include "sim/Collisions.h"
#include "sim/Keeper.h"
#include "sim/ShotModel.h"
#include "sim/Wall.h"
#include "sim/ai.h"
#include "sim/challenge.h"
#include "sim/input_state.h"
#include "sim/math/rng.h"
#include "sim/presets.h"
#include "sim/settings.h"

namespace sim {

enum class Phase { Aim, Charge, Runup, Flight, Result };
enum class Woodwork { Post, Bar };
enum class KeeperTouch { Hold, Touch };
enum class Mode { Free, Practice, Challenge };

const char* str(Phase p);
const char* str(Woodwork w);
const char* str(KeeperTouch t);
const char* str(Mode m);

enum class EventType { Strike, Goal, Result, Post, Bar, Net, Bounce, Wall, Keeper };

struct SimEvent {
    EventType type = EventType::Goal;
    double speed = 0;                 // strike, collisions
    ResultType result = ResultType::Miss;  // `result` events
    V3 pos;                           // collisions
    std::optional<double> preSpeed;   // keeper contacts
    bool hands = false;
};

struct SimConfig {
    SettingsPatch settings;  // Partial<SimSettings>
    PhysicsParams physics;
    ShotParams shot;
    AiParams ai;
    std::vector<Taker> takers;
    std::vector<ChallengeDef> challenges;
    Presets presets;
};

struct ShotPreview {
    double azimuth = 0, elevation = 0;
    bool knuckle = false;
    std::optional<std::vector<V3>> path;
};

struct AttemptResult {
    ResultType type = ResultType::Miss;
    std::optional<Woodwork> woodwork;
    bool wall = false;
    std::optional<KeeperTouch> keeper;
    bool rebound = false;  // the ball stayed in the box: a follow-up shot comes next
    double points = 0;
    int ring = -1;
    std::optional<XY> crossing;
};

struct Stats {
    int attempts = 0, goals = 0;
};

struct SimState {
    std::shared_ptr<const SimConfig> cfg;
    RngState rng;
    Phase phase = Phase::Aim;
    double phaseT = 0;
    Taker taker;
    char foot = 'R';

    V3 spot;
    int spotId = 0;  // incremented whenever the ball is moved to a new spot
    BallState ball;
    V3 prevBallPos;

    XY aim, contact;
    double meter = 0;
    bool driven = false, low = false;

    std::optional<Shot> pending;
    double runupT = 0;
    std::optional<ShotPreview> preview;
    std::string previewKey;
    bool showPath = false;

    RunupSetting runup{0, 0};
    double runupDur = 0;

    Mode mode = Mode::Free;
    std::optional<ChallengeRun> challenge;
    int takerIndex = 0;
    std::string difficulty = "normal";  // a difficulty name or "custom"
    double accuracyBonus = 0;
    bool dummy = false;
    double dummyAt = 0;
    bool dummyDone = false;
    std::optional<XY> crossing;  // where the ball crossed the goal plane

    SimSettings settings;
    KeeperAi keeperAi;
    WallRolls rolls;
    bool encroached = false;
    bool followUp = false;
    int reboundCount = 0;
    std::optional<WallState> wall;
    std::optional<KeeperState> keeper;
    std::optional<AttemptResult> result;
    std::optional<Woodwork> woodwork;
    bool hitWall = false;
    std::optional<KeeperTouch> keeperTouch;
    std::optional<double> deflectT;  // flightT of the first deflection (wall or keeper)
    bool lastShotKnuckle = false;
    bool netActive = false;
    double flightT = 0;

    Stats stats;
    InputState prev;
    std::vector<SimEvent> events;
};

SimState createSim(std::shared_ptr<const SimConfig> cfg, int32_t seed = 1234);
void setSpot(SimState& s, double x, double z, bool followUp = false);
void applySettings(SimState& s);
void selectTaker(SimState& s, int index);
void applyPreset(SimState& s, DifficultyName name);
void startChallenge(SimState& s, int index);
void setMode(SimState& s, Mode mode, int challengeIndex = 0);
SpotXZ randomSpot(SimState& s);
void updatePreview(SimState& s);
void step(SimState& s, const InputState& input, double dt);

}  // namespace sim
