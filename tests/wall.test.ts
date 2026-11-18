import { describe, expect, it } from 'vitest';
import { InputState, SimState, createSim, emptyInput, setSpot, step, applySettings } from '../src/sim/Sim';
import { WALL_COUNT_MAX, WALL_HEIGHT_MAX, WALL_HEIGHT_MIN, defaultSettings, sanitizeSettings } from '../src/sim/settings';
import { WALL_SPACING, autoWallCount, layoutWall } from '../src/sim/Wall';
import { CFG, CFG_WALL, SP } from './helpers';

const DT = 1 / 240;
const sweet = (SP.sweetMin + SP.sweetMax) / 2;

function takeShot(s: SimState, power: number, extra: Partial<InputState> = {}) {
  const hold: InputState = { ...emptyInput(), ...extra, shoot: true };
  const idle: InputState = { ...emptyInput(), ...extra };
  step(s, hold, DT);
  while (s.meter < power) step(s, hold, DT);
  step(s, idle, DT);
  for (let i = 0; i < 240 * 8 && s.phase !== 'result'; i++) step(s, idle, DT);
  return s.result;
}

function fresh(x: number, z: number, height?: number) {
  const s = createSim(CFG_WALL, 5);
  if (height !== undefined) s.settings.wallHeight = height;
  setSpot(s, x, z);
  return s;
}

describe('Wall layout', () => {
  it('stands 9.15 m from the ball, shoulder to shoulder, facing it', () => {
    const w = layoutWall({ x: -8, y: 0, z: 22 }, defaultSettings())!;
    const centre = w.players.reduce((a, p) => ({ x: a.x + p.x / w.players.length, z: a.z + p.z / w.players.length }), { x: 0, z: 0 });
    // The wall's centre is ~9.15 m away (offset slightly sideways towards goal centre).
    const d = Math.hypot(centre.x + 8, centre.z - 22);
    expect(d).toBeGreaterThan(9.15);
    expect(d).toBeLessThan(9.8);
    for (let i = 1; i < w.players.length; i++) {
      expect(Math.hypot(w.players[i].x - w.players[i - 1].x, w.players[i].z - w.players[i - 1].z)).toBeCloseTo(WALL_SPACING, 5);
    }
    // Faces the ball.
    expect(w.facing.z).toBeGreaterThan(0.5);
  });

  it('covers the near post: the end player\'s edge sits on the ball -> near post line', () => {
    const spot = { x: 10, y: 0, z: 20 };
    const w = layoutWall(spot, { ...defaultSettings(), wallCount: 4 })!;
    const post = { x: 3.66, z: 0 };
    const ux = (post.x - spot.x) / Math.hypot(post.x - spot.x, post.z - spot.z);
    const uz = (post.z - spot.z) / Math.hypot(post.x - spot.x, post.z - spot.z);
    const dists = w.players.map((p) => Math.abs((p.x - spot.x) * uz - (p.z - spot.z) * ux));
    // The outermost player's edge touches the line, the rest stand towards the centre of goal.
    expect(Math.min(...dists)).toBeCloseTo(w.radius, 2);
    // and the wall lies on the goal-centre side of that line
    const others = w.players.filter((p) => Math.abs((p.x - spot.x) * uz - (p.z - spot.z) * ux) > 0.1);
    expect(others.every((p) => p.x < spot.x)).toBe(true);
  });

  it('central free kick gets a wall centred on the goal line of sight', () => {
    const w = layoutWall({ x: 0, y: 0, z: 24 }, { ...defaultSettings(), wallCount: 3 })!;
    expect(w.players[1].x).toBeCloseTo(0, 5);
    expect(w.players[0].x).toBeCloseTo(-w.players[2].x, 5);
  });

  it('auto count: closer and more central = more players', () => {
    expect(autoWallCount({ x: 0, y: 0, z: 19 })).toBeGreaterThan(autoWallCount({ x: 0, y: 0, z: 31 }));
    expect(autoWallCount({ x: 0, y: 0, z: 25 })).toBeGreaterThanOrEqual(autoWallCount({ x: 22, y: 0, z: 25 }));
  });

  it('honours an explicit count and can be switched off', () => {
    expect(layoutWall({ x: 3, y: 0, z: 24 }, { ...defaultSettings(), wallCount: 6 })!.players).toHaveLength(6);
    expect(layoutWall({ x: 3, y: 0, z: 24 }, { ...defaultSettings(), wallEnabled: false })).toBeNull();
  });

  it('settings are clamped into legal ranges', () => {
    const s = sanitizeSettings({ wallHeight: 9, wallCount: 99 });
    expect(s.wallHeight).toBe(WALL_HEIGHT_MAX);
    expect(s.wallCount).toBe(WALL_COUNT_MAX);
    expect(sanitizeSettings({ wallHeight: 0.2 }).wallHeight).toBe(WALL_HEIGHT_MIN);
    expect(sanitizeSettings({ wallHeight: NaN as unknown as number }).wallHeight).toBe(defaultSettings().wallHeight);
  });

  it('changing settings re-lays the wall while aiming', () => {
    const s = fresh(-6, 24);
    const before = s.wall!.height;
    s.settings.wallHeight = 2.05;
    applySettings(s);
    expect(s.wall!.height).toBeGreaterThan(before);
    s.settings.wallEnabled = false;
    applySettings(s);
    expect(s.wall).toBeNull();
  });
});

describe('Wall in play', () => {
  it('a flat, hard shot is blocked by the wall', () => {
    const s = fresh(-6, 24);
    s.aim = { x: -1.0, y: 1.2 };
    const r = takeShot(s, sweet, { contactX: 0.6 });
    expect(r?.wall).toBe(true);
    expect(r?.type).not.toBe('goal');
  });

  it('a dipping shot clears the wall and scores', () => {
    const s = fresh(-6, 24);
    s.aim = { x: -1.0, y: 2.0 };
    const r = takeShot(s, sweet, { contactX: 0.3, contactY: 0.8 });
    expect(r?.wall).toBe(false);
    expect(r?.type).toBe('goal');
  });

  it('the same shot that clears a short wall is blocked by the tallest wall', () => {
    const play = (h: number) => {
      const s = fresh(-6, 24, h);
      s.aim = { x: -1.0, y: 2.0 };
      return takeShot(s, sweet, { contactX: 0.3 });
    };
    expect(play(WALL_HEIGHT_MIN)?.wall).toBe(false);
    expect(play(WALL_HEIGHT_MAX)?.wall).toBe(true);
  });

  it('shots round the far side of the wall are not blocked (the wall covers the near post)', () => {
    const s = fresh(-6, 24);
    s.aim = { x: 2.8, y: 1.2 };
    const r = takeShot(s, sweet, { contactX: 0.6 });
    expect(r?.wall).toBe(false);
  });

  it('with the wall disabled the flat shot is not blocked', () => {
    const s = createSim(CFG, 5);
    setSpot(s, -6, 24);
    s.aim = { x: -1.0, y: 1.2 };
    const r = takeShot(s, sweet, { contactX: 0.6 });
    expect(r?.wall).toBe(false);
    expect(r?.type).toBe('goal');
  });
});
