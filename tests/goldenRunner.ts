import { SimSettings, sanitizeSettings } from '../src/sim/settings';
import { InputState, SimConfig, createSim, emptyInput, selectTaker, setSpot, startChallenge, step } from '../src/sim/Sim';
import { AI, CHALLENGES, DT, P, PRESETS, SP, TAKERS } from './helpers';

/**
 * A golden scenario is a small, fully deterministic script that any implementation of the
 * simulation (the TypeScript one, or the native port) can replay. See docs/porting-contract.md.
 */
export interface Scenario {
  name: string;
  desc: string;
  seed: number;
  /** Only the fields that differ from BASE_SETTINGS (everything else off). */
  settings: Partial<SimSettings>;
  taker?: number;
  foot?: 'R' | 'L';
  spot: { x: number; z: number };
  aim: { x: number; y: number };
  contact: { x: number; y: number };
  runup?: { angle: number; dist: number };
  driven?: boolean;
  low?: boolean;
  dummy?: boolean;
  /** Power meter value at the release. */
  power: number;
  /** Skip the strike: start already in flight with this ball state (keeper stands still). */
  force?: { ball: { x: number; y: number; z: number }; vel: { x: number; y: number; z: number } };
  /** Keep stepping after the result until the next attempt is set up (rebounds, retakes). */
  continueAfterResult?: boolean;
  /** Start this challenge (its rules override `settings`). */
  challenge?: number;
}

/** Everything switched off, so each scenario only turns on what it tests. */
export const BASE_SETTINGS: Partial<SimSettings> = {
  wallEnabled: false,
  wallJumps: false,
  keeperPresent: false,
  lyingMan: 'off',
  encroachment: 'off',
  rebounds: false,
  windEnabled: false,
};

export const SAMPLE_EVERY = 24; // steps (= 10 Hz at 240 Hz)

export interface Sample {
  step: number;
  phase: string;
  ball: number[];
  vel: number[];
  spin: number[];
  keeperHand: number[] | null;
  wallLiftMax: number | null;
}

export interface Fixture {
  scenario: Scenario;
  /** The full resolved settings used (so a port need not know BASE_SETTINGS). */
  settings: SimSettings;
  inputs: { holdSteps: number; releaseStep: number; totalSteps: number };
  samples: Sample[];
  final: Record<string, unknown>;
}

const v = (a: { x: number; y: number; z: number }) => [a.x, a.y, a.z];

export function resolveSettings(sc: Scenario): SimSettings {
  return sanitizeSettings({ ...BASE_SETTINGS, ...sc.settings } as Partial<SimSettings>);
}

/**
 * The driver. The order of operations matters because several of them draw from the RNG:
 *   createSim(seed) -> [startChallenge] -> selectTaker -> foot -> setSpot -> aim/contact/runup
 *   -> hold shoot until meter >= power -> release (with dummy) -> idle until the result.
 */
export function runScenario(sc: Scenario): Fixture {
  const settings = resolveSettings(sc);
  const cfg: SimConfig = {
    physics: P,
    shot: SP,
    ai: AI,
    takers: TAKERS.map((t) => ({ ...t })),
    challenges: CHALLENGES,
    presets: PRESETS,
    settings,
  };
  const s = createSim(cfg, sc.seed);
  if (sc.challenge !== undefined) startChallenge(s, sc.challenge);
  selectTaker(s, sc.taker ?? 0);
  if (sc.foot) s.foot = sc.foot;
  if (sc.challenge === undefined) setSpot(s, sc.spot.x, sc.spot.z);
  s.aim = { ...sc.aim };
  s.contact = { ...sc.contact };
  if (sc.runup) s.runup = { ...sc.runup };

  const extra = { driven: !!sc.driven, low: !!sc.low, contactX: sc.contact.x, contactY: sc.contact.y };
  const hold: InputState = { ...emptyInput(), ...extra, shoot: true };
  const idle: InputState = { ...emptyInput(), ...extra };
  const release: InputState = { ...idle, dummy: !!sc.dummy };

  const samples: Sample[] = [];
  let n = 0;
  const run = (input: InputState) => {
    step(s, input, DT);
    n++;
    if (n % SAMPLE_EVERY === 0) {
      samples.push({
        step: n,
        phase: s.phase,
        ball: v(s.ball.pos),
        vel: v(s.ball.vel),
        spin: v(s.ball.spin),
        keeperHand: s.keeper ? [s.keeper.b.x, s.keeper.b.y] : null,
        wallLiftMax: s.wall ? Math.max(...s.wall.lift) : null,
      });
    }
  };

  let holdSteps = 0;
  let releaseStep = 0;
  if (sc.force) {
    s.phase = 'flight';
    if (s.keeper) s.keeper.phase = 'watching';
    s.ball.pos = { ...sc.force.ball };
    s.ball.vel = { ...sc.force.vel };
    s.prevBallPos = { ...sc.force.ball };
  } else {
    run(hold);
    while (s.meter < sc.power) run(hold);
    holdSteps = n;
    run(release);
    releaseStep = n;
  }
  while (n < 240 * 10 && s.phase !== 'result') run(idle);
  const atResult = JSON.parse(JSON.stringify({ result: s.result, stats: s.stats }));
  if (sc.continueAfterResult) while (n < 240 * 20 && s.phase === 'result') run(idle);

  const final = JSON.parse(
    JSON.stringify({
      phase: s.phase,
      /** The attempt's result and the stats at the moment it was decided (before any follow-on). */
      atResult,
      result: s.result,
      stats: s.stats,
      rng: s.rng.s,
      ball: { pos: v(s.ball.pos), vel: v(s.ball.vel), spin: v(s.ball.spin) },
      flightT: s.flightT,
      encroached: s.encroached,
      crossing: s.crossing,
      spot: v(s.spot),
      spotId: s.spotId,
      followUp: s.followUp,
      reboundCount: s.reboundCount,
      keeperTouch: s.keeperTouch,
      woodwork: s.woodwork,
      hitWall: s.hitWall,
      challenge: s.challenge && { taken: s.challenge.taken, points: s.challenge.points, done: s.challenge.done, medal: s.challenge.medal },
    }),
  );
  return { scenario: sc, settings, inputs: { holdSteps, releaseStep, totalSteps: n }, samples, final };
}

/** Deep-compare two JSON values; numbers may differ by `tol` (absolute). Returns the first difference or null. */
export function firstDiff(a: unknown, b: unknown, tol = 1e-6, path = '$'): string | null {
  if (typeof a === 'number' && typeof b === 'number') {
    return Math.abs(a - b) <= tol ? null : `${path}: ${a} vs ${b}`;
  }
  if (a === null || b === null || typeof a !== 'object' || typeof b !== 'object') {
    return a === b ? null : `${path}: ${JSON.stringify(a)} vs ${JSON.stringify(b)}`;
  }
  if (Array.isArray(a) !== Array.isArray(b)) return `${path}: array mismatch`;
  const ka = Object.keys(a as object);
  const kb = Object.keys(b as object);
  if (ka.length !== kb.length) return `${path}: keys ${ka.join(',')} vs ${kb.join(',')}`;
  for (const k of ka) {
    const d = firstDiff((a as Record<string, unknown>)[k], (b as Record<string, unknown>)[k], tol, `${path}.${k}`);
    if (d) return d;
  }
  return null;
}
