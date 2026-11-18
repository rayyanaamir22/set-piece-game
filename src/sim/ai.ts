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
  /** Time for a diving keeper to hit the ground. */
  sinkTime: number;
}

export interface AiParams {
  wall: WallAi;
  keeper: KeeperAi;
}
