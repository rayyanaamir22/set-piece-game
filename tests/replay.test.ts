import { describe, expect, it } from 'vitest';
import { CAM_LABEL, CamKind, REPLAY_HZ, Recorder, ReplayPlayer, RingBuffer, camerasFor, replayCamera, sampleAt, viewStateFor } from '../src/replay/Replay';
import { SimState, createSim, emptyInput, setSpot, step } from '../src/sim/Sim';
import { DT, makeCfg, sweet } from './helpers';

/** Play one attempt, feeding the recorder like the main loop does. */
function record(s: SimState, rec = new Recorder(), wait = 6) {
  const hold = { ...emptyInput(), shoot: true };
  const run = (input: ReturnType<typeof emptyInput>) => {
    step(s, input, DT);
    rec.onStep(s, DT);
    // the main loop drains events once per frame (every 4 steps here)
    if (Math.round(s.phaseT / DT) % 4 === 0) {
      s.events.length = 0;
      rec.frameDone();
    }
  };
  run(hold);
  while (s.meter < sweet) run(hold);
  run(emptyInput());
  for (let i = 0; i < 240 * wait && rec.isRecording; i++) run(emptyInput());
  return rec;
}

const fresh = (settings = {}, seed = 3) => {
  const s = createSim(makeCfg(settings), seed);
  setSpot(s, -6, 24);
  s.aim = { x: 2, y: 1.5 };
  return s;
};

describe('RingBuffer', () => {
  it('keeps the newest items, oldest first, once full', () => {
    const r = new RingBuffer<number>(3);
    [1, 2].forEach((x) => r.push(x));
    expect(r.toArray()).toEqual([1, 2]);
    [3, 4, 5].forEach((x) => r.push(x));
    expect(r.toArray()).toEqual([3, 4, 5]);
    expect(r.length).toBe(3);
    r.clear();
    expect(r.toArray()).toEqual([]);
  });
});

describe('Recorder', () => {
  it('records the attempt at 60 Hz, from the start of the run-up to just after the result', () => {
    const s = fresh();
    const rec = record(s);
    const r = rec.recording!;
    expect(r).not.toBeNull();
    expect(r.frames[0].phase).toBe('runup');
    expect(r.frames[0].t).toBeLessThan(0.1);
    expect(r.frames.at(-1)!.phase).toBe('result');
    // uniform ~60 Hz spacing
    const gaps = r.frames.slice(1, -1).map((f, i) => f.t - r.frames[i].t);
    gaps.forEach((g) => expect(g).toBeCloseTo(1 / REPLAY_HZ, 4));
    // runup (0.55 s) + flight + result tail, but well under the buffer capacity
    expect(r.duration).toBeGreaterThan(1.5);
    expect(r.duration).toBeLessThan(8);
    expect(r.frames.length).toBeGreaterThan(90);
  });

  it('captures the meta needed to redraw the scene', () => {
    const s = fresh({ wallEnabled: true, keeperPresent: true });
    const r = record(s).recording!;
    expect(r.meta.spot).toEqual({ x: -6, y: 0, z: 24 });
    expect(r.meta.wall).not.toBeNull();
    expect(r.meta.keeperPresent).toBe(true);
    expect(r.meta.resultType).toBe(s.result!.type);
    expect(r.frames[0].wall!.players.length).toBe(s.wall!.players.length);
  });

  it('records each event exactly once, in order', () => {
    const s = fresh();
    const r = record(s).recording!;
    const types = r.frames.flatMap((f) => f.events.map((e) => e.type));
    expect(types.filter((t) => t === 'strike')).toHaveLength(1);
    expect(types.filter((t) => t === 'goal').length).toBeLessThanOrEqual(1);
    expect(types).toContain('result');
    expect(types.indexOf('strike')).toBeLessThan(types.indexOf('result'));
  });

  it('the last frame is where the ball ended up', () => {
    const s = fresh();
    const r = record(s).recording!;
    const last = r.frames.at(-1)!.ball;
    expect(Math.hypot(last.x - s.ball.pos.x, last.y - s.ball.pos.y, last.z - s.ball.pos.z)).toBeLessThan(0.5);
  });

  it('a new attempt replaces the previous recording', () => {
    const s = fresh();
    const rec = new Recorder();
    record(s, rec);
    const first = rec.recording!.meta.spot;
    setSpot(s, 5, 26);
    s.aim = { x: -1, y: 1.2 };
    record(s, rec);
    expect(rec.recording!.meta.spot).not.toEqual(first);
    expect(rec.recording!.meta.spot).toEqual({ x: 5, y: 0, z: 26 });
  });

  it('has nothing to show before the first attempt, or while one is in progress', () => {
    const rec = new Recorder();
    expect(rec.recording).toBeNull();
    const s = fresh();
    const hold = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, emptyInput(), DT);
    rec.onStep(s, DT);
    expect(rec.recording).toBeNull();
  });

  it('finalises if the player resets the attempt early', () => {
    const s = fresh();
    const rec = new Recorder();
    const hold = { ...emptyInput(), shoot: true };
    step(s, hold, DT);
    while (s.meter < sweet) step(s, hold, DT);
    step(s, emptyInput(), DT);
    rec.onStep(s, DT);
    for (let i = 0; i < 240 * 0.8; i++) {
      step(s, emptyInput(), DT);
      rec.onStep(s, DT);
    }
    s.phase = 'aim'; // e.g. a new spot
    step(s, emptyInput(), DT);
    rec.onStep(s, DT);
    expect(rec.recording).not.toBeNull();
  });
});

describe('Playback', () => {
  const rec = record(fresh({ wallEnabled: true, keeperPresent: true })).recording!;

  it('interpolates between recorded frames', () => {
    const f = rec.frames;
    const mid = (f[10].t + f[11].t) / 2;
    const s = sampleAt(rec, mid);
    expect(s.ball.x).toBeCloseTo((f[10].ball.x + f[11].ball.x) / 2, 6);
    expect(s.ball.y).toBeCloseTo((f[10].ball.y + f[11].ball.y) / 2, 6);
    expect(sampleAt(rec, 0).ball).toEqual(f[0].ball);
    expect(sampleAt(rec, 1e9).ball).toEqual(f.at(-1)!.ball);
    expect(sampleAt(rec, -5).ball).toEqual(f[0].ball);
  });

  it('plays at the chosen speed and fires each event once', () => {
    const p = new ReplayPlayer(rec, 0.5, false);
    const seen: string[] = [];
    for (let i = 0; i < 60 * 30 && !p.finished; i++) p.advance(1 / 60, (e) => seen.push(e.type));
    expect(p.finished).toBe(true);
    // slow-mo: it takes about twice the recording's length in real time
    expect(seen.filter((t) => t === 'strike')).toHaveLength(1);
    expect(seen.filter((t) => t === 'result')).toHaveLength(1);
    const real = new ReplayPlayer(rec, 1, false);
    let n = 0;
    while (!real.finished && n < 60 * 30) {
      real.advance(1 / 60);
      n++;
    }
    expect(n / 60).toBeCloseTo(rec.duration, 0);
  });

  it('pauses, restarts and loops', () => {
    const p = new ReplayPlayer(rec, 1, true);
    p.advance(0.5);
    const t = p.time;
    p.paused = true;
    p.advance(1);
    expect(p.time).toBe(t);
    p.paused = false;
    for (let i = 0; i < 60 * 40; i++) p.advance(1 / 60);
    // looped back at least once, so it is never stuck at the end
    expect(p.time).toBeLessThan(rec.duration);
  });

  it('builds a view state the renderer can use without touching the live sim', () => {
    const live = fresh({ wallEnabled: true, keeperPresent: true });
    const before = JSON.stringify(live.ball.pos);
    const f = sampleAt(rec, rec.duration * 0.4);
    const v = viewStateFor(live, rec, f);
    expect(v.ball.pos).toEqual(f.ball);
    expect(v.prevBallPos).toEqual(f.ball);
    expect(v.phase).toBe(f.phase);
    expect(v.spot).toEqual(rec.meta.spot);
    expect(v.wall!.players).toHaveLength(rec.frames[0].wall!.players.length);
    expect(v.wall!.height).toBe(rec.meta.wall!.height);
    expect(v.keeper!.b).toEqual(f.keeper!.b);
    expect(JSON.stringify(live.ball.pos)).toBe(before);
  });
});

describe('Replay cameras', () => {
  const s = fresh({ wallEnabled: true, keeperPresent: true });
  const rec = record(s).recording!;
  const kinds: CamKind[] = ['broadcast', 'goal', 'side', 'keeper', 'overhead'];

  it('every camera gives finite positions at every moment', () => {
    for (const k of kinds) {
      for (const f of [0, 0.3, 0.7, 1].map((u) => sampleAt(rec, rec.duration * u))) {
        const c = replayCamera(k, rec.meta, f);
        for (const v of [c.pos, c.look]) {
          expect(Number.isFinite(v.x + v.y + v.z)).toBe(true);
        }
        expect(c.pos.y).toBeGreaterThan(0);
      }
    }
  });

  it('places the cameras where they should be', () => {
    const f = sampleAt(rec, rec.duration * 0.5);
    expect(replayCamera('goal', rec.meta, f).pos.z).toBeLessThan(0); // behind the goal line
    const side = replayCamera('side', rec.meta, f).pos;
    expect(Math.sign(side.x)).toBe(-Math.sign(rec.meta.spot.x)); // opposite side of the ball
    const kc = replayCamera('keeper', rec.meta, f).pos;
    expect(Math.abs(kc.x - f.keeper!.b.x)).toBeLessThan(4.5); // beside him
    expect(kc.z).toBeGreaterThan(3); // out in front of the goal, so the ball arrives towards the camera
    expect(replayCamera('overhead', rec.meta, f).pos.y).toBeGreaterThan(15);
  });

  it('shows a keeper camera first for a save, and only offers it when there is a keeper', () => {
    const base = rec.meta;
    expect(camerasFor({ ...base, resultType: 'saved', keeperPresent: true })[0]).toBe('keeper');
    expect(camerasFor({ ...base, resultType: 'goal' })).not.toContain('keeper');
    expect(camerasFor({ ...base, resultType: 'saved', keeperPresent: false })).not.toContain('keeper');
    expect(camerasFor({ ...base, resultType: 'wide' })[0]).toBe('broadcast');
  });

  it('all cameras have labels', () => {
    for (const k of kinds) expect(CAM_LABEL[k]).toBeTruthy();
  });
});
