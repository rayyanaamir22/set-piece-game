/** Metres of reticle movement per pixel of mouse movement at sensitivity 1. */
export const AIM_M_PER_PX = 0.012;
/** Contact-point units (unit disc) per pixel at sensitivity 1. */
export const CONTACT_PER_PX = 0.006;
export const SENS_MIN = 0.2;
export const SENS_MAX = 3;

export const clampSens = (s: number): number => (Number.isFinite(s) ? Math.min(SENS_MAX, Math.max(SENS_MIN, s)) : 1);

/** Mouse movement (pixels; y grows downward) -> reticle movement in metres (y grows upward). */
export function aimDeltaFromMouse(dx: number, dy: number, sens: number): { x: number; y: number } {
  const k = AIM_M_PER_PX * clampSens(sens);
  return { x: dx * k, y: -dy * k };
}

/** Move the contact-point cursor by a mouse delta, keeping it inside the unit disc. */
export function moveContactCursor(cur: { x: number; y: number }, dx: number, dy: number, sens: number): { x: number; y: number } {
  const k = CONTACT_PER_PX * clampSens(sens);
  let x = cur.x + dx * k;
  let y = cur.y - dy * k;
  const m = Math.hypot(x, y);
  if (m > 1) {
    x /= m;
    y /= m;
  }
  return { x, y };
}
