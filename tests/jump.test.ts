import { describe, expect, it } from 'vitest';
import { InputState, SimState, createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { defaultSettings } from '../src/sim/settings';
import { layoutWall, startWallJump, updateWallLift } from '../src/sim/Wall';
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

const wall = () => layoutWall({ x: -6, y: 0, z: 24 }, { ...defaultSettings(), wallCount: 4 })!;

describe('Wall jump', () => {
  it('lift is a parabola: zero before the jump, peak at mid-air, zero after landing', () => {
    const w = wall();
    startWallJump(w, { ...AI.wall, jumpChance: 1, reactionMin: 0.1, reactionMax: 0.1, jumpHeight: 0.5, jumpHeightJitter: 0, jumpDuration: 0.6 }, { s: 1 });
    updateWallLift(w, 0.05);
    expect(w.lift.every((l) => l === 0)).toBe(true);
    updateWallLift(w, 0.4); // start 0.1 + half of 0.6
    for (const l of w.lift) expect(l).toBeCloseTo(0.5, 5);
    updateWallLift(w, 0.5);
    for (const l of w.lift) expect(l).toBeGreaterThan(0.4);
    updateWallLift(w, 0.8);
    expect(w.lift.every((l) => l === 0)).toBe(true);
  });

  it('jumpChance 0: nobody jumps; chance 1: everybody does', () => {
    const none = wall();
    startWallJump(none, { ...AI.wall, jumpChance: 0 }, { s: 3 });
    updateWallLift(none, 0.3);
    expect(Math.max(...none.lift)).toBe(0);
    const all = wall();
    startWallJump(all, { ...AI.wall, jumpChance: 1, reactionMax: 0 }, { s: 3 });
    updateWallLift(all, 0.3);
    expect(Math.min(...all.lift)).toBeGreaterThan(0.2);
  });

  it('each jump is randomised (not all players identical) but deterministic per seed', () => {
    const a = wall();
    const b = wall();
    startWallJump(a, { ...AI.wall, jumpChance: 1 }, { s: 42 });
    startWallJump(b, { ...AI.wall, jumpChance: 1 }, { s: 42 });
    expect(a.jump).toEqual(b.jump);
    expect(new Set(a.jump!.start).size).toBeGreaterThan(1);
  });

  it('jump heights stay within the configured range', () => {
    const w = wall();
    startWallJump(w, { ...AI.wall, jumpChance: 1, jumpHeight: 0.42, jumpHeightJitter: 0.15 }, { s: 9 });
    for (const h of w.jump!.height) {
      expect(h).toBeLessThanOrEqual(0.42);
      expect(h).toBeGreaterThanOrEqual(0.42 * 0.85);
    }
  });

  it('a dip that clears the standing wall is blocked by a jumping one', () => {
    const play = (jumps: boolean) => {
      const s = createSim(makeCfg({ wallEnabled: true, wallJumps: jumps }, { wall: { jumpChance: 1, reactionMin: 0, reactionMax: 0, jumpHeight: 0.5, jumpHeightJitter: 0 } }), 5);
      setSpot(s, -6, 24);
      s.aim = { x: -1.0, y: 2.0 };
      return takeShot(s, sweet, { contactX: 0.3, contactY: 0.8 });
    };
    const standing = play(false);
    expect(standing?.wall).toBe(false);
    expect(standing?.type).toBe('goal');
    expect(play(true)?.wall).toBe(true);
  });

  it('the wall lands again: lift returns to zero after the jump', () => {
    const s = createSim(makeCfg({ wallEnabled: true, wallJumps: true }, { wall: { jumpChance: 1 } }), 5);
    setSpot(s, -6, 24);
    s.aim = { x: 3, y: 1 };
    takeShot(s, sweet);
    expect(s.phase).toBe('result');
    for (let i = 0; i < 240 * 1.5 && s.phase === 'result'; i++) step(s, emptyInput(), DT);
    expect(s.phase).toBe('result');
    expect(s.wall).not.toBeNull();
    for (const l of s.wall!.lift) expect(l).toBe(0);
  });

  it('with wallJumps off nobody leaves the ground', () => {
    const s = createSim(makeCfg({ wallEnabled: true, wallJumps: false }, { wall: { jumpChance: 1 } }), 5);
    setSpot(s, -6, 24);
    s.aim = { x: -1, y: 1.2 };
    let maxLift = 0;
    const hold = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    for (let i = 0; i < 240 * 3; i++) {
      step(s, emptyInput(), DT);
      if (s.wall) maxLift = Math.max(maxLift, ...s.wall.lift);
    }
    expect(maxLift).toBe(0);
  });
});
