import * as THREE from 'three';
import { WALL_DISTANCE } from '../sim/constants';
import { dirToGoal, rightOf } from '../sim/layout';
import type { SimState } from '../sim/Sim';

/**
 * The referee's vanishing spray: a mark under the ball and the 9.15 m line the
 * wall has to stand behind. It is there while you line up and fades out after the strike.
 */
export class SprayView {
  readonly group = new THREE.Group();
  private mat = new THREE.MeshBasicMaterial({ color: 0xf4f9ff, transparent: true, opacity: 0.9, depthWrite: false });
  private spot: THREE.Mesh;
  private line: THREE.Mesh;

  constructor() {
    this.spot = new THREE.Mesh(new THREE.RingGeometry(0.18, 0.34, 28), this.mat);
    this.spot.rotation.x = -Math.PI / 2;
    this.line = new THREE.Mesh(new THREE.PlaneGeometry(3.4, 0.16), this.mat);
    this.line.rotation.x = -Math.PI / 2;
    this.group.add(this.spot, this.line);
  }

  update(s: SimState): void {
    const d = dirToGoal(s.spot);
    // Fully visible until the strike, then gone within ~3.5 s.
    const fade = s.phase === 'flight' ? Math.max(0, 1 - s.flightT / 3.5) : s.phase === 'result' ? 0 : 1;
    this.mat.opacity = 0.9 * fade;
    this.group.visible = fade > 0.01;
    this.spot.position.set(s.spot.x, 0.02, s.spot.z);

    const dist = Math.hypot(s.spot.x, s.spot.z);
    this.line.visible = dist > WALL_DISTANCE + 1.5;
    if (!this.line.visible) return;
    // Where the wall stands if there is one, otherwise square across the line to goal.
    let cx = s.spot.x + d.x * WALL_DISTANCE;
    let cz = s.spot.z + d.z * WALL_DISTANCE;
    let ax = rightOf(d).x;
    let az = rightOf(d).z;
    if (s.wall) {
      cx = s.wall.base.reduce((a, p) => a + p.x, 0) / s.wall.base.length;
      cz = s.wall.base.reduce((a, p) => a + p.z, 0) / s.wall.base.length;
      ax = s.wall.axis.x;
      az = s.wall.axis.z;
      // the line sits on the ball side of the players' feet
      cx += s.wall.facing.x * 0.45;
      cz += s.wall.facing.z * 0.45;
    }
    this.line.position.set(cx, 0.02, cz);
    this.line.rotation.set(-Math.PI / 2, 0, -Math.atan2(az, ax), 'XYZ');
  }
}
