import { BallState, PhysicsParams, newBall } from './BallPhysics';
import { CollisionEvent, stepWorld } from './Collisions';
import {
  GOAL_HALF_WIDTH,
  GOAL_HEIGHT,
  PENALTY_BOX_HALF,
  PENALTY_BOX_Z,
} from './constants';
import { RngState, rngRange } from './math/rng';
import { V3, clamp, copy, lerp, v3 } from './math/vec3';
import { Shot, ShotParams, Taker, computeShot } from './ShotModel';
import { AiParams, KeeperAi, effectiveKeeperAi } from './ai';
import { ChallengeDef, ChallengeRun, medalFor, scoreShot } from './challenge';
import { DifficultyName, DifficultyPreset } from './presets';
import { KeeperState, keeperOnStrike, keeperStep, layoutKeeper } from './Keeper';
import { SimSettings, defaultSettings, sanitizeSettings, windVector } from './settings';
import {
  WallRolls,
  WallState,
  layoutWall,
  noRolls,
  rollWall,
  startWallJump,
  updateWallFlight,
  updateWallLift,
  updateWallRunup,
  wallDummyJump,
  wallEncroaching,
  wallShouldJump,
} from './Wall';

export type Phase = 'aim' | 'charge' | 'runup' | 'flight' | 'result';
export type ResultType = 'goal' | 'wide' | 'over' | 'miss' | 'saved' | 'retake';
export type KeeperTouch = 'hold' | 'touch';
export type Mode = 'free' | 'practice' | 'challenge';

export interface InputState {
  /** Reticle velocity axes, -1..1 (+x right, +y up). */
  aimX: number;
  aimY: number;
  /** Absolute contact point on the ball, unit disc. */
  contactX: number;
  contactY: number;
  shoot: boolean;
  driven: boolean;
  low: boolean;
  toggleFoot: boolean;
  retry: boolean;
  newSpot: boolean;
  /** Run-up arc angle / distance axes, -1..1 (wider/longer = +1). */
  runupAngle: number;
  runupDist: number;
  /** Mouse: reticle movement in metres this frame (applied once, not per sim step). */
  aimDeltaX: number;
  aimDeltaY: number;
  /** Hold at the release to send a dummy runner over the ball first. */
  dummy: boolean;
}

export const emptyInput = (): InputState => ({
  aimX: 0,
  aimY: 0,
  contactX: 0,
  contactY: 0,
  shoot: false,
  driven: false,
  low: false,
  toggleFoot: false,
  retry: false,
  newSpot: false,
  runupAngle: 0,
  runupDist: 0,
  aimDeltaX: 0,
  aimDeltaY: 0,
  dummy: false,
});

export type SimEvent =
  | { type: 'strike'; speed: number }
  | { type: 'goal' }
  | { type: 'result'; result: ResultType }
  | CollisionEvent;

export interface SimConfig {
  settings?: Partial<SimSettings>;
  physics: PhysicsParams;
  shot: ShotParams;
  ai: AiParams;
  takers: Taker[];
  challenges: ChallengeDef[];
  presets: Record<DifficultyName, DifficultyPreset>;
}

export interface ShotPreview {
  azimuth: number;
  elevation: number;
  knuckle: boolean;
  path: V3[] | null;
}

export interface SimState {
  cfg: SimConfig;
  rng: RngState;
  phase: Phase;
  phaseT: number;
  taker: Taker;
  foot: 'R' | 'L';

  spot: V3;
  /** Incremented whenever the ball is moved to a new spot (camera snaps). */
  spotId: number;
  ball: BallState;
  prevBallPos: V3;

  aim: { x: number; y: number };
  contact: { x: number; y: number };
  meter: number;
  driven: boolean;
  low: boolean;

  pending: Shot | null;
  runupT: number;
  preview: ShotPreview | null;
  previewKey: string;
  showPath: boolean;

  /** Run-up arc: degrees off the straight line behind the ball, and distance (m). */
  runup: { angle: number; dist: number };
  /** Duration of the current run-up animation/clock (scales with distance). */
  runupDur: number;

  /** Free kicks (random spots), practice (same spot, instant retries) or a scored skill challenge. */
  mode: Mode;
  challenge: ChallengeRun | null;
  takerIndex: number;
  difficulty: DifficultyName | 'custom';
  /** Added to the taker's accuracy (difficulty preset). */
  accuracyBonus: number;
  /** A dummy runner goes over the ball first in this attempt. */
  dummy: boolean;
  dummyAt: number;
  dummyDone: boolean;
  /** Where the ball crossed the goal plane (z = 0) in this attempt, or null. */
  crossing: { x: number; y: number } | null;

  settings: SimSettings;
  /** Keeper tuning in force: base values + the selected keeper type. */
  keeperAi: KeeperAi;
  /** Per-attempt random wall decisions (lying man, encroachment). */
  rolls: WallRolls;
  /** The wall was past the 9.15 m line at the strike (a retake unless it scores). */
  encroached: boolean;
  /** This attempt is a rebound follow-up after a parry. */
  followUp: boolean;
  reboundCount: number;
  wall: WallState | null;
  keeper: KeeperState | null;
  result: {
    type: ResultType;
    woodwork: 'post' | 'bar' | null;
    wall: boolean;
    keeper: KeeperTouch | null;
    /** The ball stayed in the box: a follow-up shot comes next. */
    rebound: boolean;
    /** Challenge mode: points scored by this attempt and the ring crossed (-1 = none). */
    points: number;
    ring: number;
    crossing: { x: number; y: number } | null;
  } | null;
  woodwork: 'post' | 'bar' | null;
  hitWall: boolean;
  /** How the keeper first got to the ball: caught it ('hold') or got a hand/body to it ('touch'). */
  keeperTouch: KeeperTouch | null;
  /** flightT of the first deflection (wall or keeper); used to end the attempt after a wait. */
  deflectT: number | null;
  lastShotKnuckle: boolean;
  netActive: boolean;
  flightT: number;

  stats: { attempts: number; goals: number };
  prev: InputState;
  events: SimEvent[];
}

export function createSim(cfg: SimConfig, seed = 1234): SimState {
  const taker = cfg.takers[0];
  const spot = v3(-6, 0, 24);
  const s: SimState = {
    cfg,
    rng: { s: seed },
    phase: 'aim',
    phaseT: 0,
    taker,
    foot: taker.foot,
    spot,
    spotId: 0,
    ball: newBall({ x: spot.x, y: cfg.physics.radius, z: spot.z }),
    prevBallPos: v3(),
    aim: { x: 0, y: 1.2 },
    contact: { x: 0, y: 0 },
    meter: 0,
    driven: false,
    low: false,
    pending: null,
    runupT: 0,
    preview: null,
    previewKey: '',
    showPath: false,
    mode: 'free',
    challenge: null,
    takerIndex: 0,
    difficulty: 'normal',
    accuracyBonus: 0,
    dummy: false,
    dummyAt: 0,
    dummyDone: false,
    crossing: null,
    runup: { angle: cfg.shot.runupAngleDefault, dist: cfg.shot.runupDistDefault },
    runupDur: cfg.shot.runupTime,
    settings: { ...defaultSettings(), ...cfg.settings },
    keeperAi: cfg.ai.keeper,
    rolls: noRolls(),
    encroached: false,
    followUp: false,
    reboundCount: 0,
    wall: null,
    keeper: null,
    result: null,
    woodwork: null,
    hitWall: false,
    keeperTouch: null,
    deflectT: null,
    lastShotKnuckle: false,
    netActive: false,
    flightT: 0,
    stats: { attempts: 0, goals: 0 },
    prev: emptyInput(),
    events: [],
  };
  s.prevBallPos = copy(s.ball.pos);
  s.keeperAi = effectiveKeeperAi(cfg.ai, s.settings.keeperType);
  s.rolls = rollWall(s.rng, s.settings, cfg.ai.wall);
  s.ball.wind = windVector(s.settings);
  layoutDefenders(s);
  updatePreview(s);
  return s;
}

/** Put the ball on a new spot and return to the aiming phase. */
export function setSpot(s: SimState, x: number, z: number, followUp = false): void {
  s.followUp = followUp;
  s.reboundCount = followUp ? s.reboundCount + 1 : 0;
  s.spot = v3(x, 0, z);
  s.ball = newBall({ x, y: s.cfg.physics.radius, z });
  s.prevBallPos = copy(s.ball.pos);
  s.spotId++;
  resetAttempt(s);
}

function resetAttempt(s: SimState): void {
  const r = s.cfg.physics.radius;
  s.ball = newBall({ x: s.spot.x, y: r, z: s.spot.z });
  s.prevBallPos = copy(s.ball.pos);
  s.phase = 'aim';
  s.phaseT = 0;
  s.meter = 0;
  s.pending = null;
  s.result = null;
  s.woodwork = null;
  s.hitWall = false;
  s.keeperTouch = null;
  s.deflectT = null;
  s.encroached = false;
  s.dummy = false;
  s.dummyDone = false;
  s.crossing = null;
  s.netActive = false;
  s.flightT = 0;
  s.keeperAi = effectiveKeeperAi(s.cfg.ai, s.settings.keeperType);
  s.rolls = rollWall(s.rng, s.settings, s.cfg.ai.wall);
  s.ball.wind = windVector(s.settings);
  layoutDefenders(s);
  s.previewKey = '';
  updatePreview(s);
}

function layoutDefenders(s: SimState): void {
  // No wall on a rebound: it is a scramble in the box.
  s.wall = s.followUp ? null : layoutWall(s.spot, s.settings, s.cfg.ai.wall, s.rolls);
  s.keeper = layoutKeeper(s.spot, s.settings, s.keeperAi);
}

/** Apply changed settings. Defenders are re-laid immediately while the player is still lining up. */
export function applySettings(s: SimState): void {
  if (s.phase === 'aim' || s.phase === 'charge') {
    s.keeperAi = effectiveKeeperAi(s.cfg.ai, s.settings.keeperType);
    s.rolls = rollWall(s.rng, s.settings, s.cfg.ai.wall);
    s.ball.wind = windVector(s.settings);
    layoutDefenders(s);
    s.previewKey = '';
  }
}

/** Pick a taker from the roster; their preferred foot comes with them. */
export function selectTaker(s: SimState, index: number): void {
  const n = s.cfg.takers.length;
  s.takerIndex = ((index % n) + n) % n;
  s.taker = s.cfg.takers[s.takerIndex];
  s.foot = s.taker.foot;
  s.previewKey = '';
}

/** Apply a difficulty preset (keeper, wall, wind, accuracy). */
export function applyPreset(s: SimState, name: DifficultyName): void {
  const p = s.cfg.presets[name];
  Object.assign(s.settings, sanitizeSettings({ ...s.settings, ...p.settings }));
  s.accuracyBonus = p.accuracyBonus;
  s.difficulty = name;
  applySettings(s);
}

function endChallenge(s: SimState): void {
  if (s.challenge) Object.assign(s.settings, s.challenge.backup);
  s.challenge = null;
}

/** Start (or restart) a challenge: its rules override the settings until you leave the mode. */
export function startChallenge(s: SimState, index: number): void {
  const defs = s.cfg.challenges;
  const i = ((index % defs.length) + defs.length) % defs.length;
  const def = defs[i];
  const backup = s.challenge ? s.challenge.backup : { ...s.settings };
  if (s.challenge) Object.assign(s.settings, backup);
  Object.assign(s.settings, def.settings);
  s.mode = 'challenge';
  s.challenge = { index: i, def, taken: 0, points: 0, log: [], done: false, medal: null, backup };
  setSpot(s, def.spot.x, def.spot.z);
}

export function setMode(s: SimState, mode: Mode, challengeIndex = 0): void {
  if (mode === 'challenge') {
    startChallenge(s, challengeIndex);
    return;
  }
  endChallenge(s);
  s.mode = mode;
  resetAttempt(s);
}

export function randomSpot(s: SimState): { x: number; z: number } {
  for (let i = 0; i < 50; i++) {
    const d = rngRange(s.rng, 19, 32);
    const a = rngRange(s.rng, -0.75, 0.75);
    const x = d * Math.sin(a);
    const z = d * Math.cos(a);
    // Free kicks here are always outside the box.
    if (z > PENALTY_BOX_Z + 1.5 || Math.abs(x) > PENALTY_BOX_HALF + 1.5) return { x, z };
  }
  return { x: 0, z: 25 };
}

function newSpot(s: SimState): void {
  const p = randomSpot(s);
  setSpot(s, p.x, p.z);
}

function shotRequest(s: SimState, power: number, withRng: boolean) {
  return {
    spot: s.spot,
    target: s.aim,
    contact: s.contact,
    power,
    driven: s.driven,
    low: s.low,
    taker: { ...s.taker, accuracy: clamp(s.taker.accuracy + s.accuracyBonus, 0, 1) },
    runup: s.runup,
    // The solver never uses wind; it is only passed so the optional path aid shows the real drift.
    wind: s.showPath ? windVector(s.settings) : undefined,
    rng: withRng ? s.rng : null,
  };
}

/** Recompute the aim-arrow preview (ideal power, no error) when its inputs change. */
export function updatePreview(s: SimState): void {
  const sweet = (s.cfg.shot.sweetMin + s.cfg.shot.sweetMax) / 2;
  const key = [
    s.spot.x.toFixed(2),
    s.spot.z.toFixed(2),
    s.aim.x.toFixed(3),
    s.aim.y.toFixed(3),
    s.contact.x.toFixed(3),
    s.contact.y.toFixed(3),
    s.driven,
    s.low,
    s.showPath,
    s.taker.name,
    s.runup.angle.toFixed(1),
    s.runup.dist.toFixed(2),
    s.settings.windEnabled ? `${s.settings.windSpeed}/${s.settings.windDirection}` : 'nowind',
  ].join('|');
  if (key === s.previewKey) return;
  s.previewKey = key;
  const shot = computeShot(shotRequest(s, sweet, false), s.cfg.physics, s.cfg.shot, s.showPath);
  s.preview = {
    azimuth: shot.azimuth,
    elevation: shot.elevation,
    knuckle: shot.knuckle,
    path: shot.path ?? null,
  };
}

function classifyCrossing(s: SimState): void {
  const { x, y } = s.ball.pos;
  const inFrame = Math.abs(x) < GOAL_HALF_WIDTH && y < GOAL_HEIGHT;
  if (inFrame) {
    s.netActive = true;
    finish(s, 'goal');
  } else if (y >= GOAL_HEIGHT) {
    finish(s, 'over');
  } else {
    finish(s, 'wide');
  }
}

/** Is a parried ball still live in the box (and slow enough to be played again)? */
function reboundAvailable(s: SimState): boolean {
  const ai = s.keeperAi;
  const b = s.ball;
  const speed = Math.hypot(b.vel.x, b.vel.y, b.vel.z);
  return (
    s.mode !== 'challenge' &&
    s.settings.rebounds &&
    s.settings.keeperPresent &&
    s.reboundCount < ai.maxRebounds &&
    Math.abs(b.pos.x) <= ai.reboundBoxX &&
    b.pos.z >= ai.reboundZMin &&
    b.pos.z <= ai.reboundZMax &&
    speed < ai.reboundMaxSpeed
  );
}

function finish(s: SimState, type: ResultType): void {
  s.phase = 'result';
  s.phaseT = 0;
  if (s.encroached && type !== 'goal') {
    // The wall was off the line: no penalty for the taker. The attempt does not count; he retakes.
    type = 'retake';
    s.stats.attempts = Math.max(0, s.stats.attempts - 1);
  } else if (s.keeperTouch && type !== 'goal') {
    // Anything the keeper got a touch to and that did not go in counts as a save
    // (tipped over, pushed wide, held…).
    type = 'saved';
  }
  const rebound = type === 'saved' && s.keeperTouch === 'touch' && reboundAvailable(s);
  let points = 0;
  let ring = -1;
  const ch = s.challenge;
  if (ch && !ch.done && type !== 'retake') {
    ({ points, ring } = scoreShot(ch.def, s.crossing, type));
    ch.taken++;
    ch.points += points;
    ch.log.push({ type, points, ring });
    if (ch.taken >= ch.def.shots) {
      ch.done = true;
      ch.medal = medalFor(ch.def, ch.points);
    }
  }
  s.result = { type, woodwork: s.woodwork, wall: s.hitWall, keeper: s.keeperTouch, rebound, points, ring, crossing: s.crossing };
  if (type === 'goal') {
    s.stats.goals++;
    s.events.push({ type: 'goal' });
  }
  s.events.push({ type: 'result', result: type });
}

/** After a parry that stayed in the box: take the loose ball where it lies. */
function startFollowUp(s: SimState): void {
  const ai = s.keeperAi;
  const x = clamp(s.ball.pos.x, -ai.reboundBoxX, ai.reboundBoxX);
  const z = clamp(s.ball.pos.z, 2.5, ai.reboundZMax);
  setSpot(s, x, z, true);
}

function onKeeperContact(s: SimState, e: CollisionEvent): void {
  if (s.keeperTouch || !s.keeper) return;
  const ai = s.keeperAi;
  s.deflectT = s.flightT;
  const holdMax =
    lerp(ai.holdSpeedMin, ai.holdSpeedMax, s.keeper.skill) * (s.lastShotKnuckle ? ai.knuckleHoldFactor : 1);
  if (e.hands && (e.preSpeed ?? Infinity) < holdMax) {
    // Caught: the ball drops dead at his hands.
    s.keeperTouch = 'hold';
    s.ball.vel = v3(0, -0.5, 0.3);
    s.ball.spin = v3();
    finish(s, 'saved');
  } else {
    s.keeperTouch = 'touch';
  }
}

export function step(s: SimState, input: InputState, dt: number): void {
  const SP = s.cfg.shot;
  const P = s.cfg.physics;
  const edge = (cur: boolean, prev: boolean) => cur && !prev;
  s.prevBallPos = copy(s.ball.pos);
  s.phaseT += dt;

  const adjusting = s.phase === 'aim' || s.phase === 'charge';
  if (adjusting) {
    s.aim.x = clamp(s.aim.x + input.aimX * SP.aimSpeedX * dt + input.aimDeltaX, -7, 7);
    s.aim.y = clamp(s.aim.y + input.aimY * SP.aimSpeedY * dt + input.aimDeltaY, 0.1, 4);
    let cx = input.contactX;
    let cy = input.contactY;
    const m = Math.hypot(cx, cy);
    if (m > 1) {
      cx /= m;
      cy /= m;
    }
    const k = Math.min(1, dt * 14);
    s.contact.x += (cx - s.contact.x) * k;
    s.contact.y += (cy - s.contact.y) * k;
    s.driven = input.driven;
    s.low = input.low;
    s.runup.angle = clamp(s.runup.angle + input.runupAngle * SP.runupAngleRate * dt, 0, SP.runupAngleMax);
    s.runup.dist = clamp(s.runup.dist + input.runupDist * SP.runupDistRate * dt, SP.runupDistMin, SP.runupDistMax);
  }

  if (s.phase === 'aim') {
    if (edge(input.toggleFoot, s.prev.toggleFoot)) s.foot = s.foot === 'R' ? 'L' : 'R';
    if (edge(input.newSpot, s.prev.newSpot) && s.mode !== 'challenge') newSpot(s);
    else if (edge(input.shoot, s.prev.shoot)) {
      s.phase = 'charge';
      s.phaseT = 0;
      s.meter = 0;
    }
  } else if (s.phase === 'charge') {
    s.meter = Math.min(1, s.meter + dt / SP.chargeTime);
    if (!input.shoot) {
      s.pending = computeShot(shotRequest(s, s.meter, true), P, SP);
      s.phase = 'runup';
      s.phaseT = 0;
      s.runupT = 0;
      // A longer run-up takes longer; a dummy runner adds a beat before the real strike.
      const base = SP.runupTime * (s.runup.dist / SP.runupDistDefault);
      s.dummy = input.dummy;
      s.dummyDone = false;
      s.dummyAt = base;
      s.runupDur = base + (s.dummy ? SP.dummyDelay : 0);
    }
  } else if (s.phase === 'runup') {
    s.runupT += dt;
    if (s.wall) updateWallRunup(s.wall, s.cfg.ai.wall, s.runupT);
    if (s.dummy && !s.dummyDone && s.runupT >= s.dummyAt) {
      // The decoy steps over the ball: part of the wall jumps for it, far too early.
      s.dummyDone = true;
      if (s.wall && s.settings.wallJumps) wallDummyJump(s.wall, s.cfg.ai.wall, s.rng);
    }
    if (s.wall && s.dummyDone) updateWallLift(s.wall, s.runupT - s.dummyAt);
    if (s.runupT >= s.runupDur && s.pending) {
      const shot = s.pending;
      s.ball.vel = copy(shot.vel);
      s.ball.spin = copy(shot.spin);
      s.ball.knuckle = { amp: shot.knuckle ? P.knuckleAccel : 0, phase: shot.knucklePhase };
      s.stats.attempts++;
      s.lastShotKnuckle = shot.knuckle;
      s.ball.wind = windVector(s.settings);
      s.encroached = !!s.wall && wallEncroaching(s.wall, s.cfg.ai.wall);
      if (s.wall && s.settings.wallJumps) {
        startWallJump(s.wall, s.cfg.ai.wall, s.rng, wallShouldJump(s.ball, s.spot, P, s.cfg.ai.wall), s.dummy ? SP.dummyDelay : 0);
      }
      if (s.keeper) keeperOnStrike(s.keeper, s.keeperAi, s.rng, s.dummy);
      s.events.push({ type: 'strike', speed: shot.speed });
      s.phase = 'flight';
      s.phaseT = 0;
      s.flightT = 0;
    }
  }

  if (s.phase === 'flight' || s.phase === 'result') {
    s.flightT += dt;
    // Defenders move first, then the ball is stepped against their new positions.
    if (s.wall) {
      updateWallFlight(s.wall, s.cfg.ai.wall, dt);
      updateWallLift(s.wall, s.flightT);
    }
    if (s.keeper) keeperStep(s.keeper, s.flightT, s.ball, P, s.keeperAi, s.rng);

    const ev: CollisionEvent[] = [];
    stepWorld(s.ball, P, dt, s.netActive, s.wall, s.keeper, s.keeperAi, ev);
    if (s.crossing === null && s.prevBallPos.z > 0 && s.ball.pos.z <= 0) {
      const f = s.prevBallPos.z / (s.prevBallPos.z - s.ball.pos.z);
      s.crossing = { x: lerp(s.prevBallPos.x, s.ball.pos.x, f), y: lerp(s.prevBallPos.y, s.ball.pos.y, f) };
    }
    for (const e of ev) {
      if (e.type === 'post' || e.type === 'bar') s.woodwork = e.type;
      if (e.type === 'wall') {
        s.hitWall = true;
        if (s.deflectT === null) s.deflectT = s.flightT;
      }
      if (e.type === 'keeper' && s.phase === 'flight') onKeeperContact(s, e);
      s.events.push(e);
    }
  }

  if (s.phase === 'flight') {
    const b = s.ball;
    if (b.pos.z < -P.radius) classifyCrossing(s);
    else {
      const speed = Math.hypot(b.vel.x, b.vel.y, b.vel.z);
      const settled = b.pos.y <= P.radius + 0.01 && speed < 0.4;
      const gone = Math.abs(b.pos.x) > 60 || b.pos.z > 70;
      const wait = s.keeperTouch ? s.keeperAi.afterTouchWait : s.cfg.ai.wall.afterHitWait;
      const deflectDone = s.deflectT !== null && s.flightT - s.deflectT > wait;
      if (settled || gone || deflectDone || s.flightT > 7) finish(s, 'miss');
    }
  } else if (s.phase === 'result') {
    const timeUp = s.phaseT > SP.resultTime;
    const ch = s.challenge;
    if (ch && ch.done) {
      // The run is over: the summary stays up until you restart (R) or move on (N).
      if (edge(input.retry, s.prev.retry)) startChallenge(s, ch.index);
      else if (edge(input.newSpot, s.prev.newSpot)) startChallenge(s, ch.index + 1);
    } else if (edge(input.retry, s.prev.retry)) resetAttempt(s);
    else if (edge(input.newSpot, s.prev.newSpot) && s.mode !== 'challenge') newSpot(s);
    else if (timeUp) {
      if (s.result?.type === 'retake' || s.mode === 'challenge') resetAttempt(s); // same spot
      else if (s.result?.rebound && reboundAvailable(s)) startFollowUp(s);
      else if (s.mode === 'practice') resetAttempt(s);
      else newSpot(s);
    }
  }

  if (s.phase === 'aim' || s.phase === 'charge') updatePreview(s);
  s.prev = { ...input };
}
