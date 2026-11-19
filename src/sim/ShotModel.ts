import { BallState, PhysicsParams, newBall, stepBall } from './BallPhysics';
import { RngState, rngNext } from './math/rng';
import { V3, clamp, lerp } from './math/vec3';

export interface ShotParams {
  vMin: number;
  vMax: number;
  chargeTime: number;
  sweetMin: number;
  sweetMax: number;
  spinSide: number;
  spinVert: number;
  knuckleRadius: number;
  knuckleMinPower: number;
  knuckleSpin: number;
  baseError: number;
  missErrorGain: number;
  overpowerLift: number;
  drivenSpeed: number;
  drivenSpin: number;
  lowTargetY: number;
  lowTopspin: number;
  lowSpeed: number;
  /** Launch elevation (degrees) of a low drive: it skims and skids along the ground instead of flying to the target. */
  lowElevation: number;
  solverIterations: number;
  runupTime: number;
  runupAngleDefault: number;
  runupAngleMax: number;
  runupDistDefault: number;
  runupDistMin: number;
  runupDistMax: number;
  runupAngleRate: number;
  runupDistRate: number;
  powerStraight: number;
  powerWide: number;
  curlStraight: number;
  curlWide: number;
  distPower: number;
  knuckleMaxAngle: number;
  aimSpeedX: number;
  aimSpeedY: number;
  resultTime: number;
}

export interface Taker {
  name: string;
  foot: 'R' | 'L';
  /** 0..1: shrinks the random aim error. */
  accuracy: number;
  /** 0..1: scales spin. */
  curve: number;
  /** Scales launch speed. */
  power: number;
}

export interface ShotRequest {
  spot: V3;
  /** Where the reticle is on the goal plane (z = 0). */
  target: { x: number; y: number };
  /** Contact point on the ball face, unit disc: x right, y up. */
  contact: { x: number; y: number };
  /** Power meter value at release, 0..1. */
  power: number;
  driven: boolean;
  low: boolean;
  taker: Taker;
  /** Run-up angle (degrees off the straight line behind the ball) and length (m). Omitted = the neutral default. */
  runup?: { angle: number; dist: number };
  /** Wind (m/s). Only used to draw the predicted-path aid: the launch solver ignores it, so wind makes the ball miss the reticle. */
  wind?: V3;
  /** Pass null for a perfect (error-free, deterministic) shot, e.g. the aim preview. */
  rng: RngState | null;
}

export interface Shot {
  vel: V3;
  spin: V3;
  speed: number;
  /** Horizontal launch angle: 0 = straight down -z, positive towards +x. */
  azimuth: number;
  elevation: number;
  knuckle: boolean;
  knucklePhase: number[];
  /** Where the ball is aimed after timing/skill error was applied. */
  aimed: { x: number; y: number };
  error: { x: number; y: number };
  /** Sampled flight path from the solver (only if requested). */
  path?: V3[];
}

const SOLVE_DT = 1 / 240;

export interface RunupEffect {
  /** Multiplies launch speed. */
  power: number;
  /** Multiplies sidespin. */
  curl: number;
  /** A knuckleball needs a fairly straight approach. */
  knuckleOk: boolean;
}

/**
 * Straighter + longer run-up: more power, less curl. Wider: more curl, less power.
 * Normalised so the default run-up is exactly neutral (multipliers 1).
 */
export function runupEffect(runup: { angle: number; dist: number } | undefined, SP: ShotParams): RunupEffect {
  const angle = runup ? runup.angle : SP.runupAngleDefault;
  const dist = runup ? runup.dist : SP.runupDistDefault;
  const t = clamp(angle / SP.runupAngleMax, 0, 1);
  const t0 = SP.runupAngleDefault / SP.runupAngleMax;
  const angPower = lerp(SP.powerStraight, SP.powerWide, t) / lerp(SP.powerStraight, SP.powerWide, t0);
  const angCurl = lerp(SP.curlStraight, SP.curlWide, t) / lerp(SP.curlStraight, SP.curlWide, t0);
  const dd = dist - SP.runupDistDefault;
  const span = dd >= 0 ? SP.runupDistMax - SP.runupDistDefault : SP.runupDistDefault - SP.runupDistMin;
  const distPower = 1 + SP.distPower * clamp(dd / span, -1, 1);
  return { power: angPower * distPower, curl: angCurl, knuckleOk: angle <= SP.knuckleMaxAngle };
}

export function launchSpeed(
  power: number,
  driven: boolean,
  low: boolean,
  taker: Taker,
  SP: ShotParams,
  runupPower = 1,
): number {
  let v = (SP.vMin + (SP.vMax - SP.vMin) * clamp(power, 0, 1)) * taker.power * runupPower;
  if (driven) v *= SP.drivenSpeed;
  if (low) v *= SP.lowSpeed;
  return v;
}

function spinFor(
  az: number,
  contact: { x: number; y: number },
  power: number,
  driven: boolean,
  low: boolean,
  taker: Taker,
  SP: ShotParams,
  fx: RunupEffect,
): { spin: V3; knuckle: boolean } {
  let cx = contact.x;
  let cy = contact.y;
  const m = Math.hypot(cx, cy);
  if (m > 1) {
    cx /= m;
    cy /= m;
  }
  const knuckle = Math.hypot(cx, cy) < SP.knuckleRadius && power >= SP.knuckleMinPower && !driven && fx.knuckleOk;
  const rightX = Math.cos(az);
  const rightZ = Math.sin(az);
  if (knuckle) {
    // Barely any spin: this is what makes the ball unstable.
    const s = SP.knuckleSpin;
    return { spin: { x: rightX * s * 0.3, y: s, z: rightZ * s * 0.3 }, knuckle: true };
  }
  const scale = taker.curve * (driven ? SP.drivenSpin : 1);
  // Physics: striking the right of the ball spins it about +y (curls left);
  // striking the top gives topspin about -right (dips).
  const wy = cx * SP.spinSide * scale * fx.curl;
  const wr = -(cy + (low ? SP.lowTopspin : 0)) * SP.spinVert * scale;
  return { spin: { x: wr * rightX, y: wy, z: wr * rightZ }, knuckle: false };
}

function launchVelocity(az: number, el: number, speed: number): V3 {
  const ch = Math.cos(el) * speed;
  return { x: Math.sin(az) * ch, y: Math.sin(el) * speed, z: -Math.cos(az) * ch };
}

interface Crossing {
  x: number;
  y: number;
  ok: boolean;
}

/** Fly the ball (no wobble, no ground) until it reaches the goal plane z = 0. */
function flyToGoalPlane(
  spot: V3,
  vel: V3,
  spin: V3,
  P: PhysicsParams,
  path?: V3[],
  wind?: V3,
): Crossing {
  const b: BallState = newBall({ x: spot.x, y: P.radius, z: spot.z });
  b.vel = vel;
  b.spin = spin;
  if (wind) b.wind = wind;
  for (let i = 0; i < 1500; i++) {
    const prev = b.pos;
    stepBall(b, P, SOLVE_DT);
    if (path && i % 6 === 0) path.push({ ...b.pos });
    if (b.pos.z <= 0) {
      const f = prev.z === b.pos.z ? 0 : prev.z / (prev.z - b.pos.z);
      if (path) path.push({ x: prev.x + (b.pos.x - prev.x) * f, y: prev.y + (b.pos.y - prev.y) * f, z: 0 });
      return { x: prev.x + (b.pos.x - prev.x) * f, y: prev.y + (b.pos.y - prev.y) * f, ok: true };
    }
    if (b.t > 6) break;
  }
  return { x: b.pos.x, y: b.pos.y, ok: false };
}

function ballisticElevation(dist: number, height: number, speed: number, g: number): number {
  const v2 = speed * speed;
  const disc = v2 * v2 - g * (g * dist * dist + 2 * height * v2);
  if (disc < 0) return Math.PI / 4;
  return Math.atan((v2 - Math.sqrt(disc)) / (g * dist));
}

/**
 * Turn an aim request into a launch velocity + spin. A shooting-method solver
 * adjusts azimuth/elevation so the (spinning, dragged) ball arrives at the
 * target, which means the contact point decides *how* the ball gets there
 * (curl round the wall, dip under the bar) rather than where it ends up.
 */
export function computeShot(req: ShotRequest, P: PhysicsParams, SP: ShotParams, wantPath = false): Shot {
  const { spot, taker } = req;

  let tx = req.target.x;
  let ty = req.target.y;
  if (req.low) ty = Math.min(ty, SP.lowTargetY);

  // Timing / skill error.
  let ex = 0;
  let ey = 0;
  const knucklePhase = [0, 0, 0, 0, 0, 0];
  if (req.rng) {
    const d = req.power < SP.sweetMin ? SP.sweetMin - req.power : req.power > SP.sweetMax ? req.power - SP.sweetMax : 0;
    const mag = SP.baseError * (1 - taker.accuracy) + SP.missErrorGain * d;
    const theta = rngNext(req.rng) * Math.PI * 2;
    ex = mag * Math.cos(theta);
    ey = mag * Math.sin(theta) * 0.6;
    if (req.power > SP.sweetMax) ey += SP.overpowerLift * d;
    for (let i = 0; i < 6; i++) knucklePhase[i] = rngNext(req.rng) * Math.PI * 2;
  }
  tx += ex;
  ty += ey;

  const fx = runupEffect(req.runup, SP);
  const speed = launchSpeed(req.power, req.driven, req.low, taker, SP, fx.power);
  const dh = Math.hypot(tx - spot.x, spot.z);

  let az = Math.atan2(tx - spot.x, spot.z);
  let el = req.low ? (SP.lowElevation * Math.PI) / 180 : ballisticElevation(dh, ty - P.radius, speed, P.gravity);

  for (let i = 0; i < SP.solverIterations; i++) {
    const { spin: trialSpin } = spinFor(az, req.contact, req.power, req.driven, req.low, taker, SP, fx);
    const hit = flyToGoalPlane(spot, launchVelocity(az, el, speed), trialSpin, P);
    if (!hit.ok) {
      el += 0.05;
      continue;
    }
    const errX = hit.x - tx;
    const errY = hit.y - ty;
    if (Math.abs(errX) < 0.02 && Math.abs(errY) < 0.02) break;
    az -= (errX / dh) * 0.9;
    // A low drive is hit along the ground: only the direction is solved, the height is up to the bounces.
    if (!req.low) el -= (errY / dh) * 0.9;
    el = clamp(el, -0.1, 1.1);
  }

  const { spin, knuckle } = spinFor(az, req.contact, req.power, req.driven, req.low, taker, SP, fx);
  const vel = launchVelocity(az, el, speed);

  let path: V3[] | undefined;
  if (wantPath) {
    path = [{ x: spot.x, y: P.radius, z: spot.z }];
    flyToGoalPlane(spot, vel, spin, P, path, req.wind);
  }

  return {
    vel,
    spin,
    speed,
    azimuth: az,
    elevation: el,
    knuckle,
    knucklePhase,
    aimed: { x: tx, y: ty },
    error: { x: ex, y: ey },
    path,
  };
}
