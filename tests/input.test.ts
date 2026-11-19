import { describe, expect, it } from 'vitest';
import { CROWD_CUES, cueGain, crowdExcitement, cuesFor } from '../src/audio/cues';
import { ACTIONS, RESERVED_KEYS, conflicts, defaultKeymap, isHeld, keyLabel, rebind, sanitizeKeymap } from '../src/input/keymap';
import { AIM_M_PER_PX, SENS_MAX, SENS_MIN, aimDeltaFromMouse, clampSens, moveContactCursor } from '../src/input/mouse';
import { defaultPrefs, sanitizePrefs } from '../src/prefs';
import { SimEvent } from '../src/sim/Sim';

describe('Mouse helpers', () => {
  it('moving right/up moves the reticle right/up (screen y grows downward)', () => {
    const d = aimDeltaFromMouse(100, -50, 1);
    expect(d.x).toBeCloseTo(100 * AIM_M_PER_PX, 9);
    expect(d.y).toBeCloseTo(50 * AIM_M_PER_PX, 9);
    expect(aimDeltaFromMouse(0, 40, 1).y).toBeLessThan(0);
  });
  it('sensitivity scales linearly and is clamped', () => {
    expect(aimDeltaFromMouse(100, 0, 2).x).toBeCloseTo(2 * aimDeltaFromMouse(100, 0, 1).x, 9);
    expect(clampSens(99)).toBe(SENS_MAX);
    expect(clampSens(0)).toBe(SENS_MIN);
    expect(clampSens(NaN)).toBe(1);
  });
  it('the contact cursor never leaves the unit disc, and mouse up moves it up', () => {
    let c = { x: 0, y: 0 };
    c = moveContactCursor(c, 0, -100, 1);
    expect(c.y).toBeGreaterThan(0);
    for (let i = 0; i < 50; i++) c = moveContactCursor(c, 300, -300, 1);
    expect(Math.hypot(c.x, c.y)).toBeLessThanOrEqual(1 + 1e-9);
  });
});

describe('Keymap', () => {
  it('ships a unique default key for every action (no accidental clashes)', () => {
    const used = new Map<string, string>();
    for (const a of ACTIONS) {
      for (const c of a.defaults) {
        expect(used.has(c), `${c} used by ${used.get(c)} and ${a.id}`).toBe(false);
        used.set(c, a.id);
      }
    }
    expect(ACTIONS.some((a) => a.defaults.some((c) => RESERVED_KEYS.includes(c)))).toBe(false);
  });

  it('rebinding replaces the primary key and keeps the secondary one', () => {
    const m = rebind(defaultKeymap(), 'aimLeft', 'KeyH');
    expect(m.aimLeft).toEqual(['KeyH', 'ArrowLeft']);
    expect(isHeld(m, new Set(['KeyH']), 'aimLeft')).toBe(true);
    expect(isHeld(m, new Set(['KeyA']), 'aimLeft')).toBe(false);
  });

  it('a key taken from another action is removed from it', () => {
    const m = rebind(defaultKeymap(), 'camera', 'KeyF'); // F was "switch foot"
    expect(m.camera[0]).toBe('KeyF');
    expect(conflicts(m, 'KeyF')).toEqual(['camera']);
    expect(m.toggleFoot).not.toContain('KeyF');
    expect(m.toggleFoot).toEqual(['KeyC']); // swapped: it takes the key camera gave up
  });

  it('every action is still bound after any single rebind', () => {
    for (const a of ACTIONS) {
      const m = rebind(defaultKeymap(), a.id, 'KeyY');
      for (const b of ACTIONS) expect(m[b.id].length).toBeGreaterThan(0);
      expect(conflicts(m, 'KeyY')).toEqual([a.id]);
    }
  });

  it('Escape cannot be bound', () => {
    const m = defaultKeymap();
    expect(rebind(m, 'shoot', 'Escape')).toBe(m);
    expect(sanitizeKeymap({ shoot: ['Escape'] }).shoot).toEqual(['Space']);
  });

  it('sanitize repairs garbage', () => {
    expect(sanitizeKeymap(null)).toEqual(defaultKeymap());
    expect(sanitizeKeymap({ shoot: 'x', aimLeft: [], driven: [1, 2] })).toEqual(defaultKeymap());
    const ok = sanitizeKeymap({ shoot: ['Enter'], bogus: ['KeyQ'] });
    expect(ok.shoot).toEqual(['Enter']);
    expect((ok as Record<string, unknown>).bogus).toBeUndefined();
  });

  it('labels keys for humans', () => {
    expect(keyLabel('KeyA')).toBe('A');
    expect(keyLabel('ArrowLeft')).toBe('←');
    expect(keyLabel('BracketRight')).toBe(']');
    expect(keyLabel('Digit3')).toBe('3');
    expect(keyLabel('ShiftRight')).toBe('Shift');
    expect(keyLabel('Space')).toBe('Space');
  });
});

describe('Client prefs', () => {
  it('defaults are valid and stable through sanitize', () => {
    expect(sanitizePrefs(defaultPrefs())).toEqual(defaultPrefs());
    expect(sanitizePrefs(undefined)).toEqual(defaultPrefs());
    expect(sanitizePrefs('nonsense')).toEqual(defaultPrefs());
  });
  it('clamps numbers and rejects bad enums', () => {
    const p = sanitizePrefs({ mouseSensitivity: 99, masterVolume: -3, cameraDistance: 0, quality: 'ultra', timeOfDay: 'night', replaySpeed: 7, mode: 'x', difficulty: 'pro' });
    expect(p.mouseSensitivity).toBe(3);
    expect(p.masterVolume).toBe(0);
    expect(p.cameraDistance).toBe(0.6);
    expect(p.quality).toBe('high');
    expect(p.timeOfDay).toBe('night');
    expect(p.replaySpeed).toBe(0.5);
    expect(p.mode).toBe('free');
    expect(p.difficulty).toBe('pro');
  });
  it('keeps valid medals/bests and drops junk', () => {
    const p = sanitizePrefs({ medals: { a: 'gold', b: 'platinum', c: null }, best: { a: 12, b: 'x' }, collapsed: { s: true, t: 1 } });
    expect(p.medals).toEqual({ a: 'gold', c: null });
    expect(p.best).toEqual({ a: 12 });
    expect(p.collapsed).toEqual({ s: true });
  });
  it('round-trips through JSON', () => {
    const p = { ...defaultPrefs(), mouseSensitivity: 1.7, quality: 'low' as const, timeOfDay: 'dusk' as const };
    expect(sanitizePrefs(JSON.parse(JSON.stringify(p)))).toEqual(p);
  });
});

describe('Audio cues', () => {
  const ev = (e: unknown) => e as SimEvent;
  const pos = { x: 0, y: 0, z: 0 };

  it('a harder strike is louder', () => {
    expect(cuesFor(ev({ type: 'strike', speed: 33 }))[0].gain).toBeGreaterThan(cuesFor(ev({ type: 'strike', speed: 14 }))[0].gain);
  });
  it('bounces too soft to hear make no sound', () => {
    expect(cuesFor(ev({ type: 'bounce', speed: 1.5, pos }))).toEqual([]);
    expect(cuesFor(ev({ type: 'bounce', speed: 9, pos }))[0].name).toBe('bounce');
  });
  it('woodwork rings and the crowd gasps', () => {
    const post = cuesFor(ev({ type: 'post', speed: 20, pos }));
    expect(post.map((c) => c.name)).toEqual(['post', 'gasp']);
    expect(cuesFor(ev({ type: 'bar', speed: 20, pos })).map((c) => c.name)).toEqual(['bar', 'gasp']);
  });
  it('a goal roars; a save is an "ooh"; a miss groans; a retake blows the whistle', () => {
    expect(cuesFor(ev({ type: 'goal' }))[0].name).toBe('roar');
    expect(cuesFor(ev({ type: 'result', result: 'saved' }))[0].name).toBe('ooh');
    for (const r of ['wide', 'over', 'miss']) expect(cuesFor(ev({ type: 'result', result: r }))[0].name).toBe('groan');
    expect(cuesFor(ev({ type: 'result', result: 'retake' }))[0].name).toBe('whistle');
    expect(cuesFor(ev({ type: 'result', result: 'goal' }))).toEqual([]); // the roar already covers it
  });
  it('a soft touch to the hands is a catch, a hard one a parry', () => {
    expect(cuesFor(ev({ type: 'keeper', speed: 8, pos, preSpeed: 12, hands: true }))[0].name).toBe('keeperCatch');
    expect(cuesFor(ev({ type: 'keeper', speed: 20, pos, preSpeed: 28, hands: true }))[0].name).toBe('keeperParry');
    expect(cuesFor(ev({ type: 'keeper', speed: 8, pos, preSpeed: 12, hands: false }))[0].name).toBe('keeperParry');
  });
  it('every gain is within 0..1 for any speed', () => {
    for (const type of ['strike', 'bounce', 'post', 'bar', 'net', 'wall', 'keeper']) {
      for (const speed of [0, 3, 12, 40, 200]) {
        for (const c of cuesFor(ev({ type, speed, pos, preSpeed: speed, hands: true }))) {
          expect(c.gain).toBeGreaterThanOrEqual(0);
          expect(c.gain).toBeLessThanOrEqual(1);
        }
      }
    }
  });
  it('volume: master scales everything, crowd only the crowd, mute silences', () => {
    const fx = { name: 'strike' as const, gain: 0.8 };
    const crowd = { name: 'roar' as const, gain: 1 };
    expect(CROWD_CUES).toContain('roar');
    expect(cueGain(fx, { master: 0.5, crowd: 0, muted: false })).toBeCloseTo(0.4, 9);
    expect(cueGain(crowd, { master: 0.5, crowd: 0, muted: false })).toBe(0);
    expect(cueGain(crowd, { master: 0.5, crowd: 0.5, muted: false })).toBeCloseTo(0.25, 9);
    expect(cueGain(fx, { master: 1, crowd: 1, muted: true })).toBe(0);
  });
  it('the crowd gets more excited as the ball nears the goal', () => {
    const at = (z: number) => crowdExcitement({ phase: 'flight', ball: { pos: { x: 0, y: 1, z } } as never, spot: { x: 0, y: 0, z: 24 } });
    expect(at(2)).toBeGreaterThan(at(18));
    expect(crowdExcitement({ phase: 'aim', ball: { pos: pos } as never, spot: pos })).toBeLessThan(at(18));
  });
});
