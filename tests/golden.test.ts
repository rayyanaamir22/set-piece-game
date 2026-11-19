import { mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { Fixture, Scenario, firstDiff, runScenario } from './goldenRunner';
import { sweet } from './helpers';

const DIR = join(__dirname, 'golden');
const UPDATE = !!process.env.UPDATE_GOLDEN;

const S = (name: string, desc: string, o: Partial<Scenario> & Pick<Scenario, 'aim'>): Scenario => ({
  name,
  desc,
  seed: 42,
  settings: {},
  spot: { x: -6, z: 24 },
  contact: { x: 0, y: 0 },
  power: sweet,
  ...o,
});

const wall = { wallEnabled: true, wallCount: 4 };
const keeper = { keeperPresent: true, keeperSkill: 0.7 };

export const SCENARIOS: Scenario[] = [
  // ---- flight physics
  S('flight-plain', 'No spin: a straight parabola with drag (power too low for a knuckle).', { aim: { x: 1, y: 1.4 }, power: 0.6 }),
  S('flight-curl-right-contact', 'Strike on the right of the ball: curls left.', { aim: { x: 1.5, y: 1.5 }, contact: { x: 0.8, y: 0 } }),
  S('flight-curl-left-contact', 'Strike on the left of the ball: curls right.', { aim: { x: -1.5, y: 1.5 }, contact: { x: -0.8, y: 0 } }),
  S('flight-dip', 'Topspin: launches high and drops onto the target.', { aim: { x: 0.5, y: 1.2 }, contact: { x: 0.2, y: 0.9 } }),
  S('flight-lift', 'Backspin: a flatter, floating flight.', { aim: { x: 0.5, y: 1.8 }, contact: { x: 0.2, y: -0.8 } }),
  S('flight-knuckle', 'Centre contact at high power: the seeded late wobble.', { aim: { x: 0.5, y: 1.4 }, power: sweet }),
  S('flight-driven', 'Driven shot: faster, flatter, less spin.', { aim: { x: 1.5, y: 1.4 }, contact: { x: 0.6, y: 0 }, driven: true }),
  S('flight-low-drive', 'Low drive: skids along the grass.', { aim: { x: 2, y: 0.3 }, contact: { x: 0.3, y: 0 }, low: true }),
  S('wind-crosswind', 'A 9 m/s crosswind pushes the shot sideways (the aim solver ignores wind).', { aim: { x: 0, y: 1.3 }, contact: { x: 0.3, y: 0 }, settings: { windEnabled: true, windSpeed: 9, windDirection: 90 } }),
  S('wind-headwind', 'Headwind slows the ball (wind blowing away from goal).', { aim: { x: 0, y: 1.3 }, contact: { x: 0.3, y: 0 }, settings: { windEnabled: true, windSpeed: 8, windDirection: 180 } }),
  S('wind-tailwind', 'Tailwind speeds the ball up.', { aim: { x: 0, y: 1.3 }, contact: { x: 0.3, y: 0 }, settings: { windEnabled: true, windSpeed: 8, windDirection: 0 } }),
  // ---- run-up
  S('runup-straight-long', 'Straight, long run-up: more power, less curl.', { aim: { x: 1.5, y: 1.4 }, contact: { x: 0.7, y: 0 }, runup: { angle: 0, dist: 6 } }),
  S('runup-wide-short', 'Wide, short run-up: less power, more curl, no knuckle.', { aim: { x: 1.5, y: 1.4 }, contact: { x: 0.7, y: 0 }, runup: { angle: 75, dist: 2 } }),
  // ---- woodwork and the net
  S('hits-post', 'Rattles the post and goes in (aim found by search).', { spot: { x: 4, z: 22 }, aim: { x: 3.64, y: 1.0 }, contact: { x: 0.3, y: 0 } }),
  S('hits-bar', 'Hits the crossbar and goes in (aim found by search).', { spot: { x: 4, z: 22 }, aim: { x: 0.5, y: 2.36 }, contact: { x: 0.3, y: 0 } }),
  S('wide', 'Aimed outside the post.', { aim: { x: 6, y: 1 } }),
  S('over', 'Aimed above the bar.', { aim: { x: 0, y: 3.8 } }),
  // ---- the wall
  S('wall-blocks-flat', 'A flat shot into the covered side hits the wall.', { aim: { x: -1, y: 1.2 }, contact: { x: 0.6, y: 0 }, settings: wall }),
  S('wall-dip-clears', 'A dip over a standing wall scores.', { aim: { x: -1, y: 2.0 }, contact: { x: 0.3, y: 0.8 }, settings: wall }),
  S('wall-jump-blocks-dip', 'The same dip is blocked by a jumping wall.', { aim: { x: -1, y: 2.0 }, contact: { x: 0.3, y: 0.8 }, settings: { ...wall, wallJumps: true } }),
  S('wall-dummy-run', 'A dummy run-up: the wall jumps early and the dip gets through.', { aim: { x: -1, y: 2.0 }, contact: { x: 0.3, y: 0.8 }, dummy: true, settings: { ...wall, wallJumps: true } }),
  S('lying-man-blocks-grounder', 'A low drive slides under the jumpers but the lying man stops it.', { aim: { x: -0.5, y: 0.1 }, low: true, settings: { ...wall, wallJumps: true, lyingMan: 'always' } }),
  S('encroach-retake', 'A creeping wall player makes a missed shot a retake.', { aim: { x: 6, y: 1 }, settings: { ...wall, encroachment: 'always' }, continueAfterResult: true }),
  S('encroach-goal-stands', 'The same offence, but the goal stands.', { aim: { x: 3, y: 1.8 }, contact: { x: 0.5, y: 0.2 }, settings: { ...wall, encroachment: 'always' } }),
  // ---- the keeper
  S('keeper-save-central', 'A central shot at mid height is saved.', { spot: { x: 0, z: 22 }, aim: { x: 0.3, y: 1.0 }, contact: { x: 0.3, y: 0 }, settings: { ...keeper, keeperSkill: 1 } }),
  S('keeper-beaten-corner', 'A top-corner dip beats him.', { aim: { x: 3.2, y: 2.2 }, contact: { x: 0.6, y: 0.3 }, settings: keeper }),
  S('keeper-holds', 'A gentle ball at his chest is held.', { spot: { x: 0, z: 24 }, aim: { x: 0, y: 1 }, power: 0, settings: keeper, force: { ball: { x: 0, y: 1.3, z: 2 }, vel: { x: 0, y: 0, z: -9 } } }),
  S('keeper-parry-rebound', 'A hard shot is parried back into the box: a rebound follow-up.', { spot: { x: 0, z: 24 }, aim: { x: 0, y: 1 }, power: 0, settings: { ...keeper, rebounds: true }, force: { ball: { x: 0, y: 1.3, z: 2 }, vel: { x: 0, y: 0, z: -28 } }, continueAfterResult: true }),
  // ---- modes
  S('challenge-top-bins', 'Challenge 1: a clean shot into the right-hand ring scores 4.', { aim: { x: 3.0, y: 1.85 }, contact: { x: 0.4, y: 0.2 }, challenge: 0 }),
];

const file = (sc: Scenario) => join(DIR, `${sc.name}.json`);

if (UPDATE) {
  mkdirSync(DIR, { recursive: true });
  for (const f of readdirSync(DIR)) if (f.endsWith('.json') && !SCENARIOS.some((s) => `${s.name}.json` === f)) {
    throw new Error(`stale fixture ${f}: delete it or add its scenario back`);
  }
  for (const sc of SCENARIOS) writeFileSync(file(sc), JSON.stringify(runScenario(sc), null, 1) + '\n');
}

describe('Golden fixtures (the contract the native port must reproduce)', () => {
  it('one fixture per scenario, with unique names', () => {
    expect(new Set(SCENARIOS.map((s) => s.name)).size).toBe(SCENARIOS.length);
    const files = readdirSync(DIR).filter((f) => f.endsWith('.json')).sort();
    expect(files).toEqual(SCENARIOS.map((s) => `${s.name}.json`).sort());
  });

  it.each(SCENARIOS.map((s) => [s.name, s] as const))('%s replays to the recorded result', (_n, sc) => {
    const stored = JSON.parse(readFileSync(file(sc), 'utf8')) as Fixture;
    const now = JSON.parse(JSON.stringify(runScenario(sc))) as Fixture;
    expect(firstDiff(now, stored)).toBeNull();
  });

  // The scenarios are only useful if they exercise what they claim to.
  const fx = (name: string) => JSON.parse(readFileSync(file(SCENARIOS.find((s) => s.name === name)!), 'utf8')) as Fixture;
  const res = (name: string) => (fx(name).final.atResult as { result: Record<string, unknown> }).result;

  it('each scenario shows the behaviour it is named for', () => {
    expect(res('flight-plain').type).toBe('goal');
    expect(res('hits-post').woodwork).toBe('post');
    expect(res('hits-bar').woodwork).toBe('bar');
    expect(res('wide').type).toBe('wide');
    expect(res('over').type).toBe('over');
    expect(res('wall-blocks-flat').wall).toBe(true);
    expect(res('wall-dip-clears')).toMatchObject({ type: 'goal', wall: false });
    expect(res('wall-jump-blocks-dip').wall).toBe(true);
    expect(res('wall-dummy-run')).toMatchObject({ wall: false, type: 'goal' });
    expect(res('lying-man-blocks-grounder').wall).toBe(true);
    expect(res('encroach-retake').type).toBe('retake');
    expect(res('encroach-goal-stands').type).toBe('goal');
    expect(res('keeper-save-central').type).toBe('saved');
    expect(res('keeper-beaten-corner').type).toBe('goal');
    expect(res('keeper-holds')).toMatchObject({ type: 'saved', keeper: 'hold' });
    expect(res('keeper-parry-rebound')).toMatchObject({ type: 'saved', keeper: 'touch', rebound: true });
    expect(fx('keeper-parry-rebound').final.followUp).toBe(true);
    expect((fx('encroach-retake').final.atResult as { stats: unknown }).stats).toMatchObject({ attempts: 0 });
    expect(res('challenge-top-bins')).toMatchObject({ type: 'goal', points: 4 });
  });

  it('physics scenarios actually differ from each other', () => {
    const end = (n: string) => (fx(n).final.ball as { pos: number[] }).pos;
    expect(end('wind-crosswind')[0]).toBeGreaterThan(end('flight-plain')[0] - 5);
    const t = (n: string) => fx(n).final.flightT as number;
    expect(t('wind-headwind')).toBeGreaterThan(t('wind-tailwind'));
  });

  it('cover at least 25 scenarios', () => expect(SCENARIOS.length).toBeGreaterThanOrEqual(25));
});
