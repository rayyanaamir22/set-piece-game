import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { dirToGoal, rightOf } from '../sim/layout';

/** Third-person set-piece camera behind the ball; chases the ball after the strike. */
export class CameraRig {
  readonly camera = new THREE.PerspectiveCamera(42, 1, 0.1, 400);
  far = false;
  /** Multiplier on how far back / high the camera sits (client preference). */
  distanceMul = 1;
  /** Replay cameras take over completely when set. */
  override: { pos: THREE.Vector3; look: THREE.Vector3 } | null = null;
  private overrideBlend = 0;
  private lookAt = new THREE.Vector3();
  private pos = new THREE.Vector3();
  private lastSpotId = -1;

  /** Take the camera to a director's shot (replays), or give it back with null. */
  setShot(shot: { pos: { x: number; y: number; z: number }; look: { x: number; y: number; z: number } } | null): void {
    if (!shot) {
      this.override = null;
      return;
    }
    if (!this.override) this.override = { pos: new THREE.Vector3(), look: new THREE.Vector3() };
    this.override.pos.set(shot.pos.x, shot.pos.y, shot.pos.z);
    this.override.look.set(shot.look.x, shot.look.y, shot.look.z);
  }

  toggle(): void {
    this.far = !this.far;
  }

  update(s: SimState, ballPos: THREE.Vector3, dt: number): void {
    const d = dirToGoal(s.spot);
    const r = rightOf(d);
    const back = (this.far ? 11 : 7) * this.distanceMul;
    const height = (this.far ? 4.2 : 2.5) * (0.6 + 0.4 * this.distanceMul);
    // Shift away from the taker's side so he stays in frame without blocking the ball.
    const side = (s.foot === 'R' ? 1 : -1) * (this.far ? 1.6 : 1.0);

    const home = new THREE.Vector3(
      s.spot.x - d.x * back + r.x * side,
      height,
      s.spot.z - d.z * back + r.z * side,
    );

    const aimMix = 0.35;
    const baseLook = new THREE.Vector3(s.spot.x + d.x * 12, 1.2, s.spot.z + d.z * 12);
    const reticle = new THREE.Vector3(s.aim.x, s.aim.y, 0);
    const aimLook = baseLook.clone().lerp(reticle, aimMix);

    let wantLook: THREE.Vector3;
    if (s.phase === 'flight' || s.phase === 'result') {
      wantLook = ballPos.clone();
      // Don't stare at the ground: keep the target at least at head height of the net.
      wantLook.y = Math.max(wantLook.y, 0.8);
    } else {
      wantLook = aimLook;
    }

    if (this.lastSpotId !== s.spotId) {
      this.pos.copy(home);
      this.lookAt.copy(wantLook);
      this.lastSpotId = s.spotId;
    }

    const kPos = 1 - Math.exp(-5 * dt);
    const kLook = 1 - Math.exp(-(s.phase === 'flight' ? 9 : 6) * dt);
    this.pos.lerp(home, kPos);
    this.lookAt.lerp(wantLook, kLook);

    if (this.override) {
      // Replay camera: glide to the director's shot (snap on the first frame of a replay).
      const k = this.overrideBlend === 0 ? 1 : 1 - Math.exp(-6 * dt);
      this.overrideBlend = 1;
      this.camera.position.lerp(this.override.pos, k);
      this.pos.copy(this.camera.position);
      this.lookAt.lerp(this.override.look, 1 - Math.exp(-9 * dt));
      this.camera.lookAt(this.lookAt);
      return;
    }
    this.overrideBlend = 0;
    this.camera.position.copy(this.pos);
    this.camera.lookAt(this.lookAt);
  }

  resize(w: number, h: number): void {
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }
}
