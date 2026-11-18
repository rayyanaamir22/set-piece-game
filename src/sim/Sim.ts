import { BallState, PhysicsParams, newBall } from './BallPhysics';
import { CollisionEvent, stepWorld } from './Collisions';
import {
  GOAL_HALF_WIDTH,
  GOAL_HEIGHT,
  PENALTY_BOX_HALF,
  PENALTY_BOX_Z,
} from './constants';
import { RngState, rngRange } from './math/rng';
import { V3, clamp, copy, v3 } from './math/vec3';
import { Shot, ShotParams, Taker, computeShot } from './ShotModel';
import { SimSettings, defaultSettings } from './settings';
import { WallState, layoutWall } from './Wall';

export type Phase = 'aim' | 'charge' | 'runup' | 'flight' | 'result';
export type ResultType = 'goal' | 'wide' | 'over' | 'miss';

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

  settings: SimSettings;
  wall: WallState | null;
  result: { type: ResultType; woodwork: 'post' | 'bar' | null; wall: boolean } | null;
  woodwork: 'post' | 'bar' | null;
  hitWall: boolean;
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
    settings: { ...defaultSettings(), ...cfg.settings },
    wall: null,
    result: null,
    woodwork: null,
    hitWall: false,
    netActive: false,
    flightT: 0,
    stats: { attempts: 0, goals: 0 },
    prev: emptyInput(),
    events: [],
  };
  s.prevBallPos = copy(s.ball.pos);
  s.wall = layoutWall(s.spot, s.settings);
  updatePreview(s);
  return s;
}

/** Put the ball on a new spot and return to the aiming phase. */
export function setSpot(s: SimState, x: number, z: number): void {
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
  s.netActive = false;
  s.flightT = 0;
  s.wall = layoutWall(s.spot, s.settings);
  s.previewKey = '';
  updatePreview(s);
}

/** Apply changed settings. The wall is re-laid immediately while the player is still lining up. */
export function applySettings(s: SimState): void {
  if (s.phase === 'aim' || s.phase === 'charge') s.wall = layoutWall(s.spot, s.settings);
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

function finish(s: SimState, type: ResultType): void {
  s.phase = 'result';
  s.phaseT = 0;
  s.result = { type, woodwork: s.woodwork, wall: s.hitWall };
  if (type === 'goal') {
    s.stats.goals++;
    s.events.push({ type: 'goal' });
  }
  s.events.push({ type: 'result', result: type });
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
    }
  } else if (s.phase === 'runup') {
    s.runupT += dt;
    if (s.runupT >= SP.runupTime && s.pending) {
      const shot = s.pending;
      s.ball.vel = copy(shot.vel);
      s.ball.spin = copy(shot.spin);
      s.ball.knuckle = { amp: shot.knuckle ? P.knuckleAccel : 0, phase: shot.knucklePhase };
      s.stats.attempts++;
      s.events.push({ type: 'strike', speed: shot.speed });
      s.phase = 'flight';
      s.phaseT = 0;
      s.flightT = 0;
    }
  }

  if (s.phase === 'flight' || s.phase === 'result') {
    const ev: CollisionEvent[] = [];
    stepWorld(s.ball, P, dt, s.netActive, s.wall, ev);
    for (const e of ev) {
      if (e.type === 'post' || e.type === 'bar') s.woodwork = e.type;
      if (e.type === 'wall') s.hitWall = true;
      s.events.push(e);
    }
  }

  if (s.phase === 'flight') {
    s.flightT += dt;
    const b = s.ball;
    if (b.pos.z < -P.radius) classifyCrossing(s);
    else {
      const speed = Math.hypot(b.vel.x, b.vel.y, b.vel.z);
      const settled = b.pos.y <= P.radius + 0.01 && speed < 0.4;
      const gone = Math.abs(b.pos.x) > 60 || b.pos.z > 70;
      if (settled || gone || s.flightT > 7) finish(s, 'miss');
    }
  } else if (s.phase === 'result') {
    if (edge(input.retry, s.prev.retry)) resetAttempt(s);
    else if (edge(input.newSpot, s.prev.newSpot) || s.phaseT > SP.resultTime) newSpot(s);
  }

  if (s.phase === 'aim' || s.phase === 'charge') updatePreview(s);
  s.prev = { ...input };
}
