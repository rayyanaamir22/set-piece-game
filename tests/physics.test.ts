import { describe, expect, it } from 'vitest';
import { newBall, stepBall, groundContact } from '../src/sim/BallPhysics';
import { CFG, P, fly } from './helpers';
import { v3 } from '../src/sim/math/vec3';

const noAir = { ...P, cdHigh: 0, cdLow: 0, clGain: 0, spinDecay: 0 };

describe('BallPhysics', () => {
  it('no drag, no spin: follows the analytic parabola', () => {
    const b = newBall(v3(0, 1, 0));
    b.vel = v3(3, 8, -10);
    const T = 1.0;
    for (let i = 0; i < 240; i++) stepBall(b, noAir, 1 / 240);
    expect(b.pos.x).toBeCloseTo(3 * T, 3);
    expect(b.pos.z).toBeCloseTo(-10 * T, 3);
    expect(b.pos.y).toBeCloseTo(1 + 8 * T - 0.5 * 9.81 * T * T, 3);
  });

  it('drag slows the ball', () => {
    const b = newBall(v3(0, 1, 30));
    b.vel = v3(0, 0, -28);
    for (let i = 0; i < 240; i++) stepBall(b, P, 1 / 240);
    expect(-b.vel.z).toBeLessThan(24);
    expect(-b.vel.z).toBeGreaterThan(15);
  });

  it('sidespin about +y curls the ball towards -x, and -y towards +x', () => {
    const a = fly(v3(0, 1, 25), v3(0, 3, -28), v3(0, 40, 0));
    const c = fly(v3(0, 1, 25), v3(0, 3, -28), v3(0, -40, 0));
    expect(a.ball.pos.x).toBeLessThan(-0.5);
    expect(c.ball.pos.x).toBeGreaterThan(0.5);
  });

  it('typical curler deflects roughly 1.5-3.5 m over 25 m', () => {
    const a = fly(v3(0, 0.11, 25), v3(0, 3, -28), v3(0, 40, 0));
    expect(Math.abs(a.ball.pos.x)).toBeGreaterThan(1.5);
    expect(Math.abs(a.ball.pos.x)).toBeLessThan(3.5);
  });

  it('topspin drops the ball earlier than no spin; backspin keeps it up', () => {
    const none = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(0, 0, 0));
    const top = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(-40, 0, 0));
    const back = fly(v3(0, 0.11, 25), v3(0, 5, -28), v3(40, 0, 0));
    expect(top.ball.pos.y).toBeLessThan(none.ball.pos.y - 0.2);
    expect(back.ball.pos.y).toBeGreaterThan(none.ball.pos.y + 0.2);
  });

  it('knuckle wobble is bounded and deterministic', () => {
    const k = { amp: CFG.physics.knuckleAccel, phase: [0.3, 1.1, 2.0, 4.0, 5.1, 0.7] };
    const a = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0), P, k);
    const base = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0));
    const again = fly(v3(0, 0.11, 25), v3(0, 3, -30), v3(0, 0, 0), P, k);
    const dev = Math.hypot(a.ball.pos.x - base.ball.pos.x, a.ball.pos.y - base.ball.pos.y);
    expect(dev).toBeGreaterThan(0.02);
    expect(dev).toBeLessThan(2);
    expect(again.ball.pos.x).toBe(a.ball.pos.x);
  });

  it('bounces with restitution and eventually rolls to rest', () => {
    const b = newBall(v3(0, 5, 0));
    let maxAfter = 0;
    let bounced = false;
    for (let i = 0; i < 240 * 20; i++) {
      stepBall(b, P, 1 / 240);
      const imp = groundContact(b, P, 1 / 240);
      if (imp > 0 && !bounced) bounced = true;
      else if (bounced && b.vel.y > 0) maxAfter = Math.max(maxAfter, b.pos.y);
    }
    // first bounce peak ~ e^2 * h
    expect(maxAfter).toBeGreaterThan(1.2);
    expect(maxAfter).toBeLessThan(2.4);
    expect(b.pos.y).toBeCloseTo(P.radius, 2);
    expect(Math.abs(b.vel.y)).toBeLessThan(0.1);
  });

  it('backspin checks the ball on landing', () => {
    const run = (wx: number) => {
      const b = newBall(v3(0, 2, 0));
      b.vel = v3(0, 0, -10);
      b.spin = v3(wx, 0, 0);
      for (let i = 0; i < 240; i++) {
        stepBall(b, noAir, 1 / 240);
        groundContact(b, noAir, 1 / 240);
      }
      return -b.vel.z;
    };
    expect(run(60)).toBeLessThan(run(-60));
  });
});
