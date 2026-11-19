import { BallState, PhysicsParams } from './BallPhysics';
import { GOAL_HALF_WIDTH, GOAL_HEIGHT } from './constants';
import { KeeperAi } from './ai';
import { RngState, rngNext, rngRange } from './math/rng';
import { V3, clamp, lerp, v3 } from './math/vec3';
import { predictAtPlane } from './Prediction';
import { SimSettings } from './settings';

/**
 * ready      – waiting for the strike / still reacting
 * committed  – has read the shot, waiting for the right moment to move
 * diving     – moving (step or dive) towards the hand target
 * watching   – decided there is nothing to do (heading wide/over)
 */
export type KeeperPhase = 'ready' | 'committed' | 'diving' | 'watching';

export interface KeeperState {
  phase: KeeperPhase;
  /** 'step' = small lateral move staying upright, 'dive' = full stretch, 'none' = not moving. */
  kind: 'none' | 'step' | 'dive';
  skill: number;
  /** Body capsule endpoints: feet-side `a` and hands-side `b` (world coordinates). */
  a: V3;
  b: V3;
  /** Time (s since the strike) at which he reacts to the shot. */
  decideAt: number;
  target: { x: number; y: number } | null;
  /** When (s since the strike) he starts moving, once committed. */
  launchAt: number;
  startA: V3;
  startB: V3;
  endA: V3;
  endB: V3;
  t0: number;
  dur: number;
  /** -1/+1 if he committed to a side before the strike (a guess), else 0. */
  guess: number;
}

const gauss = (r: RngState) => (rngNext(r) + rngNext(r) + rngNext(r) - 1.5) * 2;

export function layoutKeeper(spot: V3, settings: SimSettings, ai: KeeperAi): KeeperState | null {
  if (!settings.keeperPresent) return null;
  // Cover the far post (the wall covers the near one), shading further the wider the angle.
  const shade = ai.shadeMax * clamp(Math.abs(spot.x) / 10, 0, 1);
  const x = spot.x === 0 ? 0 : -Math.sign(spot.x) * shade;
  const a = v3(x, 0, ai.lineZ);
  const b = v3(x, ai.standHeight, ai.lineZ);
  return {
    phase: 'ready',
    kind: 'none',
    skill: settings.keeperSkill,
    a,
    b,
    decideAt: Infinity,
    target: null,
    launchAt: Infinity,
    startA: a,
    startB: b,
    endA: a,
    endB: b,
    t0: 0,
    dur: 1,
    guess: 0,
  };
}

/** Whether the standing arms capsule is part of the hitbox (it folds into the dive line once he stretches). */
export function keeperArmsOut(k: KeeperState): boolean {
  return k.phase !== 'diving' || k.kind === 'step';
}

export function keeperArms(k: KeeperState, ai: KeeperAi): { a: V3; b: V3 } {
  const x = k.b.x;
  return { a: v3(x - ai.armSpan, ai.armY, k.b.z), b: v3(x + ai.armSpan, ai.armY, k.b.z) };
}

/**
 * Called at the strike: the keeper starts his reaction clock. Occasionally
 * (more often for weaker keepers) he has already guessed a side and goes
 * for it without reading the shot at all.
 */
export function keeperOnStrike(k: KeeperState, ai: KeeperAi, rng: RngState): void {
  k.decideAt = lerp(ai.reactionSlow, ai.reactionFast, k.skill) + rngRange(rng, -ai.reactionJitter, ai.reactionJitter);
  k.phase = 'ready';
  k.guess = 0;
  if (rngNext(rng) < ai.earlyGuessChance * (1 - k.skill)) {
    k.guess = rngNext(rng) < 0.5 ? -1 : 1;
    k.phase = 'committed';
    k.launchAt = 0.05;
  }
}

interface Read {
  px: number;
  py: number;
  /** Seconds from now until the ball reaches his line. */
  t: number;
}

/**
 * Read the flight of the ball. He only partly accounts for spin (so curl can
 * fool him), never for the knuckle wobble, and has some skill-dependent noise.
 * Returns null when the ball is heading well wide/over (nothing to do).
 */
function read(
  k: KeeperState,
  ball: BallState,
  P: PhysicsParams,
  ai: KeeperAi,
  rng: RngState,
  noiseScale: number,
): Read | null {
  const spinRead = lerp(ai.spinReadMin, ai.spinReadMax, k.skill);
  const spin = { x: ball.spin.x * spinRead, y: ball.spin.y * spinRead, z: ball.spin.z * spinRead };
  const pred = predictAtPlane(ball.pos, ball.vel, spin, P, ai.lineZ, ball.wind);
  if (!pred) return null;
  const sd = (1 - k.skill) * ai.predictNoise * noiseScale;
  const px = pred.x + gauss(rng) * sd;
  const py = pred.y + gauss(rng) * sd * 0.7;
  const off = ai.ignoreMargin;
  if (Math.abs(px) > GOAL_HALF_WIDTH + off || py > GOAL_HEIGHT + off || py < -off) return null;
  return { px, py, t: pred.t };
}

/** Choose the hand target (clamped to his reach ellipse), move type and duration from a read. */
function plan(k: KeeperState, r: Read, ai: KeeperAi): void {
  const x0 = k.b.x;
  let dx = r.px - x0;
  let dy = r.py - ai.reachCenterY;
  const e = Math.hypot(dx / ai.reachX, dy / ai.reachY);
  if (e > 1) {
    dx /= e;
    dy /= e;
  }
  const tx = x0 + dx;
  const ty = Math.max(0.2, ai.reachCenterY + dy);

  const dist = Math.hypot(tx - k.b.x, ty - k.b.y);
  const speed = ai.diveSpeed * (0.8 + 0.4 * k.skill);
  k.dur = clamp(dist / speed, ai.minDiveTime, ai.maxDiveTime);
  k.kind = dist < 1.0 ? 'step' : 'dive';
  k.target = { x: tx, y: ty };

  // Body line from the feet to the hands, pointing from his stance towards the target.
  const len = Math.hypot(tx - x0, ty) || 1;
  const dirx = (tx - x0) / len;
  const diry = ty / len;
  const endB = v3(tx, ty, k.b.z);
  const endA = v3(tx - dirx * ai.bodyLength, Math.max(0, ty - diry * ai.bodyLength), k.b.z);
  if (k.kind === 'step') {
    // Stays upright: only the lateral position and hand height change.
    endA.x = tx;
    endA.y = 0;
  }
  k.startA = k.a;
  k.startB = k.b;
  k.endA = endA;
  k.endB = endB;
}

/** The reaction: first read of the shot. He then waits so the dive arrives as the ball does. */
function decide(k: KeeperState, t: number, ball: BallState, P: PhysicsParams, ai: KeeperAi, rng: RngState): void {
  const r = read(k, ball, P, ai, rng, 1);
  if (!r) {
    k.phase = 'watching';
    return;
  }
  plan(k, r, ai);
  // Leave just early enough to arrive with the ball (never before the reaction is over).
  k.launchAt = Math.max(t, t + r.t - k.dur - 0.03);
  k.phase = 'committed';
}

/** Advance the keeper's AI/animation. `t` = seconds since the strike. */
export function keeperStep(
  k: KeeperState,
  t: number,
  ball: BallState,
  P: PhysicsParams,
  ai: KeeperAi,
  rng: RngState,
): void {
  if (k.phase === 'ready' && t >= k.decideAt) decide(k, t, ball, P, ai, rng);
  if (k.phase === 'committed' && t >= k.launchAt) {
    // Second, better-informed look just before leaving the ground (a guess has no look: he just goes).
    const r: Read | null = k.guess
      ? { px: k.b.x + k.guess * 1.9, py: 1.0, t: 0 }
      : read(k, ball, P, ai, rng, 0.5);
    if (!r) {
      k.phase = 'watching';
      return;
    }
    plan(k, r, ai);
    k.t0 = t;
    k.phase = 'diving';
  }
  if (k.phase !== 'diving') return;

  const u = clamp((t - k.t0) / k.dur, 0, 1);
  const e = 1 - (1 - u) * (1 - u);
  const a = v3(lerp(k.startA.x, k.endA.x, e), lerp(k.startA.y, k.endA.y, e), k.a.z);
  const b = v3(lerp(k.startB.x, k.endB.x, e), lerp(k.startB.y, k.endB.y, e), k.b.z);
  if (k.kind === 'dive' && u >= 1) {
    // Lands: both ends settle towards the ground.
    const s = clamp((t - k.t0 - k.dur) / ai.sinkTime, 0, 1);
    a.y = lerp(a.y, 0.1, s);
    b.y = lerp(b.y, 0.3, s);
  }
  k.a = a;
  k.b = b;
}
