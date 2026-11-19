import { dirToGoal, rightOf } from '../sim/layout';
import { V3, clamp, copy, lerp, v3 } from '../sim/math/vec3';
import type { KeeperState } from '../sim/Keeper';
import type { Phase, SimEvent, SimState } from '../sim/Sim';
import type { WallState } from '../sim/Wall';

/** Fixed-capacity ring buffer: pushing past capacity drops the oldest item. */
export class RingBuffer<T> {
  private items: T[] = [];
  private head = 0;
  constructor(readonly capacity: number) {}
  get length(): number {
    return this.items.length;
  }
  push(x: T): void {
    if (this.items.length < this.capacity) this.items.push(x);
    else {
      this.items[this.head] = x;
      this.head = (this.head + 1) % this.capacity;
    }
  }
  /** Oldest first. */
  toArray(): T[] {
    return [...this.items.slice(this.head), ...this.items.slice(0, this.head)];
  }
  clear(): void {
    this.items = [];
    this.head = 0;
  }
}

export interface FrameKeeper {
  a: V3;
  b: V3;
  startB: V3;
  endB: V3;
  phase: KeeperState['phase'];
  kind: KeeperState['kind'];
  t0: number;
  dur: number;
}

export interface FrameWall {
  players: V3[];
  lift: number[];
  lying: WallState['lying'];
}

/** What the renderer needs to redraw one instant of an attempt. */
export interface ReplayFrame {
  /** Seconds since the run-up started. */
  t: number;
  ball: V3;
  spin: V3;
  phase: Phase;
  runupT: number;
  phaseT: number;
  flightT: number;
  wall: FrameWall | null;
  keeper: FrameKeeper | null;
  /** Sim events (strike, post, net, goal…) that happened since the previous frame. */
  events: SimEvent[];
}

export interface ReplayMeta {
  spot: V3;
  foot: 'R' | 'L';
  runup: { angle: number; dist: number };
  runupDur: number;
  dummy: boolean;
  dummyAt: number;
  /** Static wall data (positions/lift are per frame). */
  wall: Pick<WallState, 'height' | 'radius' | 'facing' | 'axis' | 'base' | 'encroachers'> | null;
  keeperPresent: boolean;
  resultType: string | null;
  keeperTouch: string | null;
}

export interface Recording {
  meta: ReplayMeta;
  frames: ReplayFrame[];
  duration: number;
}

export const REPLAY_HZ = 60;
const CAPACITY = REPLAY_HZ * 12;
const TAIL = 1.2; // seconds to keep recording after the result

const snapWall = (w: WallState): FrameWall => ({
  players: w.players.map(copy),
  lift: [...w.lift],
  lying: w.lying ? { a: copy(w.lying.a), b: copy(w.lying.b), radius: w.lying.radius } : null,
});

const snapKeeper = (k: KeeperState): FrameKeeper => ({
  a: copy(k.a),
  b: copy(k.b),
  startB: copy(k.startB),
  endB: copy(k.endB),
  phase: k.phase,
  kind: k.kind,
  t0: k.t0,
  dur: k.dur,
});

/**
 * Records the last attempt (run-up -> flight -> result) at 60 Hz into a ring
 * buffer of renderer-ready frames. Call `onStep` after every sim step and
 * `frameDone` after the frame's events have been consumed (cleared).
 */
export class Recorder {
  private ring = new RingBuffer<ReplayFrame>(CAPACITY);
  private meta: ReplayMeta | null = null;
  private active = false;
  private t = 0;
  private steps = 0;
  private cursor = 0;
  private pending: SimEvent[] = [];
  private stride: number;

  constructor(simHz = 240) {
    this.stride = Math.max(1, Math.round(simHz / REPLAY_HZ));
  }

  get recording(): Recording | null {
    if (this.active || !this.meta) return null;
    const frames = this.ring.toArray();
    return frames.length > 1 ? { meta: this.meta, frames, duration: frames[frames.length - 1].t } : null;
  }

  /** True while an attempt is being captured. */
  get isRecording(): boolean {
    return this.active;
  }

  onStep(s: SimState, dt: number): void {
    // New events since the last step (the sim's list is cleared once per rendered frame).
    if (s.events.length < this.cursor) this.cursor = 0;
    const fresh = s.events.slice(this.cursor);
    this.cursor = s.events.length;

    if (!this.active) {
      if (s.phase === 'runup' && s.runupT <= dt * 1.5) this.begin(s);
      else return;
    }
    this.pending.push(...fresh);
    this.t += dt;
    this.steps++;

    const ending = s.phase === 'aim' || (s.phase === 'result' && s.phaseT > TAIL);
    if (this.steps % this.stride === 0 || ending) this.sample(s);
    if (ending) this.end(s);
  }

  /** Call once per rendered frame, after the sim's event list has been drained. */
  frameDone(): void {
    this.cursor = 0;
  }

  private begin(s: SimState): void {
    this.ring.clear();
    this.t = 0;
    this.steps = 0;
    this.pending = [];
    this.active = true;
    this.meta = {
      spot: copy(s.spot),
      foot: s.foot,
      runup: { ...s.runup },
      runupDur: s.runupDur,
      dummy: s.dummy,
      dummyAt: s.dummyAt,
      wall: s.wall
        ? { height: s.wall.height, radius: s.wall.radius, facing: copy(s.wall.facing), axis: copy(s.wall.axis), base: s.wall.base.map(copy), encroachers: [...s.wall.encroachers] }
        : null,
      keeperPresent: !!s.keeper,
      resultType: null,
      keeperTouch: null,
    };
    this.sample(s);
  }

  private sample(s: SimState): void {
    this.ring.push({
      t: this.t,
      ball: copy(s.ball.pos),
      spin: copy(s.ball.spin),
      phase: s.phase,
      runupT: s.runupT,
      phaseT: s.phaseT,
      flightT: s.flightT,
      wall: s.wall ? snapWall(s.wall) : null,
      keeper: s.keeper ? snapKeeper(s.keeper) : null,
      events: this.pending,
    });
    this.pending = [];
  }

  private end(s: SimState): void {
    this.active = false;
    if (this.meta) {
      this.meta.resultType = s.result?.type ?? null;
      this.meta.keeperTouch = s.result?.keeper ?? null;
    }
  }
}

// ---------------------------------------------------------------- playback

export interface Sample extends Omit<ReplayFrame, 'events' | 't'> {
  /** Index of the earlier frame this sample lies after. */
  i: number;
  t: number;
}

const lerpV = (a: V3, b: V3, k: number): V3 => ({ x: lerp(a.x, b.x, k), y: lerp(a.y, b.y, k), z: lerp(a.z, b.z, k) });

/** The state of the attempt at `time` seconds, interpolated between recorded frames. */
export function sampleAt(rec: Recording, time: number): Sample {
  const f = rec.frames;
  const t = clamp(time, 0, rec.duration);
  let i = 0;
  while (i < f.length - 2 && f[i + 1].t <= t) i++;
  const a = f[i];
  const b = f[Math.min(i + 1, f.length - 1)];
  const k = b.t > a.t ? clamp((t - a.t) / (b.t - a.t), 0, 1) : 0;
  return {
    i,
    t,
    ball: lerpV(a.ball, b.ball, k),
    spin: lerpV(a.spin, b.spin, k),
    phase: a.phase,
    runupT: lerp(a.runupT, b.runupT, a.phase === b.phase ? k : 0),
    phaseT: lerp(a.phaseT, b.phaseT, a.phase === b.phase ? k : 0),
    flightT: lerp(a.flightT, b.flightT, k),
    wall:
      a.wall && b.wall
        ? {
            players: a.wall.players.map((p, j) => lerpV(p, b.wall!.players[j], k)),
            lift: a.wall.lift.map((l, j) => lerp(l, b.wall!.lift[j], k)),
            lying: a.wall.lying,
          }
        : a.wall,
    keeper:
      a.keeper && b.keeper
        ? { ...a.keeper, a: lerpV(a.keeper.a, b.keeper.a, k), b: lerpV(a.keeper.b, b.keeper.b, k) }
        : a.keeper,
  };
}

export class ReplayPlayer {
  time = 0;
  paused = false;
  /** Seconds to hold on the last frame before looping. */
  private hold = 0;
  constructor(
    readonly rec: Recording,
    public speed = 0.5,
    readonly loop = true,
  ) {}

  get finished(): boolean {
    return this.time >= this.rec.duration;
  }

  restart(): void {
    this.time = 0;
    this.hold = 0;
  }

  /** Advance by real seconds; `onEvent` receives each recorded event once, as playback passes it. */
  advance(realDt: number, onEvent?: (e: SimEvent) => void): void {
    if (this.paused) return;
    if (this.finished) {
      this.hold += realDt;
      if (this.loop && this.hold > 0.9) this.restart();
      return;
    }
    const prev = this.time;
    this.time = Math.min(this.rec.duration, this.time + realDt * this.speed);
    if (onEvent) {
      for (const fr of this.rec.frames) if (fr.t > prev && fr.t <= this.time) fr.events.forEach(onEvent);
      if (prev === 0) this.rec.frames[0].events.forEach(onEvent);
    }
  }

  sample(): Sample {
    return sampleAt(this.rec, this.time);
  }
}

/** A SimState-shaped object that makes the live renderer draw the recorded instant. */
export function viewStateFor(sim: SimState, rec: Recording, f: Sample): SimState {
  const m = rec.meta;
  const wall = m.wall && f.wall ? ({ ...m.wall, players: f.wall.players, lift: f.wall.lift, lying: f.wall.lying } as unknown as WallState) : null;
  const keeper = f.keeper ? ({ ...(f.keeper as object), endA: f.keeper.a, startA: f.keeper.a, skill: 0, decideAt: 0, target: null, launchAt: 0, guess: 0 } as unknown as KeeperState) : null;
  return {
    ...sim,
    spot: m.spot,
    foot: m.foot,
    runup: m.runup,
    runupDur: m.runupDur,
    dummy: m.dummy,
    dummyAt: m.dummyAt,
    phase: f.phase,
    runupT: f.runupT,
    phaseT: f.phaseT,
    flightT: f.flightT,
    ball: { ...sim.ball, pos: f.ball, spin: f.spin },
    prevBallPos: f.ball,
    wall,
    keeper,
    preview: null,
    events: [],
  } as SimState;
}

// ---------------------------------------------------------------- cameras

export type CamKind = 'broadcast' | 'goal' | 'side' | 'keeper' | 'overhead';

export const CAM_LABEL: Record<CamKind, string> = {
  broadcast: 'Broadcast',
  goal: 'Behind goal',
  side: 'Side on',
  keeper: 'Keeper',
  overhead: 'Overhead',
};

/** Camera order for this attempt: a keeper-focused view first when he made the save. */
export function camerasFor(meta: ReplayMeta): CamKind[] {
  if (meta.resultType === 'saved' && meta.keeperPresent) return ['keeper', 'broadcast', 'goal', 'side'];
  if (meta.resultType === 'goal') return ['broadcast', 'goal', 'side', 'overhead'];
  return ['broadcast', 'side', 'overhead'];
}

export function replayCamera(kind: CamKind, meta: ReplayMeta, f: Sample): { pos: V3; look: V3 } {
  const spot = meta.spot;
  const ball = f.ball;
  const d = dirToGoal(spot);
  const r = rightOf(d);
  const sign = spot.x >= 0 ? 1 : -1;
  switch (kind) {
    case 'goal':
      return { pos: v3(clamp(spot.x * 0.25, -3, 3), 1.6, -4.5), look: v3(ball.x, Math.max(ball.y, 0.9), ball.z) };
    case 'side':
      return { pos: v3(-sign * 17, 2.6, Math.max(8, spot.z * 0.45)), look: v3(ball.x, Math.max(ball.y, 0.8), ball.z) };
    case 'keeper': {
      // Frames the keeper and the goal; follows the ball in only as it arrives.
      const kb = f.keeper ? f.keeper.b : v3(0, 1.5, 0.8);
      const w = clamp((10 - ball.z) / 7, 0, 1);
      return {
        pos: v3(kb.x + sign * 3.0, 1.4, 8),
        look: v3(lerp(kb.x, ball.x, w), lerp(Math.max(1.0, kb.y), Math.max(0.6, ball.y), w), lerp(kb.z, Math.max(ball.z, 0), w)),
      };
    }
    case 'overhead':
      return { pos: v3(spot.x * 0.45, 26, spot.z * 0.55 + 4), look: v3(ball.x * 0.7, 0, Math.max(ball.z, 0) * 0.8 + 2) };
    default:
      return {
        pos: v3(spot.x - d.x * 7 + r.x * (meta.foot === 'R' ? 1 : -1), 2.5, spot.z - d.z * 7 + r.z * (meta.foot === 'R' ? 1 : -1)),
        look: v3(ball.x, Math.max(ball.y, 0.9), ball.z),
      };
  }
}
