import { describe, expect, it } from 'vitest';
import { computeShot, ShotRequest } from '../src/sim/ShotModel';
import { CFG, P, SP, TAKERS, fly } from './helpers';
import { v3 } from '../src/sim/math/vec3';

const sweet = (SP.sweetMin + SP.sweetMax) / 2;
const base = (o: Partial<ShotRequest> = {}): ShotRequest => ({
  spot: v3(-6, 0, 24),
  target: { x: 2.8, y: 1.9 },
  contact: { x: 0, y: 0 },
  power: sweet,
  driven: false,
  low: false,
  taker: TAKERS[0],
  rng: null,
  ...o,
});

function arrival(req: ShotRequest) {
  const s = computeShot(req, P, SP);
  const f = fly({ x: req.spot.x, y: P.radius, z: req.spot.z }, s.vel, s.spin);
  return { s, x: f.ball.pos.x, y: f.ball.pos.y };
}

describe('ShotModel', () => {
  it.each([
    ['plain', { x: 0, y: 0 }],
    ['curl right-contact', { x: 0.8, y: 0 }],
    ['curl left-contact', { x: -0.8, y: 0 }],
    ['dip', { x: 0.3, y: 0.7 }],
    ['lift', { x: 0, y: -0.7 }],
  ])('solver lands the ball on the reticle (%s)', (_n, contact) => {
    const { x, y } = arrival(base({ contact }));
    expect(Math.abs(x - 2.8)).toBeLessThan(0.1);
    expect(Math.abs(y - 1.9)).toBeLessThan(0.1);
  });

  it('contact point changes the path shape, not the end point', () => {
    const plain = computeShot(base(), P, SP);
    const curl = computeShot(base({ contact: { x: 0.9, y: 0 } }), P, SP);
    // curling left (contact right) needs aiming further right initially
    expect(curl.azimuth).toBeGreaterThan(plain.azimuth + 0.01);
  });

  it('dip contact needs a higher launch than lift contact', () => {
    const dip = computeShot(base({ contact: { x: 0, y: 0.9 } }), P, SP);
    const lift = computeShot(base({ contact: { x: 0, y: -0.9 } }), P, SP);
    expect(dip.elevation).toBeGreaterThan(lift.elevation);
  });

  it('centre contact at high power is a knuckleball; side contact is not', () => {
    expect(computeShot(base({ power: 0.82 }), P, SP).knuckle).toBe(true);
    expect(computeShot(base({ power: 0.82, contact: { x: 0.5, y: 0 } }), P, SP).knuckle).toBe(false);
    expect(computeShot(base({ power: 0.3 }), P, SP).knuckle).toBe(false);
  });

  it('driven shots are faster with less spin', () => {
    const n = computeShot(base({ contact: { x: 0.8, y: 0 } }), P, SP);
    const d = computeShot(base({ contact: { x: 0.8, y: 0 }, driven: true }), P, SP);
    expect(d.speed).toBeGreaterThan(n.speed);
    expect(Math.abs(d.spin.y)).toBeLessThan(Math.abs(n.spin.y));
  });

  it('missing the sweet spot adds error, hitting it adds almost none', () => {
    const rng = { s: 7 };
    const perfect = computeShot(base({ rng }), P, SP);
    const off = computeShot(base({ rng, power: 0.4 }), P, SP);
    expect(Math.hypot(perfect.error.x, perfect.error.y)).toBeLessThan(0.3);
    expect(Math.hypot(off.error.x, off.error.y)).toBeGreaterThan(3);
  });

  it('overcharging lifts the shot', () => {
    const over = computeShot(base({ rng: { s: 3 }, power: 1 }), P, SP);
    expect(over.error.y).toBeGreaterThan(1);
  });

  it('is deterministic for a given rng seed', () => {
    const a = computeShot(base({ rng: { s: 99 }, power: 0.6 }), P, SP);
    const b = computeShot(base({ rng: { s: 99 }, power: 0.6 }), P, SP);
    expect(a.vel).toEqual(b.vel);
  });

  it('config smoke', () => expect(CFG.takers.length).toBeGreaterThan(0));
});
