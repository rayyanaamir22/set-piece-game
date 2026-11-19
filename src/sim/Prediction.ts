import { PhysicsParams, groundContact, newBall, stepBall } from './BallPhysics';
import { V3, copy, v3 } from './math/vec3';

export interface PlaneCrossing {
  x: number;
  y: number;
  /** Seconds from now until the ball reaches the plane. */
  t: number;
}

/** Fly a copy of the ball forward (no wobble, no obstacles) until it crosses the plane z = planeZ. */
export function predictAtPlane(
  pos: V3,
  vel: V3,
  spin: V3,
  P: PhysicsParams,
  planeZ: number,
  wind: V3 = v3(),
  maxT = 3,
): PlaneCrossing | null {
  const b = newBall(pos);
  b.vel = copy(vel);
  b.spin = copy(spin);
  b.wind = copy(wind);
  const dt = 1 / 120;
  while (b.t < maxT) {
    const prev = b.pos;
    stepBall(b, P, dt);
    groundContact(b, P, dt);
    if (b.pos.z <= planeZ) {
      const f = prev.z === b.pos.z ? 0 : (prev.z - planeZ) / (prev.z - b.pos.z);
      return {
        x: prev.x + (b.pos.x - prev.x) * f,
        y: prev.y + (b.pos.y - prev.y) * f,
        t: b.t - dt * (1 - f),
      };
    }
  }
  return null;
}

/** Fly a copy of the ball forward until it is `dist` metres (horizontally) from `from`; returns where it is then. */
export function predictAtDistance(
  pos: V3,
  vel: V3,
  spin: V3,
  P: PhysicsParams,
  from: V3,
  dist: number,
  wind: V3 = v3(),
  maxT = 2,
): PlaneCrossing | null {
  const b = newBall(pos);
  b.vel = copy(vel);
  b.spin = copy(spin);
  b.wind = copy(wind);
  const dt = 1 / 120;
  while (b.t < maxT) {
    stepBall(b, P, dt);
    groundContact(b, P, dt);
    if (Math.hypot(b.pos.x - from.x, b.pos.z - from.z) >= dist) {
      return { x: b.pos.x, y: b.pos.y, t: b.t };
    }
  }
  return null;
}
