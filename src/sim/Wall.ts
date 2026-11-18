import { GOAL_HALF_WIDTH, WALL_DISTANCE } from './constants';
import { WallAi } from './ai';
import { RngState, rngNext, rngRange } from './math/rng';
import { V3, add, mul, norm } from './math/vec3';
import { SimSettings } from './settings';

export const WALL_PLAYER_RADIUS = 0.23;
/** Centre-to-centre spacing of players standing shoulder to shoulder. */
export const WALL_SPACING = 0.47;

export interface WallState {
  /** Feet position of each player (y = 0). */
  players: V3[];
  height: number;
  radius: number;
  /** Unit vector from the wall towards the ball (players face this way). */
  facing: V3;
  /** Current vertical lift of each player (0 = standing). Driven by `updateWallLift`. */
  lift: number[];
  /** Per-player jump plan, set when the ball is struck (null = nobody jumps). */
  jump: { start: number[]; height: number[]; duration: number } | null;
}

/** How many players the "referee" would put in the wall: closer and more central = more. */
export function autoWallCount(spot: V3): number {
  const dist = Math.hypot(spot.x, spot.z);
  let n = dist < 21 ? 5 : dist < 25 ? 4 : dist < 30 ? 3 : 2;
  const angle = Math.abs(Math.atan2(spot.x, spot.z));
  if (angle > 0.6) n -= 1; // wide of the goal: the near post is the main threat
  return Math.max(1, n);
}

/**
 * Wall of `n` players 9.15 m from the ball. Their near edge sits on the line
 * from the ball to the near post and they extend towards the centre of goal,
 * like a real wall. A central free kick gets a wall centred on the goal.
 */
export function layoutWall(spot: V3, settings: SimSettings): WallState | null {
  if (!settings.wallEnabled) return null;
  const n = settings.wallCount > 0 ? settings.wallCount : autoWallCount(spot);
  const width = (n - 1) * WALL_SPACING + 2 * WALL_PLAYER_RADIUS;

  const central = Math.abs(spot.x) < 1.2;
  const side = spot.x >= 0 ? 1 : -1;
  const target = central ? { x: 0, y: 0, z: 0 } : { x: side * GOAL_HALF_WIDTH, y: 0, z: 0 };
  const u = norm({ x: target.x - spot.x, y: 0, z: target.z - spot.z });
  // Perpendicular to the ball-post line, pointing towards the centre of goal.
  let p: V3 = { x: -u.z, y: 0, z: u.x };
  if (!central) {
    const towardsCentre = -side;
    if (Math.sign(p.x) !== towardsCentre) p = mul(p, -1);
  }
  const centre = central ? add(spot, mul(u, WALL_DISTANCE)) : add(add(spot, mul(u, WALL_DISTANCE)), mul(p, width / 2));

  const players: V3[] = [];
  for (let i = 0; i < n; i++) {
    const off = (i - (n - 1) / 2) * WALL_SPACING;
    const q = add(centre, mul(p, off));
    players.push({ x: q.x, y: 0, z: q.z });
  }
  return {
    players,
    height: settings.wallHeight,
    radius: WALL_PLAYER_RADIUS,
    facing: mul(u, -1),
    lift: players.map(() => 0),
    jump: null,
  };
}

/** Called at the strike: each player decides whether and when to jump. */
export function startWallJump(wall: WallState, ai: WallAi, rng: RngState): void {
  const start: number[] = [];
  const height: number[] = [];
  for (let i = 0; i < wall.players.length; i++) {
    const jumps = rngNext(rng) < ai.jumpChance;
    const delay = rngRange(rng, ai.reactionMin, ai.reactionMax);
    const h = ai.jumpHeight * (1 - ai.jumpHeightJitter * rngNext(rng));
    start.push(delay);
    height.push(jumps ? h : 0);
  }
  wall.jump = { start, height, duration: ai.jumpDuration };
}

/** Parabolic jump: lift(t) = h * 4u(1-u) with u = (t - start) / duration. `t` = seconds since the strike. */
export function updateWallLift(wall: WallState, t: number): void {
  const j = wall.jump;
  for (let i = 0; i < wall.lift.length; i++) {
    if (!j || j.height[i] <= 0) {
      wall.lift[i] = 0;
      continue;
    }
    const u = (t - j.start[i]) / j.duration;
    wall.lift[i] = u <= 0 || u >= 1 ? 0 : j.height[i] * 4 * u * (1 - u);
  }
}
