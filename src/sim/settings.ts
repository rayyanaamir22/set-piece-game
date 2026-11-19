/**
 * User-tunable rules of the free kick. Lives in the sim state so the native
 * build can load the same values. Fields marked "reserved" are exposed in the
 * settings UI but not simulated yet (see plan.md, remaining Phase A steps).
 */
export type Frequency = 'off' | 'sometimes' | 'always';
export type KeeperType = 'balanced' | 'quick' | 'tall' | 'strong';
export const FREQUENCIES: Frequency[] = ['off', 'sometimes', 'always'];
export const KEEPER_TYPES: KeeperType[] = ['balanced', 'quick', 'tall', 'strong'];

export interface SimSettings {
  wallEnabled: boolean;
  /** Height of the wall players in metres. */
  wallHeight: number;
  /** Number of wall players; 0 = automatic from distance and angle. */
  wallCount: number;
  /** Wall players jump as the ball is struck. */
  wallJumps: boolean;
  /** A player lies down behind the wall to block low shots. */
  lyingMan: Frequency;
  /** A wall player creeps forward before the kick (a retake unless the shot scores). */
  encroachment: Frequency;
  /** A goalkeeper defends the goal. */
  keeperPresent: boolean;
  /** 0..1: reaction time, prediction quality, dive speed and hold strength. */
  keeperSkill: number;
  /** Parried balls that stay in the box become a follow-up shot. */
  rebounds: boolean;
  /** Keeper preset: reaction, reach, dive speed and hold strength. */
  keeperType: KeeperType;
  /** Wind blows across the pitch. */
  windEnabled: boolean;
  /** Wind speed in m/s. */
  windSpeed: number;
  /** Direction the wind blows towards, degrees (0 = towards goal, 90 = towards +x). */
  windDirection: number;
}

export const WALL_HEIGHT_MIN = 1.6;
export const WALL_HEIGHT_MAX = 2.1;
export const WALL_COUNT_MAX = 6;
export const WIND_SPEED_MAX = 15;

export const defaultSettings = (): SimSettings => ({
  wallEnabled: true,
  wallHeight: 1.85,
  wallCount: 0,
  wallJumps: true,
  lyingMan: 'sometimes',
  encroachment: 'sometimes',
  rebounds: true,
  keeperType: 'balanced',
  keeperPresent: true,
  keeperSkill: 0.7,
  windEnabled: false,
  windSpeed: 4,
  windDirection: 90,
});

/** Clamp a settings object into its legal ranges (used for values loaded from storage). */
export function sanitizeSettings(raw: Partial<SimSettings>): SimSettings {
  const d = defaultSettings();
  const num = (v: unknown, lo: number, hi: number, fallback: number) =>
    typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : fallback;
  const bool = (v: unknown, fallback: boolean) => (typeof v === 'boolean' ? v : fallback);
  return {
    wallEnabled: bool(raw.wallEnabled, d.wallEnabled),
    wallHeight: num(raw.wallHeight, WALL_HEIGHT_MIN, WALL_HEIGHT_MAX, d.wallHeight),
    wallCount: Math.round(num(raw.wallCount, 0, WALL_COUNT_MAX, d.wallCount)),
    wallJumps: bool(raw.wallJumps, d.wallJumps),
    lyingMan: FREQUENCIES.includes(raw.lyingMan as Frequency) ? (raw.lyingMan as Frequency) : d.lyingMan,
    encroachment: FREQUENCIES.includes(raw.encroachment as Frequency) ? (raw.encroachment as Frequency) : d.encroachment,
    rebounds: bool(raw.rebounds, d.rebounds),
    keeperType: KEEPER_TYPES.includes(raw.keeperType as KeeperType) ? (raw.keeperType as KeeperType) : d.keeperType,
    keeperPresent: bool(raw.keeperPresent, d.keeperPresent),
    keeperSkill: num(raw.keeperSkill, 0, 1, d.keeperSkill),
    windEnabled: bool(raw.windEnabled, d.windEnabled),
    windSpeed: num(raw.windSpeed, 0, WIND_SPEED_MAX, d.windSpeed),
    windDirection: num(raw.windDirection, 0, 360, d.windDirection),
  };
}

/** The wind as a vector (m/s): direction 0 = towards goal (-z), 90 = towards +x. Zero when switched off. */
export function windVector(s: SimSettings): { x: number; y: number; z: number } {
  if (!s.windEnabled || s.windSpeed <= 0) return { x: 0, y: 0, z: 0 };
  const a = (s.windDirection * Math.PI) / 180;
  return { x: Math.sin(a) * s.windSpeed, y: 0, z: -Math.cos(a) * s.windSpeed };
}
