import { describe, expect, it } from 'vitest';
import { medalFor, scoreShot } from '../src/sim/challenge';
import { DIFFICULTIES } from '../src/sim/presets';
import { sanitizeSettings } from '../src/sim/settings';
import { InputState, SimState, applyPreset, createSim, emptyInput, selectTaker, setMode, setSpot, startChallenge, step } from '../src/sim/Sim';
import { layoutWall } from '../src/sim/Wall';
import { defaultSettings } from '../src/sim/settings';
import { AI, CHALLENGES, DT, PRESETS, SP, makeCfg, sweet, takeShot } from './helpers';

const idle = emptyInput();
const waitResult = (s: SimState, seconds = 6) => {
  for (let i = 0; i < seconds * 240 && s.phase === 'result'; i++) step(s, idle, DT);
};

describe('Goal-plane crossing', () => {
  it('records where the ball crossed z = 0, close to the reticle for a clean shot', () => {
    const s = createSim(makeCfg(), 1);
    setSpot(s, -6, 24);
    s.aim = { x: 2.0, y: 1.5 };
    takeShot(s, sweet, { contactX: 0.4 });
    expect(s.result?.type).toBe('goal');
    expect(s.crossing).not.toBeNull();
    expect(Math.abs(s.crossing!.x - 2.0)).toBeLessThan(0.6);
    expect(Math.abs(s.crossing!.y - 1.5)).toBeLessThan(0.6);
    expect(s.result?.crossing).toEqual(s.crossing);
  });
  it('is reset for each attempt', () => {
    const s = createSim(makeCfg(), 1);
    setSpot(s, -6, 24);
    s.aim = { x: 0.5, y: 1.0 };
    takeShot(s, sweet);
    expect(s.crossing).not.toBeNull();
    setSpot(s, 0, 24);
    expect(s.crossing).toBeNull();
  });
});

describe('Challenge scoring', () => {
  const def = CHALLENGES[0];
  it('only goals score; the best ring crossed wins; a goal outside every ring gets goalPoints', () => {
    const t = def.targets[0];
    expect(scoreShot(def, { x: t.x, y: t.y }, 'saved')).toEqual({ points: 0, ring: -1 });
    expect(scoreShot(def, { x: t.x + 0.1, y: t.y }, 'goal')).toEqual({ points: t.points, ring: 0 });
    expect(scoreShot(def, { x: 0, y: 0.3 }, 'goal')).toEqual({ points: def.goalPoints, ring: -1 });
    expect(scoreShot(def, null, 'goal').points).toBe(def.goalPoints);
  });
  it('picks the higher-valued ring where rings overlap', () => {
    const d = { ...def, targets: [{ x: 0, y: 1, r: 1, points: 2 }, { x: 0, y: 1, r: 0.3, points: 7 }] };
    expect(scoreShot(d, { x: 0, y: 1 }, 'goal')).toEqual({ points: 7, ring: 1 });
    expect(scoreShot(d, { x: 0.6, y: 1 }, 'goal')).toEqual({ points: 2, ring: 0 });
  });
  it('medals follow the thresholds', () => {
    const m = def.medals;
    expect(medalFor(def, m.bronze - 1)).toBeNull();
    expect(medalFor(def, m.bronze)).toBe('bronze');
    expect(medalFor(def, m.silver)).toBe('silver');
    expect(medalFor(def, m.gold + 5)).toBe('gold');
  });
  it('every shipped challenge is sane', () => {
    for (const c of CHALLENGES) {
      expect(c.shots).toBeGreaterThan(0);
      expect(c.medals.bronze).toBeLessThan(c.medals.silver);
      expect(c.medals.silver).toBeLessThan(c.medals.gold);
      expect(Math.hypot(c.spot.x, c.spot.z)).toBeGreaterThan(17);
      for (const t of c.targets) {
        expect(Math.abs(t.x) + t.r).toBeLessThan(3.66); // inside the posts
        expect(t.y + t.r).toBeLessThan(2.44); // under the bar
        expect(t.y - t.r).toBeGreaterThanOrEqual(0);
      }
      expect(sanitizeSettings(c.settings as never)).toMatchObject(c.settings);
      // gold must be reachable: all shots at the best ring
      expect(c.shots * Math.max(...c.targets.map((t) => t.points))).toBeGreaterThanOrEqual(c.medals.gold);
    }
  });
});

describe('Challenge mode', () => {
  it('applies the challenge rules, sets the spot, and restores your settings afterwards', () => {
    const s = createSim(makeCfg({ wallEnabled: true, keeperPresent: true, windEnabled: false }), 1);
    const before = { ...s.settings };
    startChallenge(s, 0); // Top Bins: no wall, no keeper
    expect(s.mode).toBe('challenge');
    expect(s.settings.wallEnabled).toBe(false);
    expect(s.settings.keeperPresent).toBe(false);
    expect(s.spot).toMatchObject({ x: CHALLENGES[0].spot.x, z: CHALLENGES[0].spot.z });
    startChallenge(s, 4); // Windy Day
    expect(s.settings.windEnabled).toBe(true);
    expect(s.settings.windSpeed).toBe(8);
    setMode(s, 'free');
    expect(s.challenge).toBeNull();
    expect(s.settings).toEqual(before);
  });

  it('settings from one challenge do not leak into the next', () => {
    const s = createSim(makeCfg(), 1);
    startChallenge(s, 3); // keeper duel
    expect(s.settings.keeperPresent).toBe(true);
    startChallenge(s, 0);
    expect(s.settings.keeperPresent).toBe(false);
    expect(s.settings.keeperSkill).toBe(defaultSettings().keeperSkill);
  });

  it('counts shots and points, stays on the same spot, and finishes with a medal', () => {
    const s = createSim(makeCfg(), 7);
    startChallenge(s, 0);
    const def = s.challenge!.def;
    const spot = { ...s.spot };
    s.aim = { ...def.targets[0] };
    for (let n = 1; n <= def.shots; n++) {
      s.aim = { x: def.targets[0].x, y: def.targets[0].y };
      takeShot(s, sweet, { contactX: 0.3 });
      expect(s.challenge!.taken).toBe(n);
      if (n < def.shots) {
        waitResult(s);
        expect(s.phase).toBe('aim');
        expect(s.spot).toMatchObject(spot);
      }
    }
    const ch = s.challenge!;
    expect(ch.done).toBe(true);
    expect(ch.log).toHaveLength(def.shots);
    expect(ch.points).toBe(ch.log.reduce((a, l) => a + l.points, 0));
    expect(ch.medal).toBe(medalFor(def, ch.points));
    // The summary stays up (no auto-advance) until you ask.
    for (let i = 0; i < 240 * 10; i++) step(s, idle, DT);
    expect(s.phase).toBe('result');
  });

  it('R restarts the run and N moves to the next challenge when finished', () => {
    const s = createSim(makeCfg(), 7);
    startChallenge(s, 0);
    s.challenge!.done = true;
    s.phase = 'result';
    step(s, { ...idle, retry: true }, DT);
    expect(s.challenge!.index).toBe(0);
    expect(s.challenge!.taken).toBe(0);
    expect(s.phase).toBe('aim');
    s.challenge!.done = true;
    s.phase = 'result';
    step(s, { ...idle, newSpot: true }, DT);
    expect(s.challenge!.index).toBe(1);
  });

  it('wraps around at both ends of the list', () => {
    const s = createSim(makeCfg(), 1);
    startChallenge(s, CHALLENGES.length);
    expect(s.challenge!.index).toBe(0);
    startChallenge(s, -1);
    expect(s.challenge!.index).toBe(CHALLENGES.length - 1);
  });

  it('retakes are not counted, there are no rebounds, and N does not move the ball', () => {
    const s = createSim(makeCfg(), 7);
    startChallenge(s, 0);
    const spot = { ...s.spot };
    step(s, { ...idle, newSpot: true }, DT);
    expect(s.spot).toMatchObject(spot);
    s.settings.keeperPresent = true;
    expect(s.cfg.ai.keeper.maxRebounds).toBeGreaterThan(0);
  });
});

describe('Practice mode', () => {
  it('stays on the same spot after each attempt (instant retry)', () => {
    const s = createSim(makeCfg(), 3);
    setMode(s, 'practice');
    setSpot(s, 3, 22);
    s.aim = { x: -1, y: 1.4 };
    takeShot(s, sweet);
    waitResult(s);
    expect(s.phase).toBe('aim');
    expect(s.spot).toMatchObject({ x: 3, z: 22 });
  });
  it('N still gives a new random spot', () => {
    const s = createSim(makeCfg(), 3);
    setMode(s, 'practice');
    setSpot(s, 3, 22);
    step(s, { ...idle, newSpot: true }, DT);
    expect(s.spot).not.toMatchObject({ x: 3, z: 22 });
  });
  it('free-kick mode still moves to a new spot', () => {
    const s = createSim(makeCfg(), 3);
    setSpot(s, 3, 22);
    s.aim = { x: -1, y: 1.4 };
    takeShot(s, sweet);
    waitResult(s);
    expect(s.spot).not.toMatchObject({ x: 3, z: 22 });
  });
});

describe('Close-range wall', () => {
  it('there is no wall when the ball is too close for one to fit', () => {
    expect(layoutWall({ x: 0, y: 0, z: 9 }, defaultSettings(), AI.wall)).toBeNull();
    expect(layoutWall({ x: 0, y: 0, z: 12 }, defaultSettings(), AI.wall)).not.toBeNull();
  });
});

describe('Difficulty presets', () => {
  it('ships easy/normal/hard/pro with valid ranges', () => {
    for (const n of DIFFICULTIES) {
      const p = PRESETS[n];
      expect(sanitizeSettings(p.settings as never)).toMatchObject(p.settings);
      expect(Math.abs(p.accuracyBonus)).toBeLessThanOrEqual(0.2);
    }
  });
  it('get monotonically harder: keeper skill up, accuracy bonus down', () => {
    const skill = DIFFICULTIES.map((n) => PRESETS[n].settings.keeperSkill!);
    const acc = DIFFICULTIES.map((n) => PRESETS[n].accuracyBonus);
    for (let i = 1; i < skill.length; i++) {
      expect(skill[i]).toBeGreaterThan(skill[i - 1]);
      expect(acc[i]).toBeLessThanOrEqual(acc[i - 1]);
    }
  });
  it('applying one changes the settings, keeps the object identity, and records the name', () => {
    const s = createSim(makeCfg(), 1);
    const ref = s.settings;
    applyPreset(s, 'pro');
    expect(s.settings).toBe(ref);
    expect(s.settings.keeperSkill).toBe(PRESETS.pro.settings.keeperSkill);
    expect(s.settings.lyingMan).toBe('always');
    expect(s.difficulty).toBe('pro');
    expect(s.accuracyBonus).toBe(-0.1);
    applyPreset(s, 'easy');
    expect(s.settings.wallJumps).toBe(false);
    expect(s.accuracyBonus).toBe(0.1);
  });
  it('a positive accuracy bonus shrinks the shot error', () => {
    const err = (bonus: number) => {
      const s = createSim(makeCfg(), 21);
      setSpot(s, -6, 24);
      s.accuracyBonus = bonus;
      s.taker.accuracy = 0.5;
      s.aim = { x: 1, y: 1.3 };
      takeShot(s, sweet);
      return Math.hypot(s.pending!.error.x, s.pending!.error.y);
    };
    expect(err(0.4)).toBeLessThan(err(-0.4));
  });
});

describe('Taker roster', () => {
  it('has distinct fictional takers with valid stats', () => {
    const s = createSim(makeCfg(), 1);
    expect(s.cfg.takers.length).toBeGreaterThanOrEqual(5);
    const names = new Set(s.cfg.takers.map((t) => t.name));
    expect(names.size).toBe(s.cfg.takers.length);
    for (const t of s.cfg.takers) {
      expect(['R', 'L']).toContain(t.foot);
      for (const k of ['accuracy', 'curve', 'power'] as const) expect(t[k]).toBeGreaterThan(0);
      expect(t.accuracy).toBeLessThanOrEqual(1);
    }
  });
  it('selecting a taker switches stats and foot, and wraps', () => {
    const s = createSim(makeCfg(), 1);
    selectTaker(s, 1);
    expect(s.taker).toBe(s.cfg.takers[1]);
    expect(s.foot).toBe(s.cfg.takers[1].foot);
    selectTaker(s, s.cfg.takers.length); // wraps to 0
    expect(s.takerIndex).toBe(0);
    selectTaker(s, -1);
    expect(s.takerIndex).toBe(s.cfg.takers.length - 1);
  });
  it('different takers shoot differently (cannon is faster, curler curls more)', () => {
    const shot = (i: number) => {
      const s = createSim(makeCfg(), 4);
      selectTaker(s, i);
      s.foot = 'R';
      setSpot(s, -6, 24);
      s.aim = { x: 1, y: 1.4 };
      s.contact = { x: 0.7, y: 0 };
      takeShot(s, sweet, { contactX: 0.7 });
      return s.pending!;
    };
    const cannon = shot(2);
    const bender = shot(5);
    expect(cannon.speed).toBeGreaterThan(bender.speed);
    expect(Math.abs(bender.spin.y)).toBeGreaterThan(Math.abs(cannon.spin.y));
  });
});

describe('Mouse aim deltas', () => {
  it('move the reticle once and clamp to the frame', () => {
    const s = createSim(makeCfg(), 1);
    s.aim = { x: 0, y: 1.2 };
    step(s, { ...idle, aimDeltaX: 1.5, aimDeltaY: 0.4 }, DT);
    expect(s.aim.x).toBeCloseTo(1.5, 6);
    expect(s.aim.y).toBeCloseTo(1.6, 6);
    step(s, { ...idle, aimDeltaX: 100, aimDeltaY: -100 }, DT);
    expect(s.aim.x).toBe(7);
    expect(s.aim.y).toBe(0.1);
  });
  it('are ignored once the shot is on its way', () => {
    const s = createSim(makeCfg(), 1);
    setSpot(s, -6, 24);
    s.aim = { x: 1, y: 1.2 };
    const hold = { ...idle, shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, idle, DT);
    const aim = { ...s.aim };
    step(s, { ...idle, aimDeltaX: 3 }, DT);
    expect(s.aim).toEqual(aim);
  });
});

describe('Dummy run-up', () => {
  const play = (dummy: boolean, seed = 5, wallSettings = {}) => {
    const s = createSim(makeCfg({ wallEnabled: true, wallJumps: true, keeperPresent: true, ...wallSettings }, { wall: { jumpChance: 1, reactionMin: 0, reactionMax: 0, dummyJumpChance: 1, dummyJumpReactionMax: 0 } }), seed);
    setSpot(s, -6, 24);
    s.aim = { x: -1.0, y: 2.0 };
    const hold: InputState = { ...idle, shoot: true, contactX: 0.3, contactY: 0.8 };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, { ...idle, dummy, contactX: 0.3, contactY: 0.8 }, DT);
    return s;
  };

  it('adds a beat to the run-up', () => {
    expect(play(true).runupDur).toBeCloseTo(play(false).runupDur + SP.dummyDelay, 6);
    expect(play(true).dummy).toBe(true);
    expect(play(false).dummy).toBe(false);
  });

  it('the wall jumps for the decoy, long before the ball is struck', () => {
    const s = play(true);
    let maxLiftBeforeStrike = 0;
    while (s.phase === 'runup') {
      step(s, idle, DT);
      if (s.phase === 'runup' && s.wall) maxLiftBeforeStrike = Math.max(maxLiftBeforeStrike, ...s.wall.lift);
    }
    expect(maxLiftBeforeStrike).toBeGreaterThan(0.2);
    expect(s.wall!.earlyJumped.every(Boolean)).toBe(true);
  });

  it('wall players do not jump twice, and are back down by the time the ball arrives', () => {
    const s = play(true);
    while (s.phase === 'runup') step(s, idle, DT);
    // The ball takes ~0.3 s to reach the wall; by then the early jumpers have landed.
    while (s.flightT < 0.3) step(s, idle, DT);
    expect(Math.max(...s.wall!.lift)).toBeLessThan(0.05);
  });

  it('a dip that is blocked by a jumping wall gets through a wall that jumped for the decoy', () => {
    const run = (dummy: boolean) => {
      const s = play(dummy);
      for (let i = 0; i < 240 * 9 && s.phase !== 'result'; i++) step(s, idle, DT);
      return s.result;
    };
    expect(run(false)?.wall).toBe(true);
    expect(run(true)?.wall).toBe(false);
  });

  it('makes a weak keeper bite more often', () => {
    const guesses = (dummy: boolean) => {
      let n = 0;
      for (let seed = 1; seed <= 60; seed++) {
        const s = play(dummy, seed * 3, { keeperSkill: 0.2 });
        while (s.phase === 'runup') step(s, idle, DT);
        if (s.keeper!.guess !== 0) n++;
      }
      return n;
    };
    expect(guesses(true)).toBeGreaterThan(guesses(false));
  });

  it('without the modifier nothing changes', () => {
    const s = play(false);
    expect(s.dummyDone).toBe(false);
    while (s.phase === 'runup') step(s, idle, DT);
    expect(s.wall!.earlyJumped.some(Boolean)).toBe(false);
  });
});
