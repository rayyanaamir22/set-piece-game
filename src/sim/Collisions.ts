import { BallState, PhysicsParams, groundContact, stepBall } from './BallPhysics';
import { GOAL_DEPTH, GOAL_HALF_WIDTH, GOAL_HEIGHT, POST_RADIUS } from './constants';
import { V3, add, dot, len, mul, sub, v3, clamp } from './math/vec3';

export interface CollisionEvent {
  type: 'post' | 'bar' | 'net' | 'bounce';
  speed: number;
  pos: V3;
}

interface Segment {
  kind: 'post' | 'bar';
  a: V3;
  b: V3;
}

const SEGMENTS: Segment[] = [
  { kind: 'post', a: v3(-GOAL_HALF_WIDTH, 0, 0), b: v3(-GOAL_HALF_WIDTH, GOAL_HEIGHT, 0) },
  { kind: 'post', a: v3(GOAL_HALF_WIDTH, 0, 0), b: v3(GOAL_HALF_WIDTH, GOAL_HEIGHT, 0) },
  { kind: 'bar', a: v3(-GOAL_HALF_WIDTH, GOAL_HEIGHT, 0), b: v3(GOAL_HALF_WIDTH, GOAL_HEIGHT, 0) },
];

/** Sphere vs capsule (segment + radius). Returns the impact speed, or 0 if no hit. */
function collideSegment(b: BallState, P: PhysicsParams, s: Segment): number {
  const ab = sub(s.b, s.a);
  const t = clamp(dot(sub(b.pos, s.a), ab) / dot(ab, ab), 0, 1);
  const closest = add(s.a, mul(ab, t));
  const d = sub(b.pos, closest);
  const dist = len(d);
  const minDist = P.radius + POST_RADIUS;
  if (dist >= minDist || dist < 1e-9) return 0;

  const n = mul(d, 1 / dist);
  b.pos = add(closest, mul(n, minDist));
  const vn = dot(b.vel, n);
  if (vn >= 0) return 0;
  b.vel = add(b.vel, mul(n, -(1 + P.postRestitution) * vn));
  // Scuff off the tangential component a little, and kill some spin.
  b.vel = mul(b.vel, 0.95);
  b.spin = mul(b.spin, 0.6);
  return -vn;
}

/** The net: a soft box behind the goal line that swallows the ball. */
function collideNet(b: BallState, P: PhysicsParams, dt: number): number {
  if (b.pos.z >= 0) return 0;
  const R = P.radius;
  const hx = GOAL_HALF_WIDTH - R;
  const top = GOAL_HEIGHT - R;
  const back = -GOAL_DEPTH + R;
  const e = P.netRestitution;
  let impact = 0;

  if (b.pos.x > hx) {
    impact = Math.max(impact, b.vel.x);
    b.pos.x = hx;
    if (b.vel.x > 0) b.vel.x *= -e;
  } else if (b.pos.x < -hx) {
    impact = Math.max(impact, -b.vel.x);
    b.pos.x = -hx;
    if (b.vel.x < 0) b.vel.x *= -e;
  }
  if (b.pos.y > top) {
    impact = Math.max(impact, b.vel.y);
    b.pos.y = top;
    if (b.vel.y > 0) b.vel.y *= -e;
  }
  if (b.pos.z < back) {
    impact = Math.max(impact, -b.vel.z);
    b.pos.z = back;
    if (b.vel.z < 0) b.vel.z *= -e;
  }
  b.vel = mul(b.vel, Math.exp(-P.netDrag * dt));
  return impact;
}

/** One fixed physics step of the ball in the world; collision events are appended to `out`. */
export function stepWorld(
  b: BallState,
  P: PhysicsParams,
  dt: number,
  netActive: boolean,
  out: CollisionEvent[],
): void {
  stepBall(b, P, dt);

  const bounce = groundContact(b, P, dt);
  if (bounce > 1.5) out.push({ type: 'bounce', speed: bounce, pos: { ...b.pos } });

  for (const s of SEGMENTS) {
    const hit = collideSegment(b, P, s);
    if (hit > 0.5) out.push({ type: s.kind, speed: hit, pos: { ...b.pos } });
  }

  if (netActive) {
    const hit = collideNet(b, P, dt);
    if (hit > 3) out.push({ type: 'net', speed: hit, pos: { ...b.pos } });
  }
}
