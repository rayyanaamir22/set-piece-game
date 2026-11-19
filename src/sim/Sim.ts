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
import { KeeperState, keeperOnStrike, keeperStep, layoutKeeper } from './Keeper';
import { SimSettings, defaultSettings, windVector } from './settings';
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
  wallEncroaching,
  wallShouldJump,
} from './Wall';

export type Phase = 'aim' | 'charge' | 'runup' | 'flight' | 'result';
export type ResultType = 'goal' | 'wide' | 'over' | 'miss' | 'saved' | 'retake';
export type KeeperTouch = 'hold' | 'touch';

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
    taker: s.taker,
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
  s.result = { type, woodwork: s.woodwork, wall: s.hitWall, keeper: s.keeperTouch, rebound };
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
    s.aim.x = clamp(s.aim.x + input.aimX * SP.aimSpeedX * dt, -7, 7);
    s.aim.y = clamp(s.aim.y + input.aimY * SP.aimSpeedY * dt, 0.1, 4);
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
    if (edge(input.newSpot, s.prev.newSpot)) newSpot(s);
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
      // A longer run-up takes longer.
      s.runupDur = SP.runupTime * (s.runup.dist / SP.runupDistDefault);
    }
  } else if (s.phase === 'runup') {
    s.runupT += dt;
    if (s.wall) updateWallRunup(s.wall, s.cfg.ai.wall, s.runupT);
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
        startWallJump(s.wall, s.cfg.ai.wall, s.rng, wallShouldJump(s.ball, s.spot, P, s.cfg.ai.wall));
      }
      if (s.keeper) keeperOnStrike(s.keeper, s.keeperAi, s.rng);
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
    if (edge(input.retry, s.prev.retry)) resetAttempt(s);
    else if (edge(input.newSpot, s.prev.newSpot)) newSpot(s);
    else if (timeUp) {
      if (s.result?.type === 'retake') resetAttempt(s); // same spot, no penalty
      else if (s.result?.rebound && reboundAvailable(s)) startFollowUp(s);
      else newSpot(s);
    }
  }

  if (s.phase === 'aim' || s.phase === 'charge') updatePreview(s);
  s.prev = { ...input };
}
