import { describe, expect, it } from 'vitest';
import { InputState, SimState, createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { defaultSettings } from '../src/sim/settings';
import { layoutWall, rollWall, startWallJump, wallEncroaching, wallShouldJump } from '../src/sim/Wall';
import { newBall } from '../src/sim/BallPhysics';
import { AI, DT, P, makeCfg, sweet, takeShot } from './helpers';

const spot = { x: -6, y: 0, z: 24 };
const base = { ...defaultSettings(), wallEnabled: true, wallCount: 4 };

describe('Lying man', () => {
  it('lies on the goal side of the wall, along its axis, on the ground', () => {
    const w = layoutWall(spot, base, AI.wall, { lyingMan: true, encroach: null })!;
    expect(w.lying).not.toBeNull();
    const l = w.lying!;
    expect(l.a.y).toBe(l.radius);
    expect(Math.hypot(l.b.x - l.a.x, l.b.z - l.a.z)).toBeCloseTo(2 * AI.wall.lyingManHalfLength, 5);
    // centre is lyingManDistance behind the wall's centre (towards goal = away from the ball)
    const cx = (l.a.x + l.b.x) / 2;
    const cz = (l.a.z + l.b.z) / 2;
    const wx = w.base.reduce((a, p) => a + p.x, 0) / w.base.length;
    const wz = w.base.reduce((a, p) => a + p.z, 0) / w.base.length;
    expect(Math.hypot(cx - wx, cz - wz)).toBeCloseTo(AI.wall.lyingManDistance, 5);
    expect((cx - wx) * w.facing.x + (cz - wz) * w.facing.z).toBeLessThan(0); // behind, away from the ball
  });

  it('is absent without the roll, or without the wall', () => {
    expect(layoutWall(spot, base, AI.wall, { lyingMan: false, encroach: null })!.lying).toBeNull();
    expect(layoutWall(spot, { ...base, wallEnabled: false }, AI.wall, { lyingMan: true, encroach: null })).toBeNull();
  });

  it('rolls: off never, always always, sometimes about at the configured rate', () => {
    const rng = { s: 99 };
    let some = 0;
    for (let i = 0; i < 400; i++) {
      expect(rollWall(rng, { ...base, lyingMan: 'off', encroachment: 'off' }, AI.wall).lyingMan).toBe(false);
      expect(rollWall(rng, { ...base, lyingMan: 'always' }, AI.wall).lyingMan).toBe(true);
      if (rollWall(rng, { ...base, lyingMan: 'sometimes' }, AI.wall).lyingMan) some++;
    }
    expect(some / 400).toBeGreaterThan(AI.wall.lyingManChance - 0.08);
    expect(some / 400).toBeLessThan(AI.wall.lyingManChance + 0.08);
  });

  // A low drive skids along the ground and slips under a jumping wall (all four jump, on time).
  const grounder = (lying: 'off' | 'always') => {
    const s = createSim(
      makeCfg(
        { wallEnabled: true, wallJumps: true, lyingMan: lying },
        { wall: { jumpChance: 1, reactionMin: 0, reactionMax: 0, jumpHeight: 0.5, jumpHeightJitter: 0 } },
      ),
      5,
    );
    setSpot(s, -6, 24);
    s.aim = { x: -0.5, y: 0.1 };
    return { s, r: takeShot(s, sweet, { low: true }) };
  };

  it('stops a low shot that slides under the jumping wall', () => {
    const without = grounder('off');
    expect(without.r?.wall).toBe(false);
    expect(without.r?.type).toBe('goal');
    const withMan = grounder('always');
    expect(withMan.r?.wall).toBe(true);
    expect(withMan.r?.type).not.toBe('goal');
  });
});

describe('Smart jumping', () => {
  const ball = (vx: number, vy: number, vz: number) => {
    const b = newBall({ x: spot.x, y: P.radius, z: spot.z });
    b.vel = { x: vx, y: vy, z: vz };
    return b;
  };

  it('jumps for a shot on target', () => {
    // aimed at roughly the middle of the goal
    expect(wallShouldJump(ball(5.5, 6, -28), spot, P, AI.wall)).toBe(true);
  });

  it('stays down for a shot going well wide', () => {
    expect(wallShouldJump(ball(14, 5, -26), spot, P, AI.wall)).toBe(false);
  });

  it('stays down for a shot sailing well over the bar', () => {
    expect(wallShouldJump(ball(5.5, 17, -22), spot, P, AI.wall)).toBe(false);
  });

  it('with a height criterion, ignores balls hugging the ground', () => {
    const ai = { ...AI.wall, jumpMinBallHeight: 0.9 };
    expect(wallShouldJump(ball(5.5, 0.2, -28), spot, P, ai)).toBe(false);
    expect(wallShouldJump(ball(5.5, 6, -28), spot, P, ai)).toBe(true);
  });

  it('startWallJump with shouldJump=false keeps everyone on the ground', () => {
    const w = layoutWall(spot, base, AI.wall)!;
    startWallJump(w, { ...AI.wall, jumpChance: 1 }, { s: 5 }, false);
    expect(Math.max(...w.jump!.height)).toBe(0);
  });
});

describe('Encroachment', () => {
  const sim = (freq: 'off' | 'always') => {
    const s = createSim(makeCfg({ wallEnabled: true, encroachment: freq }), 5);
    setSpot(s, -6, 24);
    return s;
  };
  const toStrike = (s: SimState, aim: { x: number; y: number }) => {
    s.aim = aim;
    const hold: InputState = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, emptyInput(), DT);
    while (s.phase === 'runup' || s.phase === 'charge') step(s, emptyInput(), DT);
  };

  it('one player creeps forward during the run-up, the rest stay on the line', () => {
    const s = sim('always');
    expect(s.wall!.encroachers).toHaveLength(1);
    toStrike(s, { x: 5, y: 1 });
    const w = s.wall!;
    const i = w.encroachers[0];
    expect(w.advance[i]).toBeGreaterThanOrEqual(AI.wall.encroachThreshold);
    expect(w.advance[i]).toBeLessThanOrEqual(AI.wall.creepMax + 1e-9);
    w.advance.forEach((a, j) => j === i || expect(a).toBe(0));
    expect(wallEncroaching(w, AI.wall)).toBe(true);
    // genuinely closer to the ball than he should be
    const dist = (p: { x: number; z: number }) => Math.hypot(p.x - s.spot.x, p.z - s.spot.z);
    expect(dist(w.players[i])).toBeLessThan(dist(w.base[i]) - 0.2);
  });

  it('after the strike the encroacher charges at the ball', () => {
    const s = sim('always');
    toStrike(s, { x: 5, y: 1 });
    const i = s.wall!.encroachers[0];
    const before = s.wall!.advance[i];
    for (let k = 0; k < 120; k++) step(s, emptyInput(), DT);
    expect(s.wall!.advance[i]).toBeGreaterThan(before + 0.5);
    expect(s.wall!.advance[i]).toBeLessThanOrEqual(AI.wall.rushMax + 1e-9);
  });

  it('a shot that does not score is a retake: not counted, same spot', () => {
    const s = sim('always');
    const spotId = s.spotId;
    const r = takeShot(s, sweet, {});
    expect(s.encroached).toBe(true);
    expect(r?.type).toBe('retake');
    expect(s.stats.attempts).toBe(0);
    for (let k = 0; k < 240 * 4 && s.phase === 'result'; k++) step(s, emptyInput(), DT);
    expect(s.phase).toBe('aim');
    expect(s.spotId).toBe(spotId); // same spot
    expect(s.spot).toEqual({ x: -6, y: 0, z: 24 });
    expect(s.encroached).toBe(false);
  });

  it('a goal stands (advantage)', () => {
    // Free of the wall's reach: top corner on the far side, with the encroacher on the near side.
    let goals = 0;
    for (let seed = 1; seed <= 20; seed++) {
      const s = createSim(makeCfg({ wallEnabled: true, encroachment: 'always' }), seed * 31);
      setSpot(s, -6, 24);
      s.aim = { x: 3.0, y: 1.8 };
      const r = takeShot(s, sweet, { contactX: 0.5, contactY: 0.2 });
      expect(s.encroached).toBe(true);
      if (r?.type === 'goal') {
        goals++;
        expect(s.stats.goals).toBe(1);
        expect(s.stats.attempts).toBe(1);
      } else expect(r?.type).toBe('retake');
    }
    expect(goals).toBeGreaterThan(5);
  });

  it('never happens when switched off', () => {
    const s = sim('off');
    expect(s.wall!.encroachers).toHaveLength(0);
    takeShot(s, sweet, {});
    expect(s.encroached).toBe(false);
    expect(s.result?.type).not.toBe('retake');
  });
});

describe('Wall shuffle', () => {
  it('moves the wall a little during the run-up, never more than the amplitude', () => {
    const s = createSim(makeCfg({ wallEnabled: true }), 5);
    setSpot(s, -6, 24);
    s.aim = { x: 2, y: 1.5 };
    const hold: InputState = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, emptyInput(), DT);
    let maxOff = 0;
    while (s.phase === 'runup') {
      step(s, emptyInput(), DT);
      s.wall!.players.forEach((p, i) => {
        maxOff = Math.max(maxOff, Math.hypot(p.x - s.wall!.base[i].x, p.z - s.wall!.base[i].z));
      });
    }
    expect(maxOff).toBeGreaterThan(0.02);
    expect(maxOff).toBeLessThanOrEqual(AI.wall.shuffleAmp + 1e-9);
  });
});
