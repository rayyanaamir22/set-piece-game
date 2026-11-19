import { SimSettings } from './settings';

export interface DifficultyPreset {
  label: string;
  /** Added to the taker's accuracy (shrinks or grows the timing/skill error). */
  accuracyBonus: number;
  settings: Partial<SimSettings>;
}

export type DifficultyName = 'easy' | 'normal' | 'hard' | 'pro';
export const DIFFICULTIES: DifficultyName[] = ['easy', 'normal', 'hard', 'pro'];
