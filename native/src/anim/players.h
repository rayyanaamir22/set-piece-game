// Animation of every figure on the pitch, driven by the sim state (pure maths: no GL, unit-testable).
// The sim stays the authority: the keeper's body follows his hitbox capsule, the taker's striking foot is
// IK-aligned to the ball at the sim's launch tick, the wall follows the sim's lift and encroachment.
#pragma once
#include <optional>
#include <vector>

#include "anim/skeleton.h"
#include "sim/Sim.h"

namespace anim {

enum class Role { Taker, Decoy, Wall, Lying, Keeper };

struct Character {
    Role role = Role::Wall;
    int variant = 0;  // skin-tone / kit variant
    glm::mat4 model{1.f};
    Pose pose;
};

class TakerAnimator {
public:
    void update(const sim::SimState& s, double dt);
    Character character;
    // For tests / debugging (model space of `character`): the striking and planted ankles after IK.
    glm::vec3 kickAnkleWorld() const;
    glm::vec3 plantAnkleWorld() const;
    bool kickLeft = false;

private:
    enum class Kind { Play, Celebrate, Disappoint };
    glm::vec3 lastPos_{0.f};
    bool hasLast_ = false;
    float runPhase_ = 0, speed_ = 0;
    double sinceStrike_ = -1, resultT_ = 0;
    Kind kind_ = Kind::Play;
    Pose out_, fadeFrom_;
    float fadeT_ = 1.f;
};

class KeeperAnimator {
public:
    void update(const sim::SimState& s, double dt);
    std::optional<Character> character;
    // World-space fingertip positions (left = +x side of the character) for the "hands follow the hitbox" check.
    std::array<glm::vec3, 2> tips{};

private:
    float stepPhase_ = 0, touchT_ = 0, getUpT_ = 10;
    bool wasLying_ = false;
    float lastX_ = 0;
    bool hasLast_ = false;
    std::optional<sim::KeeperTouch> lastTouch_;
};

class WallAnimator {
public:
    void update(const sim::SimState& s, double dt);
    std::vector<Character> players;
    std::optional<Character> lying;

private:
    struct PerPlayer {
        glm::vec3 last{0.f};
        bool has = false;
        float phase = 0, fwdSpeed = 0;
    };
    std::vector<PerPlayer> st_;
};

class DecoyAnimator {
public:
    void update(const sim::SimState& s, double dt);
    std::optional<Character> character;

private:
    float phase_ = 0;
    glm::vec3 last_{0.f};
    bool has_ = false;
};

class PlayerAnimators {
public:
    void update(const sim::SimState& s, double dt) {
        taker.update(s, dt);
        decoy.update(s, dt);
        wall.update(s, dt);
        keeper.update(s, dt);
    }
    TakerAnimator taker;
    DecoyAnimator decoy;
    WallAnimator wall;
    KeeperAnimator keeper;
};

}  // namespace anim
