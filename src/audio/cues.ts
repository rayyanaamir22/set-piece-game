import type { SimEvent } from '../sim/Sim';
import type { SimState } from '../sim/Sim';
import { clamp } from '../sim/math/vec3';

export type CueName =
  | 'strike'
  | 'bounce'
  | 'post'
  | 'bar'
  | 'net'
  | 'wall'
  | 'keeperCatch'
  | 'keeperParry'
  | 'whistle'
  | 'roar'
  | 'groan'
  | 'gasp'
  | 'ooh';

export interface Cue {
  name: CueName;
  /** 0..1 before volume settings. */
  gain: number;
}

/** Cues that come from the crowd (scaled by the crowd volume as well as master). */
export const CROWD_CUES: CueName[] = ['roar', 'groan', 'gasp', 'ooh'];

export interface Volumes {
  master: number;
  crowd: number;
  muted: boolean;
}

/** Which sounds a sim event makes, scaled by how hard the impact was. */
export function cuesFor(e: SimEvent): Cue[] {
  switch (e.type) {
    case 'strike':
      return [{ name: 'strike', gain: clamp(0.25 + e.speed / 40, 0.2, 1) }];
    case 'bounce':
      return e.speed > 2 ? [{ name: 'bounce', gain: clamp(e.speed / 30, 0.05, 0.6) }] : [];
    case 'post':
      return [{ name: 'post', gain: clamp(e.speed / 25, 0.3, 1) }, { name: 'gasp', gain: 0.6 }];
    case 'bar':
      return [{ name: 'bar', gain: clamp(e.speed / 25, 0.3, 1) }, { name: 'gasp', gain: 0.6 }];
    case 'net':
      return [{ name: 'net', gain: clamp(e.speed / 30, 0.1, 0.7) }];
    case 'wall':
      return [{ name: 'wall', gain: clamp(e.speed / 25, 0.2, 0.9) }];
    case 'keeper': {
      const soft = !!e.hands && (e.preSpeed ?? Infinity) < 20;
      return [{ name: soft ? 'keeperCatch' : 'keeperParry', gain: clamp(e.speed / 20, 0.3, 1) }];
    }
    case 'goal':
      return [{ name: 'roar', gain: 1 }];
    case 'result':
      if (e.result === 'saved') return [{ name: 'ooh', gain: 0.7 }];
      if (e.result === 'retake') return [{ name: 'whistle', gain: 0.8 }];
      if (e.result === 'goal') return [];
      return [{ name: 'groan', gain: 0.55 }];
    default:
      return [];
  }
}

/** Final loudness of a cue after the master/crowd sliders and mute. */
export function cueGain(cue: Cue, v: Volumes): number {
  if (v.muted) return 0;
  const m = clamp(v.master, 0, 1);
  return CROWD_CUES.includes(cue.name) ? cue.gain * m * clamp(v.crowd, 0, 1) : cue.gain * m;
}

/** How worked up the crowd is (0..1): rises as the ball closes on goal, drops once the attempt is over. */
export function crowdExcitement(s: Pick<SimState, 'phase' | 'ball' | 'spot'>): number {
  if (s.phase === 'runup') return 0.3;
  if (s.phase !== 'flight') return 0.12;
  const total = Math.max(1, Math.hypot(s.spot.x, s.spot.z));
  const left = Math.max(0, Math.hypot(s.ball.pos.x, s.ball.pos.z));
  return clamp(0.35 + 0.65 * (1 - left / total), 0.2, 1);
}
