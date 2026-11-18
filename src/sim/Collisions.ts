import { BallState, PhysicsParams, groundContact, stepBall } from './BallPhysics';
import { GOAL_DEPTH, GOAL_HALF_WIDTH, GOAL_HEIGHT, POST_RADIUS } from './constants';
import { V3, add, dot, len, mul, sub, v3, clamp } from './math/vec3';
import { KeeperAi } from './ai';
import { KeeperState, keeperArmsOut, keeperArms } from './Keeper';
import { WallState } from './Wall';

export interface CollisionEvent {
  type: 'post' | 'bar' | 'net' | 'bounce' | 'wall' | 'keeper';
  speed: number;
  pos: V3;
  /** keeper contacts: ball speed before the touch, and whether it was on the hands/upper body. */
  preSpeed?: number;
  hands?: boolean;
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
function collideCapsule(
  b: BallState,
  P: PhysicsParams,
  a: V3,
  c: V3,
  radius: number,
  restitution: number,
  absorb: number,
  spinKeep: number,
  along?: { t: number },
): number {
  const ab = sub(c, a);
  const t = clamp(dot(sub(b.pos, a), ab) / dot(ab, ab), 0, 1);
  if (along) along.t = t;
  const closest = add(a, mul(ab, t));
  const d = sub(b.pos, closest);
  const dist = len(d);
  const minDist = P.radius + radius;
  if (dist >= minDist || dist < 1e-9) return 0;

  const n = mul(d, 1 / dist);
  b.pos = add(closest, mul(n, minDist));
  const vn = dot(b.vel, n);
  if (vn >= 0) return 0;
  b.vel = add(b.vel, mul(n, -(1 + restitution) * vn));
  b.vel = mul(b.vel, absorb);
  b.spin = mul(b.spin, spinKeep);
  return -vn;
}

function collideSegment(b: BallState, P: PhysicsParams, s: Segment): number {
  return collideCapsule(b, P, s.a, s.b, POST_RADIUS, P.postRestitution, 0.95, 0.6);
}

/** Wall players are upright capsules spanning 0..height. Returns the hardest impact. */
function collideWall(b: BallState, P: PhysicsParams, wall: WallState): number {
  let hardest = 0;
  const r = wall.radius;
  for (const p of wall.players) {
    const lift = wall.lift[wall.players.indexOf(p)] ?? 0;
    const a = v3(p.x, lift + r, p.z);
    const c = v3(p.x, lift + Math.max(r, wall.height - r), p.z);
    hardest = Math.max(hardest, collideCapsule(b, P, a, c, r, 0.3, 0.75, 0.5));
  }
  return hardest;
}

/** Keeper: body capsule from feet to hands, plus an arms capsule while he is upright. */
function collideKeeper(
  b: BallState,
  P: PhysicsParams,
  k: KeeperState,
  ai: KeeperAi,
): { speed: number; preSpeed: number; hands: boolean } | null {
  const pre = len(b.vel);
  const info = { t: 0 };
  let hit = collideCapsule(b, P, k.a, k.b, ai.radius, 0.35, 0.7, 0.5, info);
  let hands = info.t >= ai.handsT;
  if (hit === 0 && keeperArmsOut(k)) {
    const arms = keeperArms(k, ai);
    hit = collideCapsule(b, P, arms.a, arms.b, ai.armRadius, 0.35, 0.7, 0.5);
    hands = hit > 0;
  }
  return hit > 0 ? { speed: hit, preSpeed: pre, hands } : null;
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
  wall: WallState | null,
  keeper: KeeperState | null,
  keeperAi: KeeperAi,
  out: CollisionEvent[],
): void {
  stepBall(b, P, dt);

  const bounce = groundContact(b, P, dt);
  if (bounce > 1.5) out.push({ type: 'bounce', speed: bounce, pos: { ...b.pos } });

  for (const s of SEGMENTS) {
    const hit = collideSegment(b, P, s);
    if (hit > 0.5) out.push({ type: s.kind, speed: hit, pos: { ...b.pos } });
  }

  if (wall) {
    const hit = collideWall(b, P, wall);
    if (hit > 0.5) out.push({ type: 'wall', speed: hit, pos: { ...b.pos } });
  }

  if (keeper) {
    const hit = collideKeeper(b, P, keeper, keeperAi);
    if (hit && hit.speed > 0.3) {
      out.push({ type: 'keeper', speed: hit.speed, pos: { ...b.pos }, preSpeed: hit.preSpeed, hands: hit.hands });
    }
  }

  if (netActive) {
    const hit = collideNet(b, P, dt);
    if (hit > 3) out.push({ type: 'net', speed: hit, pos: { ...b.pos } });
  }
}
