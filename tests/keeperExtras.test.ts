import { describe, expect, it } from 'vitest';
import { effectiveKeeperAi } from '../src/sim/ai';
import { keeperOnStrike, layoutKeeper } from '../src/sim/Keeper';
import { SimState, applySettings, createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { defaultSettings, sanitizeSettings } from '../src/sim/settings';
import { AI, DT, makeCfg, sweet, takeShot } from './helpers';

describe('Keeper types', () => {
  it('apply their overrides on top of the base tuning', () => {
    const bal = effectiveKeeperAi(AI, 'balanced');
    const quick = effectiveKeeperAi(AI, 'quick');
    const tall = effectiveKeeperAi(AI, 'tall');
    const strong = effectiveKeeperAi(AI, 'strong');
    expect(bal).toEqual(AI.keeper);
    expect(quick.reactionFast).toBeLessThan(bal.reactionFast);
    expect(quick.reachX).toBeLessThan(bal.reachX);
    expect(tall.reachX).toBeGreaterThan(bal.reachX);
    expect(tall.reactionSlow).toBeGreaterThan(bal.reactionSlow);
    expect(strong.holdSpeedMax).toBeGreaterThan(bal.holdSpeedMax);
    expect(strong.spinReadMin).toBeLessThan(bal.spinReadMin);
  });

  it('the sim picks the type from settings and refreshes it when settings change', () => {
    const s = createSim(makeCfg({ keeperPresent: true, keeperType: 'tall' }), 1);
    expect(s.keeperAi.reachX).toBe(AI.keeperPresets.tall.reachX);
    s.settings.keeperType = 'quick';
    applySettings(s);
    expect(s.keeperAi.reachX).toBe(AI.keeperPresets.quick.reachX);
    expect(s.keeperAi.reactionFast).toBe(AI.keeperPresets.quick.reactionFast);
  });

  it('a quick keeper reacts earlier than a tall one', () => {
    const settings = { ...defaultSettings(), keeperPresent: true, keeperSkill: 0.7 };
    const delay = (type: 'quick' | 'tall') => {
      const ai = effectiveKeeperAi({ ...AI, keeper: { ...AI.keeper, earlyGuessChance: 0 } }, type);
      const k = layoutKeeper({ x: -6, y: 0, z: 24 }, settings, ai)!;
      keeperOnStrike(k, ai, { s: 5 });
      return k.decideAt;
    };
    expect(delay('quick')).toBeLessThan(delay('tall') - 0.05);
  });

  it('sanitizes unknown types and frequencies back to defaults', () => {
    const s = sanitizeSettings({ keeperType: 'bogus' as never, lyingMan: 'maybe' as never, encroachment: 5 as never });
    expect(s.keeperType).toBe('balanced');
    expect(s.lyingMan).toBe(defaultSettings().lyingMan);
    expect(s.encroachment).toBe(defaultSettings().encroachment);
    expect(sanitizeSettings({ keeperType: 'quick', lyingMan: 'always' })).toMatchObject({ keeperType: 'quick', lyingMan: 'always' });
  });
});

describe('Early guess', () => {
  const strike = (guessChance: number, skill = 0) => {
    const ai = { ...AI.keeper, earlyGuessChance: guessChance };
    const k = layoutKeeper({ x: -6, y: 0, z: 24 }, { ...defaultSettings(), keeperPresent: true, keeperSkill: skill }, ai)!;
    keeperOnStrike(k, ai, { s: 11 });
    return k;
  };

  it('with chance 1 a weak keeper always commits to a side before reading the shot', () => {
    const k = strike(1, 0);
    expect(Math.abs(k.guess)).toBe(1);
    expect(k.phase).toBe('committed');
    expect(k.launchAt).toBeLessThan(0.1);
  });

  it('with chance 0, or a perfect keeper, he never guesses', () => {
    expect(strike(0, 0).guess).toBe(0);
    expect(strike(1, 1).guess).toBe(0);
    expect(strike(0, 0).phase).toBe('ready');
  });

  it('guessers are on the move long before a normal keeper has reacted', () => {
    const play = (chance: number) => {
      const s = createSim(makeCfg({ keeperPresent: true, keeperSkill: 0 }, { keeper: { earlyGuessChance: chance } }), 3);
      setSpot(s, -6, 24);
      s.aim = { x: 1, y: 1.2 };
      const hold = { ...emptyInput(), shoot: true };
      step(s, hold, DT);
      while (s.meter < sweet) step(s, hold, DT);
      while (s.phase !== 'flight') step(s, emptyInput(), DT);
      const start = s.keeper!.b.x;
      while (s.flightT < 0.25) step(s, emptyInput(), DT);
      return Math.abs(s.keeper!.b.x - start);
    };
    expect(play(1)).toBeGreaterThan(0.3);
    expect(play(0)).toBe(0);
  });

  it('a wrong guess leaves the goal open', () => {
    let goals = 0;
    for (let seed = 1; seed <= 20; seed++) {
      const s = createSim(makeCfg({ keeperPresent: true, keeperSkill: 0 }, { keeper: { earlyGuessChance: 1 } }), seed * 5);
      setSpot(s, -6, 24);
      s.aim = { x: 0, y: 1.2 }; // dead centre: either direction of dive is wrong
      if (takeShot(s, sweet, { contactX: 0.3 })?.type === 'goal') goals++;
    }
    expect(goals).toBeGreaterThan(8);
  });
});

describe('Rebounds', () => {
  /** A hard shot straight at a standing keeper: he gets a hand to it and spills it back into the box. */
  const parried = (settings = {}, seed = 1, setup?: (s: SimState) => void) => {
    const s = createSim(makeCfg({ keeperPresent: true, rebounds: true, ...settings }, { keeper: { earlyGuessChance: 0 } }), seed);
    setSpot(s, 0, 24);
    setup?.(s);
    s.phase = 'flight';
    s.keeper!.phase = 'watching';
    s.ball.pos = { x: s.keeper!.b.x, y: 1.2, z: 2 };
    s.ball.vel = { x: 0, y: 0, z: -28 };
    s.prevBallPos = { ...s.ball.pos };
    for (let i = 0; i < 240 * 4 && (s.phase as string) !== 'result'; i++) step(s, emptyInput(), DT);
    return s;
  };
  const wait = (s: SimState) => {
    for (let i = 0; i < 240 * 5 && s.phase === 'result'; i++) step(s, emptyInput(), DT);
    return s;
  };

  it('a parry that stays in the box is flagged as a rebound', () => {
    const s = parried();
    expect(s.result?.type).toBe('saved');
    expect(s.result?.keeper).toBe('touch');
    expect(s.result?.rebound).toBe(true);
  });

  it('after the result you get a follow-up from where the ball ended up, with no wall', () => {
    const s = parried();
    // The ball keeps rolling while the result is shown; the follow-up is taken where it lies when that ends.
    let last = { ...s.ball.pos };
    for (let i = 0; i < 240 * 6 && s.phase === 'result'; i++) {
      last = { ...s.ball.pos };
      step(s, emptyInput(), DT);
    }
    expect(s.phase).toBe('aim');
    expect(s.followUp).toBe(true);
    expect(s.reboundCount).toBe(1);
    expect(s.wall).toBeNull();
    expect(s.keeper).not.toBeNull();
    expect(s.keeper!.phase).toBe('ready'); // up on his feet again
    expect(Math.abs(s.spot.x - last.x)).toBeLessThan(0.5);
    expect(Math.abs(s.spot.z - last.z)).toBeLessThan(0.5);
    expect(s.spot.z).toBeGreaterThanOrEqual(2.5);
    expect(s.spot.z).toBeLessThanOrEqual(AI.keeper.reboundZMax);
  });

  it('the follow-up can be taken, and counts as an attempt', () => {
    const s = parried({ wallEnabled: true });
    wait(s);
    const before = s.stats.attempts;
    s.aim = { x: 2.5, y: 0.6 };
    takeShot(s, sweet, { contactX: 0.3 });
    expect(s.stats.attempts).toBe(before + 1);
  });

  it('rebounds can be switched off', () => {
    const s = parried({ rebounds: false });
    expect(s.result?.rebound).toBe(false);
    wait(s);
    expect(s.followUp).toBe(false);
    expect(s.reboundCount).toBe(0);
  });

  it('is capped at the configured number of consecutive rebounds', () => {
    const s = parried({}, 1, (x) => (x.reboundCount = AI.keeper.maxRebounds));
    expect(s.result?.rebound).toBe(false);
  });

  it('a held ball is not a rebound', () => {
    const s = createSim(makeCfg({ keeperPresent: true, rebounds: true }), 1);
    setSpot(s, 0, 24);
    s.phase = 'flight';
    s.keeper!.phase = 'watching';
    s.ball.pos = { x: s.keeper!.b.x, y: 1.3, z: 2 };
    s.ball.vel = { x: 0, y: 0, z: -8 };
    s.prevBallPos = { ...s.ball.pos };
    for (let i = 0; i < 240 * 3 && (s.phase as string) !== 'result'; i++) step(s, emptyInput(), DT);
    expect(s.result?.keeper).toBe('hold');
    expect(s.result?.rebound).toBe(false);
  });

  it('a normal new spot clears the follow-up state', () => {
    const s = parried();
    wait(s);
    expect(s.followUp).toBe(true);
    setSpot(s, 5, 25);
    expect(s.followUp).toBe(false);
    expect(s.reboundCount).toBe(0);
  });
});
