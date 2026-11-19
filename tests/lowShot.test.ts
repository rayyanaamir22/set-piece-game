import { describe, expect, it } from 'vitest';
import { computeShot } from '../src/sim/ShotModel';
import { createSim, setSpot } from '../src/sim/Sim';
import { v3 } from '../src/sim/math/vec3';
import { P, SP, TAKERS, makeCfg, sweet, takeShot } from './helpers';

const req = (low: boolean, x = 1.0, y = 1.5) => ({
  spot: v3(-6, 0, 24),
  target: { x, y },
  contact: { x: 0, y: 0 },
  power: sweet,
  driven: false,
  low,
  taker: TAKERS[0],
  rng: null,
});

describe('Low drive', () => {
  it('is launched along the ground, not lobbed to the target', () => {
    const normal = computeShot(req(false), P, SP);
    const low = computeShot(req(true), P, SP);
    expect((low.elevation * 180) / Math.PI).toBeCloseTo(SP.lowElevation, 3);
    expect(low.elevation).toBeLessThan(normal.elevation / 5);
  });

  it('still points at the reticle sideways', () => {
    const left = computeShot(req(true, -2), P, SP);
    const right = computeShot(req(true, 2), P, SP);
    expect(right.azimuth).toBeGreaterThan(left.azimuth + 0.05);
  });

  it('skids along the grass at ball height all the way to goal', () => {
    const s = createSim(makeCfg(), 1);
    setSpot(s, -6, 24);
    s.aim = { x: 1.0, y: 1.5 };
    takeShot(s, sweet, { low: true });
    // It ends up in the net having stayed low.
    expect(s.ball.pos.y).toBeLessThan(1.0);
    expect(s.result?.type).toBe('goal');
    expect(Math.abs(s.ball.pos.x - 1.0)).toBeLessThan(2.5);
  });
});
