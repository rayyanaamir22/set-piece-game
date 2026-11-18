// Small deterministic PRNG (mulberry32) whose state is a plain number, so it
// can be stored in the sim state, snapshotted for replays and ported to C.
export interface RngState {
  s: number;
}

export function rngNext(r: RngState): number {
  r.s = (r.s + 0x6d2b79f5) | 0;
  let t = r.s;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

export const rngRange = (r: RngState, lo: number, hi: number): number => lo + (hi - lo) * rngNext(r);
