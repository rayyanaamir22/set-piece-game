import { KeeperType } from './settings';

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

/** The keeper tuning actually in force: base values with the chosen type's overrides. */
export function effectiveKeeperAi(ai: AiParams, type: KeeperType): KeeperAi {
  return { ...ai.keeper, ...ai.keeperPresets[type] };
}
