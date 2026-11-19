import { BallState, PhysicsParams } from './BallPhysics';
import { GOAL_HALF_WIDTH, GOAL_HEIGHT, WALL_DISTANCE } from './constants';
import { WallAi } from './ai';
import { RngState, rngNext, rngRange } from './math/rng';
import { V3, add, mul, norm } from './math/vec3';
import { predictAtDistance, predictAtPlane } from './Prediction';
import { Frequency, SimSettings } from './settings';

export const WALL_PLAYER_RADIUS = 0.23;
/** Centre-to-centre spacing of players standing shoulder to shoulder. */
export const WALL_SPACING = 0.47;

export interface WallState {
  /** Current feet position of each player (y = 0): base + advance + shuffle. Read by collisions and rendering. */
  players: V3[];
  /** Where each player was placed. */
  base: V3[];
  /** Metres each player has moved towards the ball (encroachment / charge-off). */
  advance: number[];
  /** Sideways shuffle (m) along `axis`. */
  shuffle: number[];
  /** Unit vector along the wall. */
  axis: V3;
  /** Indices of players who creep forward before the strike and charge after it. */
  encroachers: number[];
  height: number;
  radius: number;
  /** Unit vector from the wall towards the ball (players face this way). */
  facing: V3;
  /** Current vertical lift of each player (0 = standing). Driven by `updateWallLift`. */
  lift: number[];
  /** Per-player jump plan, set when the ball is struck (null = nobody jumps). */
  jump: { start: number[]; height: number[]; duration: number } | null;
  /** Players who already jumped for a dummy run-up (they will not jump again at the real strike). */
  earlyJumped: boolean[];
  /** A player lying on the ground behind the wall (blocks low shots), as a horizontal capsule. */
  lying: { a: V3; b: V3; radius: number } | null;
}

/** Per-attempt random decisions, drawn once so that re-laying the wall (settings changes) is stable. */
export interface WallRolls {
  lyingMan: boolean;
  /** Fraction 0..1 picking which player encroaches, or null for none. */
  encroach: number | null;
}

export const noRolls = (): WallRolls => ({ lyingMan: false, encroach: null });

export function rollWall(rng: RngState, settings: SimSettings, ai: WallAi): WallRolls {
  const roll = (f: Frequency, chance: number) => (f === 'always' ? true : f === 'sometimes' ? rngNext(rng) < chance : false);
  const lyingMan = roll(settings.lyingMan, ai.lyingManChance);
  const encroach = roll(settings.encroachment, ai.encroachChance) ? rngNext(rng) : null;
  return { lyingMan, encroach };
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
export function layoutWall(spot: V3, settings: SimSettings, ai?: WallAi, rolls: WallRolls = noRolls()): WallState | null {
  if (!settings.wallEnabled) return null;
  // Too close to goal for a 9.15 m wall to fit between the ball and the goal line.
  if (Math.hypot(spot.x, spot.z) < WALL_DISTANCE + 1.5) return null;
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

  const base: V3[] = [];
  for (let i = 0; i < n; i++) {
    const off = (i - (n - 1) / 2) * WALL_SPACING;
    const q = add(centre, mul(p, off));
    base.push({ x: q.x, y: 0, z: q.z });
  }

  const facing = mul(u, -1);
  let lying: WallState['lying'] = null;
  if (ai && rolls.lyingMan) {
    // On the goal side of the wall, along its axis, hugging the ground.
    const c = add(centre, mul(facing, -ai.lyingManDistance));
    lying = {
      a: { x: c.x - p.x * ai.lyingManHalfLength, y: ai.lyingManRadius, z: c.z - p.z * ai.lyingManHalfLength },
      b: { x: c.x + p.x * ai.lyingManHalfLength, y: ai.lyingManRadius, z: c.z + p.z * ai.lyingManHalfLength },
      radius: ai.lyingManRadius,
    };
  }

  const encroachers: number[] = [];
  if (ai && rolls.encroach !== null) {
    const first = Math.min(n - 1, Math.floor(rolls.encroach * n));
    for (let k = 0; k < Math.max(1, ai.encroachPlayers); k++) encroachers.push((first + k) % n);
  }

  return {
    players: base.map((q) => ({ ...q })),
    base,
    advance: base.map(() => 0),
    shuffle: base.map(() => 0),
    axis: p,
    encroachers: [...new Set(encroachers)],
    height: settings.wallHeight,
    radius: WALL_PLAYER_RADIUS,
    facing,
    lift: base.map(() => 0),
    jump: null,
    earlyJumped: base.map(() => false),
    lying,
  };
}

/** Recompute `players` from base + advance + shuffle. */
export function syncWallPositions(wall: WallState): void {
  for (let i = 0; i < wall.players.length; i++) {
    const b = wall.base[i];
    wall.players[i] = {
      x: b.x + wall.facing.x * wall.advance[i] + wall.axis.x * wall.shuffle[i],
      y: 0,
      z: b.z + wall.facing.z * wall.advance[i] + wall.axis.z * wall.shuffle[i],
    };
  }
}

/** Run-up phase: the whole wall shuffles slightly, encroachers creep towards the ball. `t` = seconds into the run-up. */
export function updateWallRunup(wall: WallState, ai: WallAi, t: number): void {
  const ramp = Math.min(1, t / 0.2);
  for (let i = 0; i < wall.players.length; i++) {
    wall.shuffle[i] = ai.shuffleAmp * ramp * Math.sin(2 * Math.PI * ai.shuffleFreq * t + i * 1.7);
    wall.advance[i] = wall.encroachers.includes(i) ? Math.min(ai.creepMax, ai.creepSpeed * Math.max(0, t - 0.1)) : 0;
  }
  syncWallPositions(wall);
}

/** Is anyone closer than the 9.15 m line by more than the allowed margin? */
export function wallEncroaching(wall: WallState, ai: WallAi): boolean {
  return wall.advance.some((a) => a >= ai.encroachThreshold);
}

/** After the strike encroachers charge at the ball. */
export function updateWallFlight(wall: WallState, ai: WallAi, dt: number): void {
  if (wall.encroachers.length === 0) return;
  for (const i of wall.encroachers) wall.advance[i] = Math.min(ai.rushMax, wall.advance[i] + ai.rushSpeed * dt);
  syncWallPositions(wall);
}

/**
 * Would the wall jump? Only for shots it judges dangerous: heading on target
 * (within a metre of the frame). Optionally also only when the ball is higher
 * than `jumpMinBallHeight` at the wall (0 = no height criterion, so a
 * grounder can slide under the jumpers; that is what the lying man is for).
 */
export function wallShouldJump(ball: BallState, spot: V3, P: PhysicsParams, ai: WallAi): boolean {
  const goal = predictAtPlane(ball.pos, ball.vel, ball.spin, P, 0, ball.wind);
  if (!goal) return false;
  const m = ai.jumpOnTargetMargin;
  if (Math.abs(goal.x) > GOAL_HALF_WIDTH + m || goal.y > GOAL_HEIGHT + m) return false;
  if (ai.jumpMinBallHeight <= 0) return true;
  const atWall = predictAtDistance(ball.pos, ball.vel, ball.spin, P, spot, WALL_DISTANCE, ball.wind);
  return !!atWall && atWall.y > ai.jumpMinBallHeight;
}

/** A dummy run-up: the first runner steps over the ball and some of the wall jumps for it, too early. */
export function wallDummyJump(wall: WallState, ai: WallAi, rng: RngState): void {
  const start: number[] = [];
  const height: number[] = [];
  for (let i = 0; i < wall.players.length; i++) {
    const jumps = rngNext(rng) < ai.dummyJumpChance && !wall.encroachers.includes(i);
    const delay = rngRange(rng, 0, ai.dummyJumpReactionMax);
    const h = ai.jumpHeight * (1 - ai.jumpHeightJitter * rngNext(rng));
    start.push(delay);
    height.push(jumps ? h : 0);
    wall.earlyJumped[i] = jumps;
  }
  wall.jump = { start, height, duration: ai.jumpDuration };
}

/**
 * Called at the strike: each player decides whether and when to jump. Encroachers are busy charging.
 * `sinceDummy` is how long ago the dummy run happened (0 if there was none): players who jumped for it
 * carry on with that jump (and are down again by the time the ball arrives) instead of jumping twice.
 */
export function startWallJump(wall: WallState, ai: WallAi, rng: RngState, shouldJump = true, sinceDummy = 0): void {
  const early = wall.jump;
  const start: number[] = [];
  const height: number[] = [];
  for (let i = 0; i < wall.players.length; i++) {
    const jumps = rngNext(rng) < ai.jumpChance;
    const delay = rngRange(rng, ai.reactionMin, ai.reactionMax);
    const h = ai.jumpHeight * (1 - ai.jumpHeightJitter * rngNext(rng));
    if (early && wall.earlyJumped[i]) {
      start.push(early.start[i] - sinceDummy);
      height.push(early.height[i]);
    } else {
      start.push(delay);
      height.push(shouldJump && jumps && !wall.encroachers.includes(i) ? h : 0);
    }
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
