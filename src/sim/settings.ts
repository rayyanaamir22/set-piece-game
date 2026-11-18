/**
 * User-tunable rules of the free kick. Lives in the sim state so the native
 * build can load the same values. Fields marked "reserved" are exposed in the
 * settings UI but not simulated yet (see plan.md, Phase A next steps).
 */
export interface SimSettings {
  wallEnabled: boolean;
  /** Height of the wall players in metres. */
  wallHeight: number;
  /** Number of wall players; 0 = automatic from distance and angle. */
  wallCount: number;
  /** reserved: wall players jump as the ball is struck. */
  wallJumps: boolean;
  /** reserved: a goalkeeper defends the goal. */
  keeperPresent: boolean;
  /** reserved: wind blows across the pitch. */
  windEnabled: boolean;
  /** reserved: wind speed in m/s. */
  windSpeed: number;
  /** reserved: direction the wind blows towards, degrees (0 = towards goal, 90 = towards +x). */
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
  keeperPresent: true,
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
    keeperPresent: bool(raw.keeperPresent, d.keeperPresent),
    windEnabled: bool(raw.windEnabled, d.windEnabled),
    windSpeed: num(raw.windSpeed, 0, WIND_SPEED_MAX, d.windSpeed),
    windDirection: num(raw.windDirection, 0, 360, d.windDirection),
  };
}
