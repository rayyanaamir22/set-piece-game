import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { dirToGoal, rightOf } from '../sim/layout';

/** Third-person set-piece camera behind the ball; chases the ball after the strike. */
export class CameraRig {
  readonly camera = new THREE.PerspectiveCamera(42, 1, 0.1, 400);
  far = false;
  private lookAt = new THREE.Vector3();
  private pos = new THREE.Vector3();
  private lastSpotId = -1;

  toggle(): void {
    this.far = !this.far;
  }

  update(s: SimState, ballPos: THREE.Vector3, dt: number): void {
    const d = dirToGoal(s.spot);
    const r = rightOf(d);
    const back = this.far ? 11 : 7;
    const height = this.far ? 4.2 : 2.5;
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

    this.camera.position.copy(this.pos);
    this.camera.lookAt(this.lookAt);
  }

  resize(w: number, h: number): void {
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }
}
