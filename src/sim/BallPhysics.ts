import { V3, add, copy, cross, len, mul, norm, smoothstep, v3 } from './math/vec3';

export interface PhysicsParams {
  mass: number;
  radius: number;
  gravity: number;
  airDensity: number;
  cdHigh: number;
  cdLow: number;
  crisisStart: number;
  crisisEnd: number;
  clGain: number;
  clMax: number;
  spinDecay: number;
  restitution: number;
  friction: number;
  inertiaFactor: number;
  rollingResistance: number;
  knuckleAccel: number;
  knuckleSpeedMin: number;
  knuckleSpeedFull: number;
  postRestitution: number;
  netRestitution: number;
  netDrag: number;
}

export interface Knuckle {
  /** Peak lateral acceleration (m/s^2). 0 disables the wobble. */
  amp: number;
  /** Six random phases (two noise channels x three sines). */
  phase: number[];
}

export interface BallState {
  pos: V3;
  vel: V3;
  /** Angular velocity (rad/s), world frame. */
  spin: V3;
  t: number;
  knuckle: Knuckle;
  /** Air velocity (m/s). Drag and Magnus act on the ball's velocity *relative to the air*. */
  wind: V3;
}

export function newBall(pos: V3): BallState {
  return {
    pos: copy(pos),
    vel: v3(),
    spin: v3(),
    t: 0,
    knuckle: { amp: 0, phase: [0, 0, 0, 0, 0, 0] },
    wind: v3(),
  };
}

export function dragCoefficient(speed: number, P: PhysicsParams): number {
  // Drag crisis: Cd falls from cdHigh to cdLow as the boundary layer turns turbulent.
  return P.cdHigh + (P.cdLow - P.cdHigh) * smoothstep(P.crisisStart, P.crisisEnd, speed);
}

const TAU = Math.PI * 2;
const KNUCKLE_FREQ = [1.3, 2.9, 4.7];

function knuckleNoise(t: number, ph: number[], offset: number): number {
  let n = 0;
  n += Math.sin(TAU * KNUCKLE_FREQ[0] * t + ph[offset]);
  n += 0.7 * Math.sin(TAU * KNUCKLE_FREQ[1] * t + ph[offset + 1]);
  n += 0.4 * Math.sin(TAU * KNUCKLE_FREQ[2] * t + ph[offset + 2]);
  return n / 2.1;
}

/** Acceleration of the ball given velocity and spin (gravity + drag + Magnus + knuckle). */
export function acceleration(vel: V3, spin: V3, t: number, k: Knuckle, P: PhysicsParams, wind?: V3): V3 {
  let ax = 0;
  let ay = -P.gravity;
  let az = 0;
  // Velocity relative to the air: with no wind this is just the ball velocity.
  const rel = wind ? { x: vel.x - wind.x, y: vel.y - wind.y, z: vel.z - wind.z } : vel;
  const speed = len(rel);
  if (speed > 1e-6) {
    const area = Math.PI * P.radius * P.radius;
    const kd = (0.5 * P.airDensity * dragCoefficient(speed, P) * area * speed) / P.mass;
    ax -= kd * rel.x;
    ay -= kd * rel.y;
    az -= kd * rel.z;

    const w = cross(spin, rel);
    const wl = len(w);
    if (wl > 1e-9) {
      const spinPerp = wl / speed;
      const S = (P.radius * spinPerp) / speed;
      const cl = Math.min(P.clMax, P.clGain * S);
      const mag = (0.5 * P.airDensity * area * cl * speed * speed) / P.mass;
      ax += (w.x / wl) * mag;
      ay += (w.y / wl) * mag;
      az += (w.z / wl) * mag;
    }

    if (k.amp > 0) {
      const fade = smoothstep(P.knuckleSpeedMin, P.knuckleSpeedFull, speed);
      const vhat = mul(rel, 1 / speed);
      let b1 = norm(cross(vhat, v3(0, 1, 0)));
      if (len(b1) < 0.5) b1 = v3(1, 0, 0);
      const b2 = cross(b1, vhat);
      const na = knuckleNoise(t, k.phase, 0) * k.amp * fade;
      const nb = knuckleNoise(t, k.phase, 3) * k.amp * fade;
      ax += b1.x * na + b2.x * nb;
      ay += b1.y * na + b2.y * nb;
      az += b1.z * na + b2.z * nb;
    }
  }
  return { x: ax, y: ay, z: az };
}

/** Advance position/velocity by dt with RK4 (spin is constant over the step, then decays). */
export function stepBall(b: BallState, P: PhysicsParams, dt: number): void {
  const h = dt / 2;
  const v0 = b.vel;
  const a1 = acceleration(v0, b.spin, b.t, b.knuckle, P, b.wind);
  const v1 = add(v0, mul(a1, h));
  const a2 = acceleration(v1, b.spin, b.t + h, b.knuckle, P, b.wind);
  const v2 = add(v0, mul(a2, h));
  const a3 = acceleration(v2, b.spin, b.t + h, b.knuckle, P, b.wind);
  const v3_ = add(v0, mul(a3, dt));
  const a4 = acceleration(v3_, b.spin, b.t + dt, b.knuckle, P, b.wind);

  const dp = mul(add(add(v0, v3_), mul(add(v1, v2), 2)), dt / 6);
  const dv = mul(add(add(a1, a4), mul(add(a2, a3), 2)), dt / 6);
  b.pos = add(b.pos, dp);
  b.vel = add(b.vel, dv);
  b.spin = mul(b.spin, Math.exp(-P.spinDecay * dt));
  b.t += dt;
}

/**
 * Ground contact: bounce with restitution plus Coulomb friction that couples
 * linear and angular velocity (so backspin checks up, topspin kicks on).
 * Returns the impact speed of a bounce, or 0 when rolling / airborne.
 */
export function groundContact(b: BallState, P: PhysicsParams, dt: number): number {
  const r = P.radius;
  if (b.pos.y >= r) return 0;
  b.pos.y = r;

  let impact = 0;
  let jn: number; // normal impulse per unit mass
  if (b.vel.y < -0.5) {
    impact = -b.vel.y;
    jn = -(1 + P.restitution) * b.vel.y;
    b.vel.y = -P.restitution * b.vel.y;
  } else {
    b.vel.y = Math.max(0, b.vel.y);
    jn = P.gravity * dt;
  }

  const rc = v3(0, -r, 0);
  const vc = add(b.vel, cross(b.spin, rc));
  const vct = v3(vc.x, 0, vc.z);
  const st = len(vct);
  if (st > 1e-6) {
    const k = P.inertiaFactor;
    const stick = st / (1 + 1 / k);
    const jt = Math.min(stick, P.friction * jn);
    const dir = mul(vct, -1 / st);
    b.vel = add(b.vel, mul(dir, jt));
    b.spin = add(b.spin, mul(cross(rc, dir), jt / (k * r * r)));
  }
  if (impact === 0) {
    const f = Math.exp(-P.rollingResistance * dt);
    b.vel.x *= f;
    b.vel.z *= f;
  }
  return impact;
}
