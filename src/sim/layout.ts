import { V3, add, mul, norm, v3 } from './math/vec3';

/** Horizontal unit vector from the ball spot towards the centre of goal. */
export function dirToGoal(spot: V3): V3 {
  return norm({ x: -spot.x, y: 0, z: -spot.z });
}

/** Right-hand side as seen by someone facing `dir` (y up). */
export function rightOf(dir: V3): V3 {
  return { x: -dir.z, y: 0, z: dir.x };
}

/**
 * Where the taker stands. A right-footed taker stands to the LEFT of the ball
 * (so the right foot swings through it) and vice versa.
 */
export function kickerPositions(spot: V3, foot: 'R' | 'L'): { runupStart: V3; strike: V3 } {
  const d = dirToGoal(spot);
  const r = rightOf(d);
  const side = foot === 'R' ? -1 : 1;
  const strike = add(add(spot, mul(r, side * 0.8)), mul(d, -0.55));
  const runupStart = add(add(strike, mul(r, side * 0.9)), mul(d, -2.0));
  return { runupStart: { ...runupStart, y: 0 }, strike: { ...strike, y: 0 } };
}

export const origin = (): V3 => v3();
