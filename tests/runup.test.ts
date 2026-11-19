import { describe, expect, it } from 'vitest';
import { kickerPositions, dirToGoal, rightOf } from '../src/sim/layout';
import { computeShot, runupEffect } from '../src/sim/ShotModel';
import { createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { v3 } from '../src/sim/math/vec3';
import { DT, P, SP, TAKERS, fly, makeCfg, sweet } from './helpers';

const D = { angle: SP.runupAngleDefault, dist: SP.runupDistDefault };

describe('runupEffect', () => {
  it('the default run-up (and an omitted one) is exactly neutral', () => {
    for (const fx of [runupEffect(D, SP), runupEffect(undefined, SP)]) {
      expect(fx.power).toBeCloseTo(1, 10);
      expect(fx.curl).toBeCloseTo(1, 10);
      expect(fx.knuckleOk).toBe(true);
    }
  });

  it('straighter = more power and less curl; wider = less power and more curl', () => {
    const straight = runupEffect({ ...D, angle: 0 }, SP);
    const wide = runupEffect({ ...D, angle: SP.runupAngleMax }, SP);
    expect(straight.power).toBeGreaterThan(1);
    expect(straight.curl).toBeLessThan(1);
    expect(wide.power).toBeLessThan(1);
    expect(wide.curl).toBeGreaterThan(1);
  });

  it('is monotonic in the angle', () => {
    let prev = runupEffect({ ...D, angle: 0 }, SP);
    for (let a = 5; a <= SP.runupAngleMax; a += 5) {
      const fx = runupEffect({ ...D, angle: a }, SP);
      expect(fx.power).toBeLessThan(prev.power);
      expect(fx.curl).toBeGreaterThan(prev.curl);
      prev = fx;
    }
  });

  it('a longer run-up adds a little power, bounded by distPower', () => {
    const short = runupEffect({ ...D, dist: SP.runupDistMin }, SP).power;
    const long = runupEffect({ ...D, dist: SP.runupDistMax }, SP).power;
    expect(short).toBeCloseTo(1 - SP.distPower, 6);
    expect(long).toBeCloseTo(1 + SP.distPower, 6);
    // out-of-range values are clamped
    expect(runupEffect({ ...D, dist: 50 }, SP).power).toBeCloseTo(1 + SP.distPower, 6);
  });

  it('a very wide approach rules out the knuckleball', () => {
    expect(runupEffect({ ...D, angle: SP.knuckleMaxAngle }, SP).knuckleOk).toBe(true);
    expect(runupEffect({ ...D, angle: SP.knuckleMaxAngle + 1 }, SP).knuckleOk).toBe(false);
  });
});

describe('Run-up in the shot model', () => {
  const req = (runup?: { angle: number; dist: number }, contact = { x: 0.6, y: 0 }) => ({
    spot: v3(-6, 0, 24),
    target: { x: 1.5, y: 1.4 },
    contact,
    power: sweet,
    driven: false,
    low: false,
    taker: TAKERS[0],
    runup,
    rng: null,
  });

  it('the default run-up reproduces the previous behaviour exactly', () => {
    expect(computeShot(req(D), P, SP).vel).toEqual(computeShot(req(undefined), P, SP).vel);
  });

  it('straight run-up: faster and less spin; wide run-up: slower and more spin', () => {
    const straight = computeShot(req({ ...D, angle: 0 }), P, SP);
    const def = computeShot(req(D), P, SP);
    const wide = computeShot(req({ ...D, angle: SP.runupAngleMax }), P, SP);
    expect(straight.speed).toBeGreaterThan(def.speed);
    expect(wide.speed).toBeLessThan(def.speed);
    expect(Math.abs(straight.spin.y)).toBeLessThan(Math.abs(def.spin.y));
    expect(Math.abs(wide.spin.y)).toBeGreaterThan(Math.abs(def.spin.y));
  });

  it('a wide approach with a centre strike is not a knuckleball', () => {
    const centre = { x: 0, y: 0 };
    expect(computeShot(req(D, centre), P, SP).knuckle).toBe(true);
    expect(computeShot(req({ ...D, angle: 70 }, centre), P, SP).knuckle).toBe(false);
  });

  it.each([
    [0, 2],
    [0, 6],
    [75, 2],
    [75, 6],
    [35, 3.5],
  ])('the solver still lands on the reticle (angle %i, length %f)', (angle, dist) => {
    const r = req({ angle, dist });
    const shot = computeShot(r, P, SP);
    const end = fly({ x: -6, y: P.radius, z: 24 }, shot.vel, shot.spin).ball.pos;
    expect(Math.abs(end.x - 1.5)).toBeLessThan(0.15);
    expect(Math.abs(end.y - 1.4)).toBeLessThan(0.15);
  });
});

describe('Run-up geometry', () => {
  const spot = v3(-8, 0, 22);

  it('the start point is exactly `dist` from the ball, on the taker\'s side', () => {
    for (const foot of ['R', 'L'] as const) {
      const f = dirToGoal(spot);
      const r = rightOf(f);
      const side = foot === 'R' ? -1 : 1; // right-footed taker comes from the left
      for (const angle of [0, 20, 60, 75]) {
        const { runupStart } = kickerPositions(spot, foot, { angle, dist: 4 });
        const dx = runupStart.x - spot.x;
        const dz = runupStart.z - spot.z;
        expect(Math.hypot(dx, dz)).toBeCloseTo(4, 6);
        const lateral = dx * r.x + dz * r.z;
        const back = -(dx * f.x + dz * f.z);
        expect(back).toBeGreaterThan(0); // behind the ball
        if (angle === 0) expect(lateral).toBeCloseTo(0, 6);
        else expect(Math.sign(lateral)).toBe(side);
      }
    }
  });

  it('a wider angle swings the start point further sideways and less far back', () => {
    const f = dirToGoal(spot);
    const r = rightOf(f);
    const at = (angle: number) => {
      const p = kickerPositions(spot, 'R', { angle, dist: 4 }).runupStart;
      return { lat: Math.abs((p.x - spot.x) * r.x + (p.z - spot.z) * r.z), back: -((p.x - spot.x) * f.x + (p.z - spot.z) * f.z) };
    };
    expect(at(60).lat).toBeGreaterThan(at(20).lat);
    expect(at(60).back).toBeLessThan(at(20).back);
  });
});

describe('Run-up in the sim', () => {
  const fresh = () => {
    const s = createSim(makeCfg(), 1);
    setSpot(s, -6, 24);
    return s;
  };
  const hold = (s: ReturnType<typeof fresh>, input: Partial<ReturnType<typeof emptyInput>>, seconds: number) => {
    for (let i = 0; i < seconds * 240; i++) step(s, { ...emptyInput(), ...input }, DT);
  };

  it('starts at the default', () => {
    const s = fresh();
    expect(s.runup).toEqual(D);
  });

  it('the angle and distance axes move it at the configured rates', () => {
    const s = fresh();
    hold(s, { runupAngle: 1 }, 0.5);
    expect(s.runup.angle).toBeCloseTo(D.angle + SP.runupAngleRate * 0.5, 1);
    hold(s, { runupDist: 1 }, 0.5);
    expect(s.runup.dist).toBeCloseTo(D.dist + SP.runupDistRate * 0.5, 1);
    hold(s, { runupDist: -1 }, 0.25);
    expect(s.runup.dist).toBeCloseTo(D.dist + SP.runupDistRate * 0.25, 1);
  });

  it('is clamped to the legal range', () => {
    const s = fresh();
    hold(s, { runupAngle: 1, runupDist: 1 }, 5);
    expect(s.runup.angle).toBe(SP.runupAngleMax);
    expect(s.runup.dist).toBe(SP.runupDistMax);
    hold(s, { runupAngle: -1, runupDist: -1 }, 10);
    expect(s.runup.angle).toBe(0);
    expect(s.runup.dist).toBe(SP.runupDistMin);
  });

  it('changing it changes the aim preview (power and curl move the launch)', () => {
    const s = fresh();
    s.contact = { x: 0.7, y: 0 };
    hold(s, {}, 0.05);
    const before = s.preview!.azimuth;
    hold(s, { runupAngle: 1 }, 0.6);
    expect(s.preview!.azimuth).not.toBe(before);
  });

  it('a longer run-up takes longer', () => {
    const dur = (dist: number) => {
      const s = fresh();
      s.runup.dist = dist;
      const holdShoot = { ...emptyInput(), shoot: true };
      step(s, holdShoot, DT);
      while (s.meter < sweet) step(s, holdShoot, DT);
      step(s, emptyInput(), DT);
      return s.runupDur;
    };
    expect(dur(6)).toBeCloseTo(SP.runupTime * (6 / SP.runupDistDefault), 6);
    expect(dur(2)).toBeLessThan(dur(6));
  });

  it('is locked once the shot is on its way', () => {
    const s = fresh();
    s.aim = { x: 1, y: 1.2 };
    const holdShoot = { ...emptyInput(), shoot: true };
    step(s, holdShoot, DT);
    while (s.meter < sweet) step(s, holdShoot, DT);
    step(s, emptyInput(), DT);
    const locked = { ...s.runup };
    hold(s, { runupAngle: 1, runupDist: 1 }, 1);
    expect(s.runup).toEqual(locked);
  });
});
