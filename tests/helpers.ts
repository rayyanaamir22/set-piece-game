import physics from '../data/physics.json';
import shot from '../data/shot.json';
import takers from '../data/takers.json';
import { PhysicsParams, newBall, stepBall } from '../src/sim/BallPhysics';
import { ShotParams, Taker } from '../src/sim/ShotModel';
import { SimConfig } from '../src/sim/Sim';
import { V3 } from '../src/sim/math/vec3';

export const P = physics as PhysicsParams;
export const SP = shot as ShotParams;
export const TAKERS = takers as Taker[];
export const CFG: SimConfig = { physics: P, shot: SP, takers: TAKERS };

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
