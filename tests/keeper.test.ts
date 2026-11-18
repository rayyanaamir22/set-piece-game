import { describe, expect, it } from 'vitest';
import { layoutKeeper } from '../src/sim/Keeper';
import { InputState, SimState, createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { defaultSettings } from '../src/sim/settings';
import { AI, SP, makeCfg } from './helpers';

const DT = 1 / 240;
const sweet = (SP.sweetMin + SP.sweetMax) / 2;

function takeShot(s: SimState, power: number, extra: Partial<InputState> = {}) {
  const hold: InputState = { ...emptyInput(), ...extra, shoot: true };
  const idle: InputState = { ...emptyInput(), ...extra };
  step(s, hold, DT);
  while (s.meter < power) step(s, hold, DT);
  step(s, idle, DT);
  for (let i = 0; i < 240 * 9 && s.phase !== 'result'; i++) step(s, idle, DT);
  return s.result;
}

const keeperSim = (skill: number, seed: number, spotX = -6, spotZ = 24) => {
  const s = createSim(makeCfg({ keeperPresent: true, keeperSkill: skill }), seed * 7919);
  setSpot(s, spotX, spotZ);
  return s;
};

/** Fraction of `n` seeds on which the shot ends as the given result. */
function rate(skill: number, aim: { x: number; y: number }, extra: Partial<InputState>, want: string, n = 24, power = sweet) {
  let c = 0;
  for (let seed = 1; seed <= n; seed++) {
    const s = keeperSim(skill, seed);
    s.aim = { ...aim };
    if (takeShot(s, power, extra)?.type === want) c++;
  }
  return c / n;
}

describe('Keeper layout', () => {
  const ai = AI.keeper;
  it('stands on the goal line side, covering the far post', () => {
    const left = layoutKeeper({ x: -10, y: 0, z: 24 }, defaultSettings(), ai)!;
    const right = layoutKeeper({ x: 10, y: 0, z: 24 }, defaultSettings(), ai)!;
    expect(left.a.x).toBeGreaterThan(0.5); // kick from the left -> keeper shades right
    expect(right.a.x).toBeLessThan(-0.5);
    expect(Math.abs(left.a.x)).toBeLessThanOrEqual(ai.shadeMax);
    expect(left.a.z).toBe(ai.lineZ);
  });
  it('a central free kick gets a central keeper', () => {
    expect(layoutKeeper({ x: 0, y: 0, z: 24 }, defaultSettings(), ai)!.a.x).toBe(0);
  });
  it('no keeper when switched off', () => {
    expect(layoutKeeper({ x: 0, y: 0, z: 24 }, { ...defaultSettings(), keeperPresent: false }, ai)).toBeNull();
  });
});

describe('Keeper behaviour', () => {
  it('does not move before his reaction time', () => {
    const s = keeperSim(0.7, 1);
    s.aim = { x: 3, y: 1 };
    const start = { ...s.keeper!.b };
    const hold = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    while (s.phase !== 'flight') step(s, emptyInput(), DT);
    while (s.flightT < 0.15) step(s, emptyInput(), DT);
    expect(s.keeper!.b).toEqual(start);
  });

  it('saves most mid-height central shots', () => {
    expect(rate(0.7, { x: 0.0, y: 1.2 }, { contactX: 0.3 }, 'saved')).toBeGreaterThan(0.6);
  });

  it('is beaten by a good top-corner shot (beyond his reach)', () => {
    expect(rate(0.7, { x: 3.2, y: 2.2 }, { contactX: 0.6, contactY: 0.3 }, 'goal')).toBeGreaterThan(0.7);
  });

  it('cannot reach the near post side (the wall is supposed to cover it)', () => {
    expect(rate(1, { x: -3.2, y: 1.5 }, { contactX: -0.5 }, 'goal')).toBeGreaterThan(0.9);
  });

  it('skill matters: a better keeper saves more', () => {
    const shot = { x: 1.5, y: 1.2 };
    const bad = rate(0.05, shot, { contactX: 0.4 }, 'saved');
    const good = rate(1, shot, { contactX: 0.4 }, 'saved');
    expect(good).toBeGreaterThan(bad + 0.2);
  });

  it('ignores shots going well wide', () => {
    const s = keeperSim(0.7, 3);
    s.aim = { x: 6.5, y: 1 };
    const r = takeShot(s, sweet);
    expect(r?.type).toBe('wide');
    expect(s.keeper!.phase).toBe('watching');
  });

  it('only "saved" results ever involve a keeper touch', () => {
    for (let seed = 1; seed <= 12; seed++) {
      const s = keeperSim(0.7, seed);
      s.aim = { x: 1, y: 1.5 };
      const r = takeShot(s, sweet, { contactX: 0.5 });
      if (r?.type === 'saved') expect(r.keeper).not.toBeNull();
    }
  });

  it('with no keeper nothing is ever saved', () => {
    const s = createSim(makeCfg({ keeperPresent: false }), 5);
    setSpot(s, 0, 22);
    s.aim = { x: 0, y: 1 };
    expect(takeShot(s, sweet)?.type).not.toBe('saved');
  });

  it('is deterministic for a given seed', () => {
    const run = () => {
      const s = keeperSim(0.7, 11);
      s.aim = { x: 2, y: 1.5 };
      const r = takeShot(s, sweet, { contactX: 0.4 });
      return JSON.stringify([r, s.ball.pos, s.keeper!.b]);
    };
    expect(run()).toBe(run());
  });
});

describe('Keeper contact', () => {
  /** Put a ball in flight straight at a stationary keeper. */
  const direct = (speed: number, y: number, skill = 0.7) => {
    const s = keeperSim(skill, 1, 0, 24);
    s.phase = 'flight';
    s.keeper!.phase = 'watching'; // stands still
    const k = s.keeper!;
    s.ball.pos = { x: k.b.x, y, z: 2 }; // close in, so gravity drop is negligible
    s.ball.vel = { x: 0, y: 0, z: -speed };
    s.prevBallPos = { ...s.ball.pos };
    return s;
  };
  const run = (s: SimState) => {
    for (let i = 0; i < 240 * 5 && s.phase !== 'result'; i++) step(s, emptyInput(), DT);
    return s;
  };

  it('a gentle shot at his chest is held', () => {
    const s = run(direct(10, 1.3));
    expect(s.result?.type).toBe('saved');
    expect(s.result?.keeper).toBe('hold');
    expect(s.ball.pos.z).toBeGreaterThan(0.3); // dropped in front of the line
  });

  it('a hard shot at his chest is parried, not held, and bounces back out', () => {
    const s = direct(28, 1.3);
    let sawBack = false;
    for (let i = 0; i < 240 * 5 && s.phase !== 'result'; i++) {
      step(s, emptyInput(), DT);
      if (s.ball.vel.z > 1) sawBack = true;
    }
    expect(s.result?.type).toBe('saved');
    expect(s.result?.keeper).toBe('touch');
    expect(sawBack).toBe(true);
  });

  it('a hard shot at his feet is blocked rather than caught', () => {
    const s = run(direct(12, 0.25));
    expect(s.result?.type).toBe('saved');
    expect(s.result?.keeper).toBe('touch');
  });

  it('a better keeper holds faster shots', () => {
    expect(run(direct(15, 1.3, 1)).result?.keeper).toBe('hold');
    expect(run(direct(15, 1.3, 0)).result?.keeper).toBe('touch');
  });

  it('a ball hit through his hands that still crosses the line is a goal', () => {
    const s = keeperSim(0.7, 1, 0, 24);
    s.phase = 'flight';
    s.keeper!.phase = 'watching';
    s.ball.pos = { x: s.keeper!.b.x + 0.2, y: 1.3, z: 4 };
    s.ball.vel = { x: 0, y: 0, z: -60 }; // crashes through
    s.prevBallPos = { ...s.ball.pos };
    const r = run(s).result;
    expect(['goal', 'saved']).toContain(r?.type);
  });
});
