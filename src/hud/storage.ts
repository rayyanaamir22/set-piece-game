import { SimSettings, sanitizeSettings } from '../sim/settings';

const KEY = 'setpiece.settings.v1';

export interface SavedPrefs {
  settings: Partial<SimSettings>;
  taker?: { accuracy: number; curve: number; power: number };
  foot?: 'R' | 'L';
  runup?: { angle: number; dist: number };
  showPath?: boolean;
}

export function loadPrefs(): SavedPrefs | null {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return null;
    const p = JSON.parse(raw) as SavedPrefs;
    return { ...p, settings: sanitizeSettings(p.settings ?? {}) };
  } catch {
    return null;
  }
}

export function savePrefs(p: SavedPrefs): void {
  try {
    localStorage.setItem(KEY, JSON.stringify(p));
  } catch {
    /* storage unavailable (private mode etc.) – settings just won't persist */
  }
}

export function clearPrefs(): void {
  try {
    localStorage.removeItem(KEY);
  } catch {
    /* ignore */
  }
}
