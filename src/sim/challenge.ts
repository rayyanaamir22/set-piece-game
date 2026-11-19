import { SimSettings } from './settings';

export interface ChallengeTarget {
  /** Centre of the ring on the goal plane (m). */
  x: number;
  y: number;
  /** Radius (m). */
  r: number;
  points: number;
}

export interface ChallengeDef {
  id: string;
  name: string;
  desc: string;
  /** Attempts per run. */
  shots: number;
  spot: { x: number; z: number };
  /** Settings forced for the duration of the challenge. */
  settings: Partial<SimSettings>;
  targets: ChallengeTarget[];
  /** Points for any goal that misses every ring. */
  goalPoints: number;
  medals: { bronze: number; silver: number; gold: number };
}

export type Medal = 'bronze' | 'silver' | 'gold';

export interface ChallengeShot {
  type: string;
  points: number;
  /** Index of the ring that was hit, or -1. */
  ring: number;
}

export interface ChallengeRun {
  index: number;
  def: ChallengeDef;
  taken: number;
  points: number;
  log: ChallengeShot[];
  done: boolean;
  medal: Medal | null;
  /** Settings to restore when the challenge ends. */
  backup: SimSettings;
}

/** Score one attempt: only goals score; the best ring the ball crossed the line through is what counts. */
export function scoreShot(
  def: ChallengeDef,
  crossing: { x: number; y: number } | null,
  type: string,
): { points: number; ring: number } {
  if (type !== 'goal') return { points: 0, ring: -1 };
  let best = { points: def.goalPoints, ring: -1 };
  if (crossing) {
    def.targets.forEach((t, i) => {
      if (Math.hypot(crossing.x - t.x, crossing.y - t.y) <= t.r && t.points > best.points) {
        best = { points: t.points, ring: i };
      }
    });
  }
  return best;
}

export function medalFor(def: ChallengeDef, points: number): Medal | null {
  if (points >= def.medals.gold) return 'gold';
  if (points >= def.medals.silver) return 'silver';
  if (points >= def.medals.bronze) return 'bronze';
  return null;
}
