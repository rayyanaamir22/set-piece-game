import { Keymap, sanitizeKeymap } from './input/keymap';
import { DIFFICULTIES, DifficultyName } from './sim/presets';
import type { Medal } from './sim/challenge';
import type { Mode } from './sim/Sim';

export type Quality = 'low' | 'medium' | 'high';
export type TimeOfDay = 'day' | 'dusk' | 'night';

export const QUALITIES: Quality[] = ['low', 'medium', 'high'];
export const TIMES: TimeOfDay[] = ['day', 'dusk', 'night'];
export const REPLAY_SPEEDS = [0.25, 0.5, 1];

/** Everything that is about *your* setup rather than the rules of the free kick. */
export interface ClientPrefs {
  mouseAim: boolean;
  mouseSensitivity: number;
  masterVolume: number;
  crowdVolume: number;
  muted: boolean;
  quality: Quality;
  timeOfDay: TimeOfDay;
  /** Multiplier on the camera distance (1 = default). */
  cameraDistance: number;
  replaySpeed: number;
  autoReplay: boolean;
  keymap: Keymap;
  mode: Mode;
  takerIndex: number;
  challengeIndex: number;
  difficulty: DifficultyName | 'custom';
  /** Best medal and best points per challenge id. */
  medals: Record<string, Medal | null>;
  best: Record<string, number>;
  /** Settings sections the player collapsed. */
  collapsed: Record<string, boolean>;
}

export const defaultPrefs = (): ClientPrefs => ({
  mouseAim: true,
  mouseSensitivity: 1,
  masterVolume: 0.7,
  crowdVolume: 0.7,
  muted: false,
  quality: 'high',
  timeOfDay: 'day',
  cameraDistance: 1,
  replaySpeed: 0.5,
  autoReplay: false,
  keymap: sanitizeKeymap(null),
  mode: 'free',
  takerIndex: 0,
  challengeIndex: 0,
  difficulty: 'normal',
  medals: {},
  best: {},
  collapsed: {},
});

const num = (v: unknown, lo: number, hi: number, d: number) =>
  typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d;
const bool = (v: unknown, d: boolean) => (typeof v === 'boolean' ? v : d);
const pick = <T extends string>(v: unknown, all: readonly T[], d: T): T => (all.includes(v as T) ? (v as T) : d);

const MEDALS: (Medal | null)[] = ['bronze', 'silver', 'gold', null];

/** Accepts anything (stored JSON) and returns valid prefs. */
export function sanitizePrefs(raw: unknown): ClientPrefs {
  const d = defaultPrefs();
  if (!raw || typeof raw !== 'object') return d;
  const r = raw as Record<string, unknown>;
  const rec = <T,>(v: unknown, ok: (x: unknown) => boolean): Record<string, T> => {
    const out: Record<string, T> = {};
    if (v && typeof v === 'object') for (const [k, x] of Object.entries(v)) if (ok(x)) out[k] = x as T;
    return out;
  };
  return {
    mouseAim: bool(r.mouseAim, d.mouseAim),
    mouseSensitivity: num(r.mouseSensitivity, 0.2, 3, d.mouseSensitivity),
    masterVolume: num(r.masterVolume, 0, 1, d.masterVolume),
    crowdVolume: num(r.crowdVolume, 0, 1, d.crowdVolume),
    muted: bool(r.muted, d.muted),
    quality: pick(r.quality, QUALITIES, d.quality),
    timeOfDay: pick(r.timeOfDay, TIMES, d.timeOfDay),
    cameraDistance: num(r.cameraDistance, 0.6, 1.8, d.cameraDistance),
    replaySpeed: REPLAY_SPEEDS.includes(r.replaySpeed as number) ? (r.replaySpeed as number) : d.replaySpeed,
    autoReplay: bool(r.autoReplay, d.autoReplay),
    keymap: sanitizeKeymap(r.keymap),
    mode: pick(r.mode, ['free', 'practice', 'challenge'] as const, d.mode),
    takerIndex: Math.max(0, Math.floor(num(r.takerIndex, 0, 99, 0))),
    challengeIndex: Math.max(0, Math.floor(num(r.challengeIndex, 0, 99, 0))),
    difficulty: pick(r.difficulty, [...DIFFICULTIES, 'custom'] as const, d.difficulty),
    medals: rec<Medal | null>(r.medals, (x) => MEDALS.includes(x as never)),
    best: rec<number>(r.best, (x) => typeof x === 'number' && Number.isFinite(x)),
    collapsed: rec<boolean>(r.collapsed, (x) => typeof x === 'boolean'),
  };
}

const KEY = 'setpiece.client.v1';

export function loadClientPrefs(): ClientPrefs {
  try {
    const raw = localStorage.getItem(KEY);
    return sanitizePrefs(raw ? JSON.parse(raw) : null);
  } catch {
    return defaultPrefs();
  }
}

export function saveClientPrefs(p: ClientPrefs): void {
  try {
    localStorage.setItem(KEY, JSON.stringify(p));
  } catch {
    /* storage unavailable: prefs just won't persist */
  }
}
