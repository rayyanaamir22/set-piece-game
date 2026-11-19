import { describe, expect, it } from 'vitest';
import { predictAtPlane } from '../src/sim/Prediction';
import { computeShot } from '../src/sim/ShotModel';
import { applySettings, createSim, setSpot } from '../src/sim/Sim';
import { defaultSettings, windVector } from '../src/sim/settings';
import { v3 } from '../src/sim/math/vec3';
import { P, SP, TAKERS, fly, makeCfg, sweet, takeShot } from './helpers';

const start = v3(0, 0.11, 25);
const vel = v3(0, 3, -28);

describe('windVector', () => {
  const s = { ...defaultSettings(), windEnabled: true, windSpeed: 5 };
  it('direction 0 = towards goal, 90 = towards +x, 180 = towards the pitch', () => {
    const w0 = windVector({ ...s, windDirection: 0 });
    expect(w0.z).toBeCloseTo(-5, 6);
    expect(w0.x).toBeCloseTo(0, 6);
    const w90 = windVector({ ...s, windDirection: 90 });
    expect(w90.x).toBeCloseTo(5, 6);
    expect(w90.z).toBeCloseTo(0, 6);
    expect(windVector({ ...s, windDirection: 180 }).z).toBeCloseTo(5, 6);
  });
  it('is zero when off or with no speed', () => {
    expect(windVector({ ...s, windEnabled: false })).toEqual({ x: 0, y: 0, z: 0 });
    expect(windVector({ ...s, windSpeed: 0 })).toEqual({ x: 0, y: 0, z: 0 });
  });
});

describe('Wind physics', () => {
  it('zero wind changes nothing', () => {
    const a = fly(start, vel, v3());
    const b = fly(start, vel, v3(), P, undefined, v3(0, 0, 0));
    expect(b.ball.pos).toEqual(a.ball.pos);
  });

  it('a crosswind pushes the ball the way it blows, symmetrically', () => {
    const base = fly(start, vel, v3()).ball.pos.x;
    const right = fly(start, vel, v3(), P, undefined, v3(8, 0, 0)).ball.pos.x - base;
    const left = fly(start, vel, v3(), P, undefined, v3(-8, 0, 0)).ball.pos.x - base;
    expect(right).toBeGreaterThan(0.5);
    expect(left).toBeLessThan(-0.5);
    expect(right).toBeCloseTo(-left, 1);
  });

  it('the deflection grows with wind speed', () => {
    const dx = (w: number) => fly(start, vel, v3(), P, undefined, v3(w, 0, 0)).ball.pos.x;
    expect(dx(3)).toBeLessThan(dx(6));
    expect(dx(6)).toBeLessThan(dx(10));
  });

  it('a headwind slows the ball and a tailwind speeds it up', () => {
    const tail = fly(start, vel, v3(), P, undefined, v3(0, 0, -8)); // blowing towards goal
    const head = fly(start, vel, v3(), P, undefined, v3(0, 0, 8));
    const calm = fly(start, vel, v3());
    expect(tail.t).toBeLessThan(calm.t);
    expect(head.t).toBeGreaterThan(calm.t);
    expect(head.t - tail.t).toBeGreaterThan(0.04);
    expect(Math.hypot(head.ball.vel.x, head.ball.vel.y, head.ball.vel.z)).toBeLessThan(
      Math.hypot(tail.ball.vel.x, tail.ball.vel.y, tail.ball.vel.z),
    );
  });
});

describe('Wind and aiming', () => {
  const req = {
    spot: v3(-6, 0, 24),
    target: { x: 0.5, y: 1.2 },
    contact: { x: 0.3, y: 0 },
    power: sweet,
    driven: false,
    low: false,
    taker: TAKERS[0],
    rng: null,
  };

  it('the solver ignores wind, so a crosswind makes the ball miss the reticle', () => {
    const shot = computeShot(req, P, SP);
    const calm = fly({ x: -6, y: P.radius, z: 24 }, shot.vel, shot.spin);
    const windy = fly({ x: -6, y: P.radius, z: 24 }, shot.vel, shot.spin, P, undefined, v3(9, 0, 0));
    expect(Math.abs(calm.ball.pos.x - 0.5)).toBeLessThan(0.15);
    expect(windy.ball.pos.x - calm.ball.pos.x).toBeGreaterThan(0.6);
  });

  it('the launch is identical with or without wind; only the predicted-path aid shows the drift', () => {
    const plain = computeShot({ ...req }, P, SP, true);
    const windy = computeShot({ ...req, wind: v3(9, 0, 0) }, P, SP, true);
    expect(windy.vel).toEqual(plain.vel);
    const endCalm = plain.path![plain.path!.length - 1];
    const endWind = windy.path![windy.path!.length - 1];
    expect(endWind.x - endCalm.x).toBeGreaterThan(0.6);
  });

  it('the keeper\'s prediction includes wind (he can see it)', () => {
    const wind = v3(8, 0, 0);
    const spin = v3(0, 20, 0);
    const truth = fly(start, vel, spin, P, undefined, wind);
    const withWind = predictAtPlane(start, vel, spin, P, 0, wind)!;
    const without = predictAtPlane(start, vel, spin, P, 0)!;
    expect(Math.abs(withWind.x - truth.ball.pos.x)).toBeLessThan(0.1);
    expect(Math.abs(without.x - truth.ball.pos.x)).toBeGreaterThan(0.5);
  });
});

describe('Wind in the sim', () => {
  const playAt = (windOn: boolean) => {
    const s = createSim(makeCfg({ windEnabled: windOn, windSpeed: 10, windDirection: 90 }), 3);
    setSpot(s, 0, 24);
    s.aim = { x: 0, y: 1.2 };
    takeShot(s, sweet, { contactX: 0.3 });
    return s;
  };

  it('puts the wind on the ball and pushes the shot sideways', () => {
    const windy = playAt(true);
    const calm = playAt(false);
    expect(windy.ball.wind.x).toBeCloseTo(10, 6);
    expect(calm.ball.wind.x).toBe(0);
    expect(windy.ball.pos.x - calm.ball.pos.x).toBeGreaterThan(0.5);
  });

  it('switching wind on in settings reaches the ball while lining up', () => {
    const s = createSim(makeCfg(), 3);
    s.settings.windEnabled = true;
    s.settings.windSpeed = 6;
    s.settings.windDirection = 0;
    applySettings(s);
    expect(s.ball.wind.z).toBeCloseTo(-6, 6);
  });
});
