#include "sim/Sim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "sim/constants.h"

namespace sim {

const char* str(Phase p) {
    switch (p) {
    case Phase::Aim: return "aim";
    case Phase::Charge: return "charge";
    case Phase::Runup: return "runup";
    case Phase::Flight: return "flight";
    case Phase::Result: return "result";
    }
    return "aim";
}
const char* str(Woodwork w) { return w == Woodwork::Post ? "post" : "bar"; }
const char* str(KeeperTouch t) { return t == KeeperTouch::Hold ? "hold" : "touch"; }
const char* str(Mode m) { return m == Mode::Free ? "free" : m == Mode::Practice ? "practice" : "challenge"; }

namespace {

void resetAttempt(SimState& s);
void layoutDefenders(SimState& s);

std::string f2(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}
std::string f3(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    return buf;
}

ShotRequest shotRequest(SimState& s, double power, bool withRng) {
    ShotRequest r;
    r.spot = s.spot;
    r.target = s.aim;
    r.contact = s.contact;
    r.power = power;
    r.driven = s.driven;
    r.low = s.low;
    r.taker = s.taker;
    r.taker.accuracy = clamp(s.taker.accuracy + s.accuracyBonus, 0, 1);
    r.runup = s.runup;
    // The solver never uses wind; it is only passed so the optional path aid shows the real drift.
    if (s.showPath) r.wind = windVector(s.settings);
    r.rng = withRng ? &s.rng : nullptr;
    return r;
}

void finish(SimState& s, ResultType type);

void classifyCrossing(SimState& s) {
    const double x = s.ball.pos.x, y = s.ball.pos.y;
    const bool inFrame = std::fabs(x) < GOAL_HALF_WIDTH && y < GOAL_HEIGHT;
    if (inFrame) {
        s.netActive = true;
        finish(s, ResultType::Goal);
    } else if (y >= GOAL_HEIGHT) {
        finish(s, ResultType::Over);
    } else {
        finish(s, ResultType::Wide);
    }
}

// Is a parried ball still live in the box (and slow enough to be played again)?
bool reboundAvailable(const SimState& s) {
    const KeeperAi& ai = s.keeperAi;
    const BallState& b = s.ball;
    const double speed = len(b.vel);
    return s.mode != Mode::Challenge && s.settings.rebounds && s.settings.keeperPresent && s.reboundCount < ai.maxRebounds &&
           std::fabs(b.pos.x) <= ai.reboundBoxX && b.pos.z >= ai.reboundZMin && b.pos.z <= ai.reboundZMax &&
           speed < ai.reboundMaxSpeed;
}

void finish(SimState& s, ResultType type) {
    s.phase = Phase::Result;
    s.phaseT = 0;
    if (s.encroached && type != ResultType::Goal) {
        // The wall was off the line: the attempt does not count; he retakes.
        type = ResultType::Retake;
        s.stats.attempts = std::max(0, s.stats.attempts - 1);
    } else if (s.keeperTouch && type != ResultType::Goal) {
        type = ResultType::Saved;
    }
    const bool rebound = type == ResultType::Saved && s.keeperTouch == KeeperTouch::Touch && reboundAvailable(s);
    double points = 0;
    int ring = -1;
    if (s.challenge && !s.challenge->done && type != ResultType::Retake) {
        ChallengeRun& ch = *s.challenge;
        const Scored sc = scoreShot(ch.def, s.crossing, type);
        points = sc.points;
        ring = sc.ring;
        ch.taken++;
        ch.points += points;
        ch.log.push_back({type, points, ring});
        if (ch.taken >= ch.def.shots) {
            ch.done = true;
            ch.medal = medalFor(ch.def, ch.points);
        }
    }
    AttemptResult r;
    r.type = type;
    r.woodwork = s.woodwork;
    r.wall = s.hitWall;
    r.keeper = s.keeperTouch;
    r.rebound = rebound;
    r.points = points;
    r.ring = ring;
    r.crossing = s.crossing;
    s.result = r;
    if (type == ResultType::Goal) {
        s.stats.goals++;
        SimEvent g;
        g.type = EventType::Goal;
        s.events.push_back(g);
    }
    SimEvent e;
    e.type = EventType::Result;
    e.result = type;
    s.events.push_back(e);
}

void startFollowUp(SimState& s) {
    const KeeperAi& ai = s.keeperAi;
    const double x = clamp(s.ball.pos.x, -ai.reboundBoxX, ai.reboundBoxX);
    const double z = clamp(s.ball.pos.z, 2.5, ai.reboundZMax);
    setSpot(s, x, z, true);
}

void onKeeperContact(SimState& s, const CollisionEvent& e) {
    if (s.keeperTouch || !s.keeper) return;
    const KeeperAi& ai = s.keeperAi;
    s.deflectT = s.flightT;
    const double holdMax = lerp(ai.holdSpeedMin, ai.holdSpeedMax, s.keeper->skill) * (s.lastShotKnuckle ? ai.knuckleHoldFactor : 1);
    if (e.hands && e.preSpeed.value_or(INF) < holdMax) {
        // Caught: the ball drops dead at his hands.
        s.keeperTouch = KeeperTouch::Hold;
        s.ball.vel = v3(0, -0.5, 0.3);
        s.ball.spin = v3();
        finish(s, ResultType::Saved);
    } else {
        s.keeperTouch = KeeperTouch::Touch;
    }
}

void endChallenge(SimState& s) {
    if (s.challenge) s.settings = s.challenge->backup;  // Object.assign(settings, backup)
    s.challenge.reset();
}

void newSpot(SimState& s) {
    const SpotXZ p = randomSpot(s);
    setSpot(s, p.x, p.z);
}

void layoutDefenders(SimState& s) {
    // No wall on a rebound: it is a scramble in the box.
    s.wall = s.followUp ? std::nullopt : layoutWall(s.spot, s.settings, &s.cfg->ai.wall, s.rolls);
    s.keeper = layoutKeeper(s.spot, s.settings, s.keeperAi);
}

void resetAttempt(SimState& s) {
    const double r = s.cfg->physics.radius;
    s.ball = newBall({s.spot.x, r, s.spot.z});
    s.prevBallPos = s.ball.pos;
    s.phase = Phase::Aim;
    s.phaseT = 0;
    s.meter = 0;
    s.pending.reset();
    s.result.reset();
    s.woodwork.reset();
    s.hitWall = false;
    s.keeperTouch.reset();
    s.deflectT.reset();
    s.encroached = false;
    s.dummy = false;
    s.dummyDone = false;
    s.crossing.reset();
    s.netActive = false;
    s.flightT = 0;
    s.keeperAi = effectiveKeeperAi(s.cfg->ai, s.settings.keeperType);
    s.rolls = rollWall(s.rng, s.settings, s.cfg->ai.wall);
    s.ball.wind = windVector(s.settings);
    layoutDefenders(s);
    s.previewKey.clear();
    updatePreview(s);
}

}  // namespace

SimState createSim(std::shared_ptr<const SimConfig> cfgp, int32_t seed) {
    const SimConfig& cfg = *cfgp;
    SimState s;
    s.cfg = cfgp;
    s.rng.s = seed;
    const Taker& taker = cfg.takers[0];
    s.taker = taker;
    s.foot = taker.foot;
    s.spot = v3(-6, 0, 24);
    s.ball = newBall({s.spot.x, cfg.physics.radius, s.spot.z});
    s.aim = {0, 1.2};
    s.contact = {0, 0};
    s.runup = {cfg.shot.runupAngleDefault, cfg.shot.runupDistDefault};
    s.runupDur = cfg.shot.runupTime;
    s.settings = defaultSettings();
    applyPatch(s.settings, cfg.settings);
    s.keeperAi = cfg.ai.keeper;
    s.rolls = noRolls();
    s.prev = emptyInput();
    s.prevBallPos = s.ball.pos;
    s.keeperAi = effectiveKeeperAi(cfg.ai, s.settings.keeperType);
    s.rolls = rollWall(s.rng, s.settings, cfg.ai.wall);
    s.ball.wind = windVector(s.settings);
    layoutDefenders(s);
    updatePreview(s);
    return s;
}

void setSpot(SimState& s, double x, double z, bool followUp) {
    s.followUp = followUp;
    s.reboundCount = followUp ? s.reboundCount + 1 : 0;
    s.spot = v3(x, 0, z);
    s.ball = newBall({x, s.cfg->physics.radius, z});
    s.prevBallPos = s.ball.pos;
    s.spotId++;
    resetAttempt(s);
}

void applySettings(SimState& s) {
    if (s.phase == Phase::Aim || s.phase == Phase::Charge) {
        s.keeperAi = effectiveKeeperAi(s.cfg->ai, s.settings.keeperType);
        s.rolls = rollWall(s.rng, s.settings, s.cfg->ai.wall);
        s.ball.wind = windVector(s.settings);
        layoutDefenders(s);
        s.previewKey.clear();
    }
}

void selectTaker(SimState& s, int index) {
    const int n = static_cast<int>(s.cfg->takers.size());
    s.takerIndex = ((index % n) + n) % n;
    s.taker = s.cfg->takers[s.takerIndex];
    s.foot = s.taker.foot;
    s.previewKey.clear();
}

void applyPreset(SimState& s, DifficultyName name) {
    const DifficultyPreset& p = s.cfg->presets[static_cast<size_t>(name)];
    SimSettings merged = s.settings;
    applyPatch(merged, p.settings);
    s.settings = sanitizeSettings(merged);
    s.accuracyBonus = p.accuracyBonus;
    s.difficulty = DIFFICULTY_NAMES[static_cast<size_t>(name)];
    applySettings(s);
}

void startChallenge(SimState& s, int index) {
    const auto& defs = s.cfg->challenges;
    const int n = static_cast<int>(defs.size());
    const int i = ((index % n) + n) % n;
    const ChallengeDef& def = defs[i];
    const SimSettings backup = s.challenge ? s.challenge->backup : s.settings;
    if (s.challenge) s.settings = backup;
    applyPatch(s.settings, def.settings);
    s.mode = Mode::Challenge;
    ChallengeRun run;
    run.index = i;
    run.def = def;
    run.backup = backup;
    s.challenge = std::move(run);
    setSpot(s, def.spot.x, def.spot.z);
}

void setMode(SimState& s, Mode mode, int challengeIndex) {
    if (mode == Mode::Challenge) {
        startChallenge(s, challengeIndex);
        return;
    }
    endChallenge(s);
    s.mode = mode;
    resetAttempt(s);
}

SpotXZ randomSpot(SimState& s) {
    for (int i = 0; i < 50; i++) {
        const double d = rngRange(s.rng, 19, 32);
        const double a = rngRange(s.rng, -0.75, 0.75);
        const double x = d * std::sin(a);
        const double z = d * std::cos(a);
        if (z > PENALTY_BOX_Z + 1.5 || std::fabs(x) > PENALTY_BOX_HALF + 1.5) return {x, z};
    }
    return {0, 25};
}

void updatePreview(SimState& s) {
    const double sweet = (s.cfg->shot.sweetMin + s.cfg->shot.sweetMax) / 2;
    std::string key;
    key.reserve(160);
    auto add = [&](const std::string& p) {
        key += p;
        key += '|';
    };
    add(f2(s.spot.x));
    add(f2(s.spot.z));
    add(f3(s.aim.x));
    add(f3(s.aim.y));
    add(f3(s.contact.x));
    add(f3(s.contact.y));
    add(s.driven ? "1" : "0");
    add(s.low ? "1" : "0");
    add(s.showPath ? "1" : "0");
    add(s.taker.name);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f", s.runup.angle);
    add(buf);
    add(f2(s.runup.dist));
    if (s.settings.windEnabled) {
        std::snprintf(buf, sizeof buf, "%g/%g", s.settings.windSpeed, s.settings.windDirection);
        add(buf);
    } else {
        add("nowind");
    }
    if (key == s.previewKey) return;
    s.previewKey = key;
    Shot shot = computeShot(shotRequest(s, sweet, false), s.cfg->physics, s.cfg->shot, s.showPath);
    ShotPreview p;
    p.azimuth = shot.azimuth;
    p.elevation = shot.elevation;
    p.knuckle = shot.knuckle;
    p.path = std::move(shot.path);
    s.preview = std::move(p);
}

void step(SimState& s, const InputState& input, double dt) {
    const ShotParams& SP = s.cfg->shot;
    const PhysicsParams& P = s.cfg->physics;
    auto edge = [](bool cur, bool prev) { return cur && !prev; };
    s.prevBallPos = s.ball.pos;
    s.phaseT += dt;

    const bool adjusting = s.phase == Phase::Aim || s.phase == Phase::Charge;
    if (adjusting) {
        s.aim.x = clamp(s.aim.x + input.aimX * SP.aimSpeedX * dt + input.aimDeltaX, -7, 7);
        s.aim.y = clamp(s.aim.y + input.aimY * SP.aimSpeedY * dt + input.aimDeltaY, 0.1, 4);
        double cx = input.contactX;
        double cy = input.contactY;
        const double m = jhypot(cx, cy);
        if (m > 1) {
            cx /= m;
            cy /= m;
        }
        const double k = jmin(1, dt * 14);
        s.contact.x += (cx - s.contact.x) * k;
        s.contact.y += (cy - s.contact.y) * k;
        s.driven = input.driven;
        s.low = input.low;
        s.runup.angle = clamp(s.runup.angle + input.runupAngle * SP.runupAngleRate * dt, 0, SP.runupAngleMax);
        s.runup.dist = clamp(s.runup.dist + input.runupDist * SP.runupDistRate * dt, SP.runupDistMin, SP.runupDistMax);
    }

    if (s.phase == Phase::Aim) {
        if (edge(input.toggleFoot, s.prev.toggleFoot)) s.foot = s.foot == 'R' ? 'L' : 'R';
        if (edge(input.newSpot, s.prev.newSpot) && s.mode != Mode::Challenge) newSpot(s);
        else if (edge(input.shoot, s.prev.shoot)) {
            s.phase = Phase::Charge;
            s.phaseT = 0;
            s.meter = 0;
        }
    } else if (s.phase == Phase::Charge) {
        s.meter = jmin(1, s.meter + dt / SP.chargeTime);
        if (!input.shoot) {
            s.pending = computeShot(shotRequest(s, s.meter, true), P, SP);
            s.phase = Phase::Runup;
            s.phaseT = 0;
            s.runupT = 0;
            const double base = SP.runupTime * (s.runup.dist / SP.runupDistDefault);
            s.dummy = input.dummy;
            s.dummyDone = false;
            s.dummyAt = base;
            s.runupDur = base + (s.dummy ? SP.dummyDelay : 0);
        }
    } else if (s.phase == Phase::Runup) {
        s.runupT += dt;
        if (s.wall) updateWallRunup(*s.wall, s.cfg->ai.wall, s.runupT);
        if (s.dummy && !s.dummyDone && s.runupT >= s.dummyAt) {
            s.dummyDone = true;
            if (s.wall && s.settings.wallJumps) wallDummyJump(*s.wall, s.cfg->ai.wall, s.rng);
        }
        if (s.wall && s.dummyDone) updateWallLift(*s.wall, s.runupT - s.dummyAt);
        if (s.runupT >= s.runupDur && s.pending) {
            const Shot shot = *s.pending;
            s.ball.vel = shot.vel;
            s.ball.spin = shot.spin;
            s.ball.knuckle.amp = shot.knuckle ? P.knuckleAccel : 0;
            s.ball.knuckle.phase = shot.knucklePhase;
            s.stats.attempts++;
            s.lastShotKnuckle = shot.knuckle;
            s.ball.wind = windVector(s.settings);
            s.encroached = s.wall && wallEncroaching(*s.wall, s.cfg->ai.wall);
            if (s.wall && s.settings.wallJumps) {
                startWallJump(*s.wall, s.cfg->ai.wall, s.rng, wallShouldJump(s.ball, s.spot, P, s.cfg->ai.wall),
                              s.dummy ? SP.dummyDelay : 0);
            }
            if (s.keeper) keeperOnStrike(*s.keeper, s.keeperAi, s.rng, s.dummy);
            SimEvent e;
            e.type = EventType::Strike;
            e.speed = shot.speed;
            s.events.push_back(e);
            s.phase = Phase::Flight;
            s.phaseT = 0;
            s.flightT = 0;
        }
    }

    if (s.phase == Phase::Flight || s.phase == Phase::Result) {
        s.flightT += dt;
        // Defenders move first, then the ball is stepped against their new positions.
        if (s.wall) {
            updateWallFlight(*s.wall, s.cfg->ai.wall, dt);
            updateWallLift(*s.wall, s.flightT);
        }
        if (s.keeper) keeperStep(*s.keeper, s.flightT, s.ball, P, s.keeperAi, s.rng);

        std::vector<CollisionEvent> ev;
        stepWorld(s.ball, P, dt, s.netActive, s.wall ? &*s.wall : nullptr, s.keeper ? &*s.keeper : nullptr, s.keeperAi, ev);
        if (!s.crossing && s.prevBallPos.z > 0 && s.ball.pos.z <= 0) {
            const double f = s.prevBallPos.z / (s.prevBallPos.z - s.ball.pos.z);
            s.crossing = XY{lerp(s.prevBallPos.x, s.ball.pos.x, f), lerp(s.prevBallPos.y, s.ball.pos.y, f)};
        }
        for (const CollisionEvent& e : ev) {
            if (e.type == CollisionType::Post) s.woodwork = Woodwork::Post;
            if (e.type == CollisionType::Bar) s.woodwork = Woodwork::Bar;
            if (e.type == CollisionType::Wall) {
                s.hitWall = true;
                if (!s.deflectT) s.deflectT = s.flightT;
            }
            if (e.type == CollisionType::Keeper && s.phase == Phase::Flight) onKeeperContact(s, e);
            SimEvent se;
            se.type = static_cast<EventType>(static_cast<int>(EventType::Post) + (static_cast<int>(e.type) - static_cast<int>(CollisionType::Post)));
            se.speed = e.speed;
            se.pos = e.pos;
            se.preSpeed = e.preSpeed;
            se.hands = e.hands;
            s.events.push_back(se);
        }
    }

    if (s.phase == Phase::Flight) {
        const BallState& b = s.ball;
        if (b.pos.z < -P.radius) classifyCrossing(s);
        else {
            const double speed = len(b.vel);
            const bool settled = b.pos.y <= P.radius + 0.01 && speed < 0.4;
            const bool gone = std::fabs(b.pos.x) > 60 || b.pos.z > 70;
            const double wait = s.keeperTouch ? s.keeperAi.afterTouchWait : s.cfg->ai.wall.afterHitWait;
            const bool deflectDone = s.deflectT && s.flightT - *s.deflectT > wait;
            if (settled || gone || deflectDone || s.flightT > 7) finish(s, ResultType::Miss);
        }
    } else if (s.phase == Phase::Result) {
        const bool timeUp = s.phaseT > SP.resultTime;
        if (s.challenge && s.challenge->done) {
            // The run is over: the summary stays up until you restart (R) or move on (N).
            const int idx = s.challenge->index;
            if (edge(input.retry, s.prev.retry)) startChallenge(s, idx);
            else if (edge(input.newSpot, s.prev.newSpot)) startChallenge(s, idx + 1);
        } else if (edge(input.retry, s.prev.retry)) resetAttempt(s);
        else if (edge(input.newSpot, s.prev.newSpot) && s.mode != Mode::Challenge) newSpot(s);
        else if (timeUp) {
            if ((s.result && s.result->type == ResultType::Retake) || s.mode == Mode::Challenge) resetAttempt(s);
            else if (s.result && s.result->rebound && reboundAvailable(s)) startFollowUp(s);
            else if (s.mode == Mode::Practice) resetAttempt(s);
            else newSpot(s);
        }
    }

    if (s.phase == Phase::Aim || s.phase == Phase::Charge) updatePreview(s);
    s.prev = input;
}

}  // namespace sim
