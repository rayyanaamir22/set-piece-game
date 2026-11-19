import physics from '../data/physics.json';
import shot from '../data/shot.json';
import ai from '../data/ai.json';
import challenges from '../data/challenges.json';
import presets from '../data/presets.json';
import takers from '../data/takers.json';
import { PhysicsParams, newBall, stepBall } from '../src/sim/BallPhysics';
import { AiParams } from '../src/sim/ai';
import { ChallengeDef } from '../src/sim/challenge';
import { DifficultyName, DifficultyPreset } from '../src/sim/presets';
import { SimSettings } from '../src/sim/settings';
import { InputState, SimState, emptyInput, step } from '../src/sim/Sim';
import { ShotParams, Taker } from '../src/sim/ShotModel';
import { SimConfig } from '../src/sim/Sim';
import { V3 } from '../src/sim/math/vec3';

export const P = physics as PhysicsParams;
export const SP = shot as ShotParams;
export const TAKERS = takers as Taker[];
export const AI = ai as AiParams;
export const CHALLENGES = challenges as ChallengeDef[];
export const PRESETS = presets as Record<DifficultyName, DifficultyPreset>;

/** Build a sim config. Defenders are off unless a test asks for them, so each test isolates one thing. */
export function makeCfg(settings: Partial<SimSettings> = {}, aiOverride: Partial<{ wall: Partial<AiParams['wall']>; keeper: Partial<AiParams['keeper']> }> = {}): SimConfig {
  return {
    physics: P,
    shot: SP,
    takers: TAKERS.map((t) => ({ ...t })),
    challenges: CHALLENGES,
    presets: PRESETS,
    ai: { wall: { ...AI.wall, ...aiOverride.wall }, keeper: { ...AI.keeper, ...aiOverride.keeper }, keeperPresets: AI.keeperPresets },
    settings: {
      wallEnabled: false,
      wallJumps: false,
      keeperPresent: false,
      // Extras off by default so each test isolates one behaviour.
      lyingMan: 'off',
      encroachment: 'off',
      rebounds: false,
      ...settings,
    },
  };
}

export const CFG: SimConfig = makeCfg();
export const CFG_WALL: SimConfig = makeCfg({ wallEnabled: true });

/** Free flight (no ground/posts) until z <= 0, returns samples. */
export function fly(start: V3, vel: V3, spin: V3, params: PhysicsParams = P, knuckle?: { amp: number; phase: number[] }, wind?: V3) {
  const b = newBall(start);
  if (wind) b.wind = { ...wind };
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

export const DT = 1 / 240;
export const sweet = (SP.sweetMin + SP.sweetMax) / 2;



/** Hold shoot until the meter reaches `power`, release, and play until the attempt has a result. */
export function takeShot(s: SimState, power: number, extra: Partial<InputState> = {}) {
  const hold: InputState = { ...emptyInput(), ...extra, shoot: true };
  const idle: InputState = { ...emptyInput(), ...extra };
  step(s, hold, DT);
  while (s.meter < power) step(s, hold, DT);
  step(s, idle, DT);
  for (let i = 0; i < 240 * 9 && s.phase !== 'result'; i++) step(s, idle, DT);
  return s.result;
}
