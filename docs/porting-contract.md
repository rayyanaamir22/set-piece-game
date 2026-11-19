# Porting contract: the Set Piece simulation

> **Generated** by `npm run contract` from the TypeScript sources in `src/sim/`. Do not edit by hand:
> `npm test` fails when this file is out of date. The TypeScript code is the reference implementation;
> the native C++ build must reproduce it. The proof is `tests/golden/*.json` (see "Verification").

## 1. What is being ported

Everything under `src/sim/` and nothing else. The simulation has no dependency on rendering, the DOM,
input devices, audio or time: it is a pure function of `(state, input, dt)`. Phase B reads and renders
that state, and never writes to it.

| Module | Role |
|---|---|
| `math/vec3.ts`, `math/rng.ts` | vector maths; the seeded random generator |
| `constants.ts` | pitch/goal geometry |
| `BallPhysics.ts` | drag, Magnus, wind, knuckle wobble, RK4 step, ground contact |
| `Collisions.ts` | posts/bar, net, wall players, lying man, keeper capsules |
| `Prediction.ts` | ball look-ahead used by the keeper and the wall |
| `ShotModel.ts` | aim + contact point + power + run-up -> launch velocity and spin (shooting-method solver) |
| `Wall.ts` | wall layout, jump, encroachment, lying man |
| `Keeper.ts` | keeper AI: read, commit, dive, early guess |
| `settings.ts`, `ai.ts`, `presets.ts`, `challenge.ts` | rules, tuning types, difficulty, challenges |
| `layout.ts` | taker / run-up geometry |
| `Sim.ts` | the state machine and `step` |

Tuning values are *data*, shared by both builds: `data/physics.json`, `data/shot.json`, `data/ai.json`,
`data/takers.json`, `data/presets.json`, `data/challenges.json`.

## 2. Conventions

- **Units:** SI. Metres, seconds, kilograms, radians (the run-up angle is stored in *degrees*).
- **Axes:** origin = centre of the goal line on the ground. +x = to the right as seen from the pitch looking at
  goal, +y = up, +z = from the goal line out into the pitch (the ball starts at +z and goals are scored at z < 0).
- **Numbers:** IEEE-754 `double` everywhere (JavaScript numbers). Use `double` in C++, not `float`.
  Functions used: `sin`, `cos`, `atan`, `atan2`, `sqrt`, `exp`, `hypot`, `min`, `max`, `abs`, `floor`, `round`, `sign`.
  Keep the order of floating-point operations identical to the TypeScript.
- **Time:** one fixed tick of `dt = 1/240` s per `step`. Interpolation for rendering happens outside the sim
  (`prevBallPos` is saved at the start of every tick for that purpose).
- **Randomness:** only through `rngNext` / `rngRange` on `SimState.rng`. The *order and number of draws* is part of the
  contract. A port is correct only if the final `rng` state of every golden fixture matches exactly.
- **Input edges:** the sim detects edges itself by comparing the current `InputState` with `SimState.prev`. The app must
  pass the same held-state input on every tick of a frame, except the mouse deltas (`aimDeltaX/Y`), which are applied once.
- **Events:** `SimState.events` is an append-only list that the *app* clears once per rendered frame.

## 3. The random generator

```ts
export function rngNext(r: RngState): number {
  r.s = (r.s + 0x6d2b79f5) | 0;
  let t = r.s;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

export const rngRange = (r: RngState, lo: number, hi: number): number => lo + (hi - lo) * rngNext(r);
```

`>>>`, `|0` and `Math.imul` are 32-bit integer operations: use `uint32_t` / `int32_t` with wrap-around.

## 4. Geometry constants

```ts
// World layout. SI units. Origin = centre of the goal line on the ground.
//   +x = right as seen from the pitch looking at goal, +y = up,
//   +z = from the goal line out into the pitch (the ball starts at +z).
export const GOAL_HALF_WIDTH = 3.66;
export const GOAL_HEIGHT = 2.44;
export const GOAL_DEPTH = 2.0;
export const POST_RADIUS = 0.06;

export const PITCH_HALF_WIDTH = 34;
export const PENALTY_BOX_Z = 16.5;
export const PENALTY_BOX_HALF = 20.16;
export const SIX_YARD_Z = 5.5;
export const SIX_YARD_HALF = 9.16;
export const PENALTY_SPOT_Z = 11;
export const CENTRE_ARC_RADIUS = 9.15;

export const WALL_DISTANCE = 9.15;
```

## 5. State and input structures

### Input
```ts
export interface InputState {
  /** Reticle velocity axes, -1..1 (+x right, +y up). */
  aimX: number;
  aimY: number;
  /** Absolute contact point on the ball, unit disc. */
  contactX: number;
  contactY: number;
  shoot: boolean;
  driven: boolean;
  low: boolean;
  toggleFoot: boolean;
  retry: boolean;
  newSpot: boolean;
  /** Run-up arc angle / distance axes, -1..1 (wider/longer = +1). */
  runupAngle: number;
  runupDist: number;
  /** Mouse: reticle movement in metres this frame (applied once, not per sim step). */
  aimDeltaX: number;
  aimDeltaY: number;
  /** Hold at the release to send a dummy runner over the ball first. */
  dummy: boolean;
}

export const emptyInput = (): InputState => ({
  aimX: 0,
  aimY: 0,
  contactX: 0,
  contactY: 0,
  shoot: false,
  driven: false,
  low: false,
  toggleFoot: false,
  retry: false,
  newSpot: false,
  runupAngle: 0,
  runupDist: 0,
  aimDeltaX: 0,
  aimDeltaY: 0,
  dummy: false,
});
```

### Events
```ts
export type SimEvent =
  | { type: 'strike'; speed: number }
  | { type: 'goal' }
  | { type: 'result'; result: ResultType }
  | CollisionEvent;

export interface CollisionEvent {
  type: 'post' | 'bar' | 'net' | 'bounce' | 'wall' | 'keeper';
  speed: number;
  pos: V3;
  /** keeper contacts: ball speed before the touch, and whether it was on the hands/upper body. */
  preSpeed?: number;
  hands?: boolean;
}
```

### Simulation state
```ts
export type Phase = 'aim' | 'charge' | 'runup' | 'flight' | 'result';

export type ResultType = 'goal' | 'wide' | 'over' | 'miss' | 'saved' | 'retake';

export type KeeperTouch = 'hold' | 'touch';

export type Mode = 'free' | 'practice' | 'challenge';

export interface SimConfig {
  settings?: Partial<SimSettings>;
  physics: PhysicsParams;
  shot: ShotParams;
  ai: AiParams;
  takers: Taker[];
  challenges: ChallengeDef[];
  presets: Record<DifficultyName, DifficultyPreset>;
}

export interface ShotPreview {
  azimuth: number;
  elevation: number;
  knuckle: boolean;
  path: V3[] | null;
}

export interface SimState {
  cfg: SimConfig;
  rng: RngState;
  phase: Phase;
  phaseT: number;
  taker: Taker;
  foot: 'R' | 'L';

  spot: V3;
  /** Incremented whenever the ball is moved to a new spot (camera snaps). */
  spotId: number;
  ball: BallState;
  prevBallPos: V3;

  aim: { x: number; y: number };
  contact: { x: number; y: number };
  meter: number;
  driven: boolean;
  low: boolean;

  pending: Shot | null;
  runupT: number;
  preview: ShotPreview | null;
  previewKey: string;
  showPath: boolean;

  /** Run-up arc: degrees off the straight line behind the ball, and distance (m). */
  runup: { angle: number; dist: number };
  /** Duration of the current run-up animation/clock (scales with distance). */
  runupDur: number;

  /** Free kicks (random spots), practice (same spot, instant retries) or a scored skill challenge. */
  mode: Mode;
  challenge: ChallengeRun | null;
  takerIndex: number;
  difficulty: DifficultyName | 'custom';
  /** Added to the taker's accuracy (difficulty preset). */
  accuracyBonus: number;
  /** A dummy runner goes over the ball first in this attempt. */
  dummy: boolean;
  dummyAt: number;
  dummyDone: boolean;
  /** Where the ball crossed the goal plane (z = 0) in this attempt, or null. */
  crossing: { x: number; y: number } | null;

  settings: SimSettings;
  /** Keeper tuning in force: base values + the selected keeper type. */
  keeperAi: KeeperAi;
  /** Per-attempt random wall decisions (lying man, encroachment). */
  rolls: WallRolls;
  /** The wall was past the 9.15 m line at the strike (a retake unless it scores). */
  encroached: boolean;
  /** This attempt is a rebound follow-up after a parry. */
  followUp: boolean;
  reboundCount: number;
  wall: WallState | null;
  keeper: KeeperState | null;
  result: {
    type: ResultType;
    woodwork: 'post' | 'bar' | null;
    wall: boolean;
    keeper: KeeperTouch | null;
    /** The ball stayed in the box: a follow-up shot comes next. */
    rebound: boolean;
    /** Challenge mode: points scored by this attempt and the ring crossed (-1 = none). */
    points: number;
    ring: number;
    crossing: { x: number; y: number } | null;
  } | null;
  woodwork: 'post' | 'bar' | null;
  hitWall: boolean;
  /** How the keeper first got to the ball: caught it ('hold') or got a hand/body to it ('touch'). */
  keeperTouch: KeeperTouch | null;
  /** flightT of the first deflection (wall or keeper); used to end the attempt after a wait. */
  deflectT: number | null;
  lastShotKnuckle: boolean;
  netActive: boolean;
  flightT: number;

  stats: { attempts: number; goals: number };
  prev: InputState;
  events: SimEvent[];
}
```

### Ball
```ts
export interface PhysicsParams {
  mass: number;
  radius: number;
  gravity: number;
  airDensity: number;
  cdHigh: number;
  cdLow: number;
  crisisStart: number;
  crisisEnd: number;
  clGain: number;
  clMax: number;
  spinDecay: number;
  restitution: number;
  friction: number;
  inertiaFactor: number;
  rollingResistance: number;
  knuckleAccel: number;
  knuckleSpeedMin: number;
  knuckleSpeedFull: number;
  postRestitution: number;
  netRestitution: number;
  netDrag: number;
}

export interface Knuckle {
  /** Peak lateral acceleration (m/s^2). 0 disables the wobble. */
  amp: number;
  /** Six random phases (two noise channels x three sines). */
  phase: number[];
}

export interface BallState {
  pos: V3;
  vel: V3;
  /** Angular velocity (rad/s), world frame. */
  spin: V3;
  t: number;
  knuckle: Knuckle;
  /** Air velocity (m/s). Drag and Magnus act on the ball's velocity *relative to the air*. */
  wind: V3;
}
```

### Shot model
```ts
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
  /** Extra run-up time (s) when a dummy runner goes first. */
  dummyDelay: number;
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
  /** Short description for the roster picker. */
  style?: string;
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

export interface RunupEffect {
  /** Multiplies launch speed. */
  power: number;
  /** Multiplies sidespin. */
  curl: number;
  /** A knuckleball needs a fairly straight approach. */
  knuckleOk: boolean;
}
```

### Wall and keeper
```ts
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

export interface WallRolls {
  lyingMan: boolean;
  /** Fraction 0..1 picking which player encroaches, or null for none. */
  encroach: number | null;
}

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
```

### Tuning types
```ts
export interface WallAi {
  /** Probability that an individual wall player jumps. */
  jumpChance: number;
  /** Seconds after the strike before a player leaves the ground (random in [min, max]). */
  reactionMin: number;
  reactionMax: number;
  /** Peak lift of the jump in metres (reduced by up to `jumpHeightJitter`, fractionally, per player). */
  jumpHeight: number;
  jumpHeightJitter: number;
  /** Total time in the air. */
  jumpDuration: number;
  /** After the ball hits the wall: seconds to wait for a deflection to reach goal before ending the attempt. */
  afterHitWait: number;
  /** Smart jumping: they only jump when the shot is on target and above this height at the wall (m). 0 = always jump. */
  jumpMinBallHeight: number;
  /** How far outside the frame (m) a shot may be predicted and still count as "on target". */
  jumpOnTargetMargin: number;
  /** Lying man: probability under "sometimes", distance behind the wall (towards goal), half length, radius. */
  lyingManChance: number;
  lyingManDistance: number;
  lyingManHalfLength: number;
  lyingManRadius: number;
  /** Encroachment: probability under "sometimes", how many players, creep speed/limit (m/s, m) before the strike,
   *  and how far forward counts as an offence (m). */
  encroachChance: number;
  encroachPlayers: number;
  creepSpeed: number;
  creepMax: number;
  encroachThreshold: number;
  /** Speed (m/s) and max distance (m) of an early charge-off once the ball is struck. */
  rushSpeed: number;
  rushMax: number;
  /** Small sideways shuffle of the whole wall during the run-up: amplitude (m) and frequency (Hz). */
  shuffleAmp: number;
  shuffleFreq: number;
  /** Dummy run-up: chance each player jumps for the decoy, and the max reaction delay (s). */
  dummyJumpChance: number;
  dummyJumpReactionMax: number;
}

export interface KeeperAi {
  /** Distance of the keeper's feet from the goal line. */
  lineZ: number;
  /** Max sideways shade towards the far post. */
  shadeMax: number;
  /** Reaction time (s) of a keeper with skill 0 / skill 1, and random jitter. */
  reactionSlow: number;
  reactionFast: number;
  reactionJitter: number;
  /** Elliptical hand-reach envelope around the stance: half-width, half-height and centre height. */
  reachX: number;
  reachY: number;
  reachCenterY: number;
  /** Feet-to-fingertips length of the body when stretched out in a dive. */
  bodyLength: number;
  standHeight: number;
  /** Body capsule radius, arm capsule half-span/radius/height while standing. */
  radius: number;
  armSpan: number;
  armRadius: number;
  armY: number;
  /** Hand speed in a dive (m/s) at skill 0.5, and the allowed duration range. */
  diveSpeed: number;
  minDiveTime: number;
  maxDiveTime: number;
  /** Fraction of the ball's spin the keeper accounts for when predicting its path (skill 0 -> 1). */
  spinReadMin: number;
  spinReadMax: number;
  /** Std-dev (m) of the prediction error at skill 0. */
  predictNoise: number;
  /** A shot predicted to miss the frame by more than this is left alone. */
  ignoreMargin: number;
  /** Fastest shot (m/s) he can hold at skill 0 / 1; contact beyond `handsT` along the body counts as hands. */
  holdSpeedMin: number;
  holdSpeedMax: number;
  handsT: number;
  /** Knuckleballs are harder to hold: multiplies the hold speed. */
  knuckleHoldFactor: number;
  /** After a parry/block: seconds to wait for a rebound before ending the attempt as a save. */
  afterTouchWait: number;
  /** Chance (x (1 - skill)) that he commits to a side before the strike. */
  earlyGuessChance: number;
  /** A parried ball is a live rebound if it ends inside this box (|x| <= boxX, zMin <= z <= zMax) slower than this (m/s). */
  reboundMaxSpeed: number;
  reboundBoxX: number;
  reboundZMin: number;
  reboundZMax: number;
  /** Max consecutive follow-up shots. */
  maxRebounds: number;
  /** Extra chance (x (1 - skill)) that a dummy run-up makes him commit to a side. */
  dummyBite: number;
  /** Time for a diving keeper to hit the ground. */
  sinkTime: number;
}

export interface AiParams {
  wall: WallAi;
  keeper: KeeperAi;
  /** Per-type overrides applied on top of `keeper` (see `SimSettings.keeperType`). */
  keeperPresets: Record<KeeperType, Partial<KeeperAi>>;
}

export function effectiveKeeperAi(ai: AiParams, type: KeeperType): KeeperAi {
  return { ...ai.keeper, ...ai.keeperPresets[type] };
}
```

### Settings (with their legal ranges)
```ts
export type Frequency = 'off' | 'sometimes' | 'always';

export type KeeperType = 'balanced' | 'quick' | 'tall' | 'strong';

export interface SimSettings {
  wallEnabled: boolean;
  /** Height of the wall players in metres. */
  wallHeight: number;
  /** Number of wall players; 0 = automatic from distance and angle. */
  wallCount: number;
  /** Wall players jump as the ball is struck. */
  wallJumps: boolean;
  /** A player lies down behind the wall to block low shots. */
  lyingMan: Frequency;
  /** A wall player creeps forward before the kick (a retake unless the shot scores). */
  encroachment: Frequency;
  /** A goalkeeper defends the goal. */
  keeperPresent: boolean;
  /** 0..1: reaction time, prediction quality, dive speed and hold strength. */
  keeperSkill: number;
  /** Parried balls that stay in the box become a follow-up shot. */
  rebounds: boolean;
  /** Keeper preset: reaction, reach, dive speed and hold strength. */
  keeperType: KeeperType;
  /** Wind blows across the pitch. */
  windEnabled: boolean;
  /** Wind speed in m/s. */
  windSpeed: number;
  /** Direction the wind blows towards, degrees (0 = towards goal, 90 = towards +x). */
  windDirection: number;
}

export const defaultSettings = (): SimSettings => ({
  wallEnabled: true,
  wallHeight: 1.85,
  wallCount: 0,
  wallJumps: true,
  lyingMan: 'sometimes',
  encroachment: 'sometimes',
  rebounds: true,
  keeperType: 'balanced',
  keeperPresent: true,
  keeperSkill: 0.7,
  windEnabled: false,
  windSpeed: 4,
  windDirection: 90,
});

export function sanitizeSettings(raw: Partial<SimSettings>): SimSettings {
  const d = defaultSettings();
  const num = (v: unknown, lo: number, hi: number, fallback: number) =>
    typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : fallback;
  const bool = (v: unknown, fallback: boolean) => (typeof v === 'boolean' ? v : fallback);
  return {
    wallEnabled: bool(raw.wallEnabled, d.wallEnabled),
    wallHeight: num(raw.wallHeight, WALL_HEIGHT_MIN, WALL_HEIGHT_MAX, d.wallHeight),
    wallCount: Math.round(num(raw.wallCount, 0, WALL_COUNT_MAX, d.wallCount)),
    wallJumps: bool(raw.wallJumps, d.wallJumps),
    lyingMan: FREQUENCIES.includes(raw.lyingMan as Frequency) ? (raw.lyingMan as Frequency) : d.lyingMan,
    encroachment: FREQUENCIES.includes(raw.encroachment as Frequency) ? (raw.encroachment as Frequency) : d.encroachment,
    rebounds: bool(raw.rebounds, d.rebounds),
    keeperType: KEEPER_TYPES.includes(raw.keeperType as KeeperType) ? (raw.keeperType as KeeperType) : d.keeperType,
    keeperPresent: bool(raw.keeperPresent, d.keeperPresent),
    keeperSkill: num(raw.keeperSkill, 0, 1, d.keeperSkill),
    windEnabled: bool(raw.windEnabled, d.windEnabled),
    windSpeed: num(raw.windSpeed, 0, WIND_SPEED_MAX, d.windSpeed),
    windDirection: num(raw.windDirection, 0, 360, d.windDirection),
  };
}

export function windVector(s: SimSettings): { x: number; y: number; z: number } {
  if (!s.windEnabled || s.windSpeed <= 0) return { x: 0, y: 0, z: 0 };
  const a = (s.windDirection * Math.PI) / 180;
  return { x: Math.sin(a) * s.windSpeed, y: 0, z: -Math.cos(a) * s.windSpeed };
}
```

### Modes, challenges, difficulty
```ts
export interface ChallengeTarget {
  /** Centre of the ring on the goal plane (m). */
  x: number;
  y: number;
  /** Radius (m). */
  r: number;
  points: number;
}

export interface ChallengeDef {
  id: string;
  name: string;
  desc: string;
  /** Attempts per run. */
  shots: number;
  spot: { x: number; z: number };
  /** Settings forced for the duration of the challenge. */
  settings: Partial<SimSettings>;
  targets: ChallengeTarget[];
  /** Points for any goal that misses every ring. */
  goalPoints: number;
  medals: { bronze: number; silver: number; gold: number };
}

export interface ChallengeShot {
  type: string;
  points: number;
  /** Index of the ring that was hit, or -1. */
  ring: number;
}

export interface ChallengeRun {
  index: number;
  def: ChallengeDef;
  taken: number;
  points: number;
  log: ChallengeShot[];
  done: boolean;
  medal: Medal | null;
  /** Settings to restore when the challenge ends. */
  backup: SimSettings;
}

export function scoreShot(
  def: ChallengeDef,
  crossing: { x: number; y: number } | null,
  type: string,
): { points: number; ring: number } {

export function medalFor(def: ChallengeDef, points: number): Medal | null {
  if (points >= def.medals.gold) return 'gold';
  if (points >= def.medals.silver) return 'silver';
  if (points >= def.medals.bronze) return 'bronze';
  return null;
}

export interface DifficultyPreset {
  label: string;
  /** Added to the taker's accuracy (shrinks or grows the timing/skill error). */
  accuracyBonus: number;
  settings: Partial<SimSettings>;
}

export type DifficultyName = 'easy' | 'normal' | 'hard' | 'pro';
```

## 6. The tick: order of operations

`step(s, input, dt)` is the only entry point that advances time. The order below matters.

```ts
export function step(s: SimState, input: InputState, dt: number): void {
  const SP = s.cfg.shot;
  const P = s.cfg.physics;
  const edge = (cur: boolean, prev: boolean) => cur && !prev;
  s.prevBallPos = copy(s.ball.pos);
  s.phaseT += dt;

  const adjusting = s.phase === 'aim' || s.phase === 'charge';
  if (adjusting) {
    s.aim.x = clamp(s.aim.x + input.aimX * SP.aimSpeedX * dt + input.aimDeltaX, -7, 7);
    s.aim.y = clamp(s.aim.y + input.aimY * SP.aimSpeedY * dt + input.aimDeltaY, 0.1, 4);
    let cx = input.contactX;
    let cy = input.contactY;
    const m = Math.hypot(cx, cy);
    if (m > 1) {
      cx /= m;
      cy /= m;
    }
    const k = Math.min(1, dt * 14);
    s.contact.x += (cx - s.contact.x) * k;
    s.contact.y += (cy - s.contact.y) * k;
    s.driven = input.driven;
    s.low = input.low;
    s.runup.angle = clamp(s.runup.angle + input.runupAngle * SP.runupAngleRate * dt, 0, SP.runupAngleMax);
    s.runup.dist = clamp(s.runup.dist + input.runupDist * SP.runupDistRate * dt, SP.runupDistMin, SP.runupDistMax);
  }

  if (s.phase === 'aim') {
    if (edge(input.toggleFoot, s.prev.toggleFoot)) s.foot = s.foot === 'R' ? 'L' : 'R';
    if (edge(input.newSpot, s.prev.newSpot) && s.mode !== 'challenge') newSpot(s);
    else if (edge(input.shoot, s.prev.shoot)) {
      s.phase = 'charge';
      s.phaseT = 0;
      s.meter = 0;
    }
  } else if (s.phase === 'charge') {
    s.meter = Math.min(1, s.meter + dt / SP.chargeTime);
    if (!input.shoot) {
      s.pending = computeShot(shotRequest(s, s.meter, true), P, SP);
      s.phase = 'runup';
      s.phaseT = 0;
      s.runupT = 0;
      // A longer run-up takes longer; a dummy runner adds a beat before the real strike.
      const base = SP.runupTime * (s.runup.dist / SP.runupDistDefault);
      s.dummy = input.dummy;
      s.dummyDone = false;
      s.dummyAt = base;
      s.runupDur = base + (s.dummy ? SP.dummyDelay : 0);
    }
  } else if (s.phase === 'runup') {
    s.runupT += dt;
    if (s.wall) updateWallRunup(s.wall, s.cfg.ai.wall, s.runupT);
    if (s.dummy && !s.dummyDone && s.runupT >= s.dummyAt) {
      // The decoy steps over the ball: part of the wall jumps for it, far too early.
      s.dummyDone = true;
      if (s.wall && s.settings.wallJumps) wallDummyJump(s.wall, s.cfg.ai.wall, s.rng);
    }
    if (s.wall && s.dummyDone) updateWallLift(s.wall, s.runupT - s.dummyAt);
    if (s.runupT >= s.runupDur && s.pending) {
      const shot = s.pending;
      s.ball.vel = copy(shot.vel);
      s.ball.spin = copy(shot.spin);
      s.ball.knuckle = { amp: shot.knuckle ? P.knuckleAccel : 0, phase: shot.knucklePhase };
      s.stats.attempts++;
      s.lastShotKnuckle = shot.knuckle;
      s.ball.wind = windVector(s.settings);
      s.encroached = !!s.wall && wallEncroaching(s.wall, s.cfg.ai.wall);
      if (s.wall && s.settings.wallJumps) {
        startWallJump(s.wall, s.cfg.ai.wall, s.rng, wallShouldJump(s.ball, s.spot, P, s.cfg.ai.wall), s.dummy ? SP.dummyDelay : 0);
      }
      if (s.keeper) keeperOnStrike(s.keeper, s.keeperAi, s.rng, s.dummy);
      s.events.push({ type: 'strike', speed: shot.speed });
      s.phase = 'flight';
      s.phaseT = 0;
      s.flightT = 0;
    }
  }

  if (s.phase === 'flight' || s.phase === 'result') {
    s.flightT += dt;
    // Defenders move first, then the ball is stepped against their new positions.
    if (s.wall) {
      updateWallFlight(s.wall, s.cfg.ai.wall, dt);
      updateWallLift(s.wall, s.flightT);
    }
    if (s.keeper) keeperStep(s.keeper, s.flightT, s.ball, P, s.keeperAi, s.rng);

    const ev: CollisionEvent[] = [];
    stepWorld(s.ball, P, dt, s.netActive, s.wall, s.keeper, s.keeperAi, ev);
    if (s.crossing === null && s.prevBallPos.z > 0 && s.ball.pos.z <= 0) {
      const f = s.prevBallPos.z / (s.prevBallPos.z - s.ball.pos.z);
      s.crossing = { x: lerp(s.prevBallPos.x, s.ball.pos.x, f), y: lerp(s.prevBallPos.y, s.ball.pos.y, f) };
    }
    for (const e of ev) {
      if (e.type === 'post' || e.type === 'bar') s.woodwork = e.type;
      if (e.type === 'wall') {
        s.hitWall = true;
        if (s.deflectT === null) s.deflectT = s.flightT;
      }
      if (e.type === 'keeper' && s.phase === 'flight') onKeeperContact(s, e);
      s.events.push(e);
    }
  }

  if (s.phase === 'flight') {
    const b = s.ball;
    if (b.pos.z < -P.radius) classifyCrossing(s);
    else {
      const speed = Math.hypot(b.vel.x, b.vel.y, b.vel.z);
      const settled = b.pos.y <= P.radius + 0.01 && speed < 0.4;
      const gone = Math.abs(b.pos.x) > 60 || b.pos.z > 70;
      const wait = s.keeperTouch ? s.keeperAi.afterTouchWait : s.cfg.ai.wall.afterHitWait;
      const deflectDone = s.deflectT !== null && s.flightT - s.deflectT > wait;
      if (settled || gone || deflectDone || s.flightT > 7) finish(s, 'miss');
    }
  } else if (s.phase === 'result') {
    const timeUp = s.phaseT > SP.resultTime;
    const ch = s.challenge;
    if (ch && ch.done) {
      // The run is over: the summary stays up until you restart (R) or move on (N).
      if (edge(input.retry, s.prev.retry)) startChallenge(s, ch.index);
      else if (edge(input.newSpot, s.prev.newSpot)) startChallenge(s, ch.index + 1);
    } else if (edge(input.retry, s.prev.retry)) resetAttempt(s);
    else if (edge(input.newSpot, s.prev.newSpot) && s.mode !== 'challenge') newSpot(s);
    else if (timeUp) {
      if (s.result?.type === 'retake' || s.mode === 'challenge') resetAttempt(s); // same spot
      else if (s.result?.rebound && reboundAvailable(s)) startFollowUp(s);
      else if (s.mode === 'practice') resetAttempt(s);
      else newSpot(s);
    }
  }

  if (s.phase === 'aim' || s.phase === 'charge') updatePreview(s);
  s.prev = { ...input };
}
```

Supporting routines that `step` calls (their order of RNG draws is significant):

```ts
export function setSpot(s: SimState, x: number, z: number, followUp = false): void {
  s.followUp = followUp;
  s.reboundCount = followUp ? s.reboundCount + 1 : 0;
  s.spot = v3(x, 0, z);
  s.ball = newBall({ x, y: s.cfg.physics.radius, z });
  s.prevBallPos = copy(s.ball.pos);
  s.spotId++;
  resetAttempt(s);
}

function resetAttempt(s: SimState): void {
  const r = s.cfg.physics.radius;
  s.ball = newBall({ x: s.spot.x, y: r, z: s.spot.z });
  s.prevBallPos = copy(s.ball.pos);
  s.phase = 'aim';
  s.phaseT = 0;
  s.meter = 0;
  s.pending = null;
  s.result = null;
  s.woodwork = null;
  s.hitWall = false;
  s.keeperTouch = null;
  s.deflectT = null;
  s.encroached = false;
  s.dummy = false;
  s.dummyDone = false;
  s.crossing = null;
  s.netActive = false;
  s.flightT = 0;
  s.keeperAi = effectiveKeeperAi(s.cfg.ai, s.settings.keeperType);
  s.rolls = rollWall(s.rng, s.settings, s.cfg.ai.wall);
  s.ball.wind = windVector(s.settings);
  layoutDefenders(s);
  s.previewKey = '';
  updatePreview(s);
}

function layoutDefenders(s: SimState): void {
  // No wall on a rebound: it is a scramble in the box.
  s.wall = s.followUp ? null : layoutWall(s.spot, s.settings, s.cfg.ai.wall, s.rolls);
  s.keeper = layoutKeeper(s.spot, s.settings, s.keeperAi);
}

export function applySettings(s: SimState): void {
  if (s.phase === 'aim' || s.phase === 'charge') {
    s.keeperAi = effectiveKeeperAi(s.cfg.ai, s.settings.keeperType);
    s.rolls = rollWall(s.rng, s.settings, s.cfg.ai.wall);
    s.ball.wind = windVector(s.settings);
    layoutDefenders(s);
    s.previewKey = '';
  }
}

function classifyCrossing(s: SimState): void {
  const { x, y } = s.ball.pos;
  const inFrame = Math.abs(x) < GOAL_HALF_WIDTH && y < GOAL_HEIGHT;
  if (inFrame) {
    s.netActive = true;
    finish(s, 'goal');
  } else if (y >= GOAL_HEIGHT) {
    finish(s, 'over');
  } else {
    finish(s, 'wide');
  }
}

function reboundAvailable(s: SimState): boolean {
  const ai = s.keeperAi;
  const b = s.ball;
  const speed = Math.hypot(b.vel.x, b.vel.y, b.vel.z);
  return (
    s.mode !== 'challenge' &&
    s.settings.rebounds &&
    s.settings.keeperPresent &&
    s.reboundCount < ai.maxRebounds &&
    Math.abs(b.pos.x) <= ai.reboundBoxX &&
    b.pos.z >= ai.reboundZMin &&
    b.pos.z <= ai.reboundZMax &&
    speed < ai.reboundMaxSpeed
  );
}

function finish(s: SimState, type: ResultType): void {
  s.phase = 'result';
  s.phaseT = 0;
  if (s.encroached && type !== 'goal') {
    // The wall was off the line: no penalty for the taker. The attempt does not count; he retakes.
    type = 'retake';
    s.stats.attempts = Math.max(0, s.stats.attempts - 1);
  } else if (s.keeperTouch && type !== 'goal') {
    // Anything the keeper got a touch to and that did not go in counts as a save
    // (tipped over, pushed wide, held…).
    type = 'saved';
  }
  const rebound = type === 'saved' && s.keeperTouch === 'touch' && reboundAvailable(s);
  let points = 0;
  let ring = -1;
  const ch = s.challenge;
  if (ch && !ch.done && type !== 'retake') {
    ({ points, ring } = scoreShot(ch.def, s.crossing, type));
    ch.taken++;
    ch.points += points;
    ch.log.push({ type, points, ring });
    if (ch.taken >= ch.def.shots) {
      ch.done = true;
      ch.medal = medalFor(ch.def, ch.points);
    }
  }
  s.result = { type, woodwork: s.woodwork, wall: s.hitWall, keeper: s.keeperTouch, rebound, points, ring, crossing: s.crossing };
  if (type === 'goal') {
    s.stats.goals++;
    s.events.push({ type: 'goal' });
  }
  s.events.push({ type: 'result', result: type });
}

function startFollowUp(s: SimState): void {
  const ai = s.keeperAi;
  const x = clamp(s.ball.pos.x, -ai.reboundBoxX, ai.reboundBoxX);
  const z = clamp(s.ball.pos.z, 2.5, ai.reboundZMax);
  setSpot(s, x, z, true);
}

function onKeeperContact(s: SimState, e: CollisionEvent): void {
  if (s.keeperTouch || !s.keeper) return;
  const ai = s.keeperAi;
  s.deflectT = s.flightT;
  const holdMax =
    lerp(ai.holdSpeedMin, ai.holdSpeedMax, s.keeper.skill) * (s.lastShotKnuckle ? ai.knuckleHoldFactor : 1);
  if (e.hands && (e.preSpeed ?? Infinity) < holdMax) {
    // Caught: the ball drops dead at his hands.
    s.keeperTouch = 'hold';
    s.ball.vel = v3(0, -0.5, 0.3);
    s.ball.spin = v3();
    finish(s, 'saved');
  } else {
    s.keeperTouch = 'touch';
  }
}
```

```ts
export function createSim(cfg: SimConfig, seed = 1234): SimState {
  const taker = cfg.takers[0];
  const spot = v3(-6, 0, 24);
  const s: SimState = {
    cfg,
    rng: { s: seed },
    phase: 'aim',
    phaseT: 0,
    taker,
    foot: taker.foot,
    spot,
    spotId: 0,
    ball: newBall({ x: spot.x, y: cfg.physics.radius, z: spot.z }),
    prevBallPos: v3(),
    aim: { x: 0, y: 1.2 },
    contact: { x: 0, y: 0 },
    meter: 0,
    driven: false,
    low: false,
    pending: null,
    runupT: 0,
    preview: null,
    previewKey: '',
    showPath: false,
    mode: 'free',
    challenge: null,
    takerIndex: 0,
    difficulty: 'normal',
    accuracyBonus: 0,
    dummy: false,
    dummyAt: 0,
    dummyDone: false,
    crossing: null,
    runup: { angle: cfg.shot.runupAngleDefault, dist: cfg.shot.runupDistDefault },
    runupDur: cfg.shot.runupTime,
    settings: { ...defaultSettings(), ...cfg.settings },
    keeperAi: cfg.ai.keeper,
    rolls: noRolls(),
    encroached: false,
    followUp: false,
    reboundCount: 0,
    wall: null,
    keeper: null,
    result: null,
    woodwork: null,
    hitWall: false,
    keeperTouch: null,
    deflectT: null,
    lastShotKnuckle: false,
    netActive: false,
    flightT: 0,
    stats: { attempts: 0, goals: 0 },
    prev: emptyInput(),
    events: [],
  };
  s.prevBallPos = copy(s.ball.pos);
  s.keeperAi = effectiveKeeperAi(cfg.ai, s.settings.keeperType);
  s.rolls = rollWall(s.rng, s.settings, cfg.ai.wall);
  s.ball.wind = windVector(s.settings);
  layoutDefenders(s);
  updatePreview(s);
  return s;
}

export function selectTaker(s: SimState, index: number): void {
  const n = s.cfg.takers.length;
  s.takerIndex = ((index % n) + n) % n;
  s.taker = s.cfg.takers[s.takerIndex];
  s.foot = s.taker.foot;
  s.previewKey = '';
}

export function applyPreset(s: SimState, name: DifficultyName): void {
  const p = s.cfg.presets[name];
  Object.assign(s.settings, sanitizeSettings({ ...s.settings, ...p.settings }));
  s.accuracyBonus = p.accuracyBonus;
  s.difficulty = name;
  applySettings(s);
}

export function startChallenge(s: SimState, index: number): void {
  const defs = s.cfg.challenges;
  const i = ((index % defs.length) + defs.length) % defs.length;
  const def = defs[i];
  const backup = s.challenge ? s.challenge.backup : { ...s.settings };
  if (s.challenge) Object.assign(s.settings, backup);
  Object.assign(s.settings, def.settings);
  s.mode = 'challenge';
  s.challenge = { index: i, def, taken: 0, points: 0, log: [], done: false, medal: null, backup };
  setSpot(s, def.spot.x, def.spot.z);
}

export function setMode(s: SimState, mode: Mode, challengeIndex = 0): void {
  if (mode === 'challenge') {
    startChallenge(s, challengeIndex);
    return;
  }
  endChallenge(s);
  s.mode = mode;
  resetAttempt(s);
}

export function randomSpot(s: SimState): { x: number; z: number } {
  for (let i = 0; i < 50; i++) {
    const d = rngRange(s.rng, 19, 32);
    const a = rngRange(s.rng, -0.75, 0.75);
    const x = d * Math.sin(a);
    const z = d * Math.cos(a);
    // Free kicks here are always outside the box.
    if (z > PENALTY_BOX_Z + 1.5 || Math.abs(x) > PENALTY_BOX_HALF + 1.5) return { x, z };
  }
  return { x: 0, z: 25 };
}

function newSpot(s: SimState): void {
  const p = randomSpot(s);
  setSpot(s, p.x, p.z);
}
```

## 7. Function inventory

Every exported function of the sim, per module (the full source is the specification).

**`BallPhysics.ts`**
```ts
export function newBall(pos: V3): BallState
export function dragCoefficient(speed: number, P: PhysicsParams): number
export function acceleration(vel: V3, spin: V3, t: number, k: Knuckle, P: PhysicsParams, wind?: V3): V3
export function stepBall(b: BallState, P: PhysicsParams, dt: number): void
export function groundContact(b: BallState, P: PhysicsParams, dt: number): number
```

**`Collisions.ts`**
```ts
export function stepWorld(
```

**`Prediction.ts`**
```ts
export function predictAtPlane(
export function predictAtDistance(
```

**`ShotModel.ts`**
```ts
export function runupEffect(runup: { angle: number; dist: number } | undefined, SP: ShotParams): RunupEffect
export function launchSpeed(
export function computeShot(req: ShotRequest, P: PhysicsParams, SP: ShotParams, wantPath = false): Shot
```

**`Wall.ts`**
```ts
export const WALL_PLAYER_RADIUS = 0.23;
export const WALL_SPACING = 0.47;
export const noRolls = (): WallRolls => ({ lyingMan: false, encroach: null });
export function rollWall(rng: RngState, settings: SimSettings, ai: WallAi): WallRolls
export function autoWallCount(spot: V3): number
export function layoutWall(spot: V3, settings: SimSettings, ai?: WallAi, rolls: WallRolls = noRolls()): WallState | null
export function syncWallPositions(wall: WallState): void
export function updateWallRunup(wall: WallState, ai: WallAi, t: number): void
export function wallEncroaching(wall: WallState, ai: WallAi): boolean
export function updateWallFlight(wall: WallState, ai: WallAi, dt: number): void
export function wallShouldJump(ball: BallState, spot: V3, P: PhysicsParams, ai: WallAi): boolean
export function wallDummyJump(wall: WallState, ai: WallAi, rng: RngState): void
export function startWallJump(wall: WallState, ai: WallAi, rng: RngState, shouldJump = true, sinceDummy = 0): void
export function updateWallLift(wall: WallState, t: number): void
```

**`Keeper.ts`**
```ts
export function layoutKeeper(spot: V3, settings: SimSettings, ai: KeeperAi): KeeperState | null
export function keeperArmsOut(k: KeeperState): boolean
export function keeperArms(k: KeeperState, ai: KeeperAi): { a: V3; b: V3 }
export function keeperOnStrike(k: KeeperState, ai: KeeperAi, rng: RngState, dummy = false): void
export function keeperStep(
```

**`layout.ts`**
```ts
export function dirToGoal(spot: V3): V3
export function rightOf(dir: V3): V3
export function kickerPositions(
export const origin = (): V3 => v3();
```

**`ai.ts`**
```ts
export function effectiveKeeperAi(ai: AiParams, type: KeeperType): KeeperAi
```

**`settings.ts`**
```ts
export const FREQUENCIES: Frequency[] = ['off', 'sometimes', 'always'];
export const KEEPER_TYPES: KeeperType[] = ['balanced', 'quick', 'tall', 'strong'];
export const WALL_HEIGHT_MIN = 1.6;
export const WALL_HEIGHT_MAX = 2.1;
export const WALL_COUNT_MAX = 6;
export const WIND_SPEED_MAX = 15;
export const defaultSettings = (): SimSettings => (
export function sanitizeSettings(raw: Partial<SimSettings>): SimSettings
export function windVector(s: SimSettings): { x: number; y: number; z: number }
```

**`challenge.ts`**
```ts
export function scoreShot(
export function medalFor(def: ChallengeDef, points: number): Medal | null
```

## 8. Verification: the golden fixtures

`tests/golden/*.json` are produced by the TypeScript sim (`npm run golden`). Each file holds one scenario, the full
resolved settings, a sampled trace and the final state. A port must replay the scenario with the *driver* below and
reproduce the file.

**Driver** (the order matters because several calls draw from the RNG):

1. `s = createSim(cfg, seed)` with `cfg.settings = fixture.settings` and the shared `data/*.json`.
2. If `scenario.challenge` is set: `startChallenge(s, scenario.challenge)` (it sets the spot itself).
3. `selectTaker(s, scenario.taker ?? 0)`; if `scenario.foot`, set `s.foot`.
4. Unless it is a challenge: `setSpot(s, spot.x, spot.z)`.
5. `s.aim = aim`, `s.contact = contact`; if given, `s.runup = runup`.
6. Inputs, with `extra = { driven, low, contactX: contact.x, contactY: contact.y }`:
   - `hold = emptyInput() + extra + shoot=true`, `idle = emptyInput() + extra`, `release = idle + dummy`.
   - `step(hold)` once, then `step(hold)` until `s.meter >= power` (so `inputs.holdSteps` ticks), then `step(release)` once,
     then `step(idle)` until `s.phase == 'result'` or 10 s.
   - *Forced* scenarios (`force`) skip the strike: set `phase = 'flight'`, the keeper (if any) to `phase = 'watching'`, and the ball
     position/velocity (also `prevBallPos`), then `step(idle)` until the result.
   - `continueAfterResult`: keep stepping `idle` until the phase leaves `'result'` (at most 20 s in total).
7. Every 24th tick record a sample; at the end record `final` (`atResult` is the result and stats at the moment the attempt was decided).

**Tolerances** a port must meet: all strings, booleans, integers and the `rng` state **exactly**; every sampled position
within **1 cm** (and velocities/spins within 1e-3 relative) along the whole flight. The TypeScript self-check uses 1e-6.

## 9. Changing the simulation

Any change to `src/sim/` (or to `data/*.json`) must be made in the TypeScript and the C++ in the same change, followed by
`npm run golden` and `npm run contract`; CI fails if either committed file is stale or the native build disagrees.
