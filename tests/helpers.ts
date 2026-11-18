import physics from '../data/physics.json';
import shot from '../data/shot.json';
import ai from '../data/ai.json';
import takers from '../data/takers.json';
import { PhysicsParams, newBall, stepBall } from '../src/sim/BallPhysics';
import { AiParams } from '../src/sim/ai';
import { SimSettings } from '../src/sim/settings';
import { ShotParams, Taker } from '../src/sim/ShotModel';
import { SimConfig } from '../src/sim/Sim';
import { V3 } from '../src/sim/math/vec3';

export const P = physics as PhysicsParams;
export const SP = shot as ShotParams;
export const TAKERS = takers as Taker[];
export const AI = ai as AiParams;

/** Build a sim config. Defenders are off unless a test asks for them, so each test isolates one thing. */
export function makeCfg(settings: Partial<SimSettings> = {}, aiOverride: Partial<{ wall: Partial<AiParams['wall']>; keeper: Partial<AiParams['keeper']> }> = {}): SimConfig {
  return {
    physics: P,
    shot: SP,
    takers: TAKERS.map((t) => ({ ...t })),
    ai: { wall: { ...AI.wall, ...aiOverride.wall }, keeper: { ...AI.keeper, ...aiOverride.keeper } },
    settings: { wallEnabled: false, wallJumps: false, keeperPresent: false, ...settings },
  };
}

export const CFG: SimConfig = makeCfg();
export const CFG_WALL: SimConfig = makeCfg({ wallEnabled: true });

/** Free flight (no ground/posts) until z <= 0, returns samples. */
export function fly(start: V3, vel: V3, spin: V3, params: PhysicsParams = P, knuckle?: { amp: number; phase: number[] }) {
  const b = newBall(start);
  b.vel = { ...vel };
  b.spin = { ...spin };
  if (knuckle) b.knuckle = knuckle;
  const pts: V3[] = [];
  for (let i = 0; i < 3000 && b.pos.z > 0; i++) {
    stepBall(b, params, 1 / 240);
    pts.push({ ...b.pos });
  }
  return { ball: b, pts, t: b.t };
}
