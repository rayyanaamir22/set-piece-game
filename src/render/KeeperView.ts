import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { buildFigure } from './Figures';

const UP = new THREE.Vector3(0, 1, 0);
const STAND_LEN = 1.9;

/** Goalkeeper figure: stretched along the sim's body capsule (feet `a` -> hands `b`). */
export class KeeperView {
  readonly group: THREE.Group;
  private armA: THREE.Mesh;
  private armB: THREE.Mesh;
  private dir = new THREE.Vector3();
  private roll = new THREE.Quaternion();

  constructor() {
    const f = buildFigure({ shirt: 0xf2c200, shorts: 0x1c1c1c, skin: 0xe8b894 });
    this.group = f.group;
    this.armA = f.armA;
    this.armB = f.armB;
    // Gloves on the fingertip end of each arm.
    const glove = new THREE.MeshStandardMaterial({ color: 0xff5a1f, roughness: 0.6 });
    for (const arm of [f.armA, f.armB]) {
      const g = new THREE.Mesh(new THREE.SphereGeometry(0.095, 10, 8), glove);
      g.position.set(0, 0.28, 0);
      arm.add(g);
    }
    this.group.visible = false;
  }

  update(s: SimState): void {
    const k = s.keeper;
    this.group.visible = !!k;
    if (!k) return;

    this.dir.set(k.b.x - k.a.x, k.b.y - k.a.y, 0);
    const len = this.dir.length();
    if (len > 1e-3) this.dir.multiplyScalar(1 / len);
    else this.dir.copy(UP);
    this.group.position.set(k.a.x, k.a.y, k.a.z);
    this.group.quaternion.setFromUnitVectors(UP, this.dir);
    // Roll about his own long axis a little as he stretches out sideways.
    const dive = k.phase === 'diving' && k.kind === 'dive';
    const u = dive ? THREE.MathUtils.clamp((s.flightT - k.t0) / k.dur, 0, 1) : 0;
    const side = Math.sign(k.endB.x - k.startB.x) || 1;
    this.roll.setFromAxisAngle(UP, side * 0.7 * u);
    this.group.quaternion.multiply(this.roll);
    // Crouch when the hitbox is shorter than a standing keeper (low step / sunk after a dive).
    this.group.scale.set(1, THREE.MathUtils.clamp(len / STAND_LEN, 0.45, 1.2), 1);

    const stretched = k.phase === 'diving' && k.kind === 'dive';
    const sx = this.armA.position.x >= 0 ? 1 : -1;
    for (const [arm, side] of [[this.armA, sx], [this.armB, -sx]] as const) {
      if (stretched) {
        // Arms straight above the head along the dive line.
        arm.position.set(side * 0.16, 2.02, 0);
        arm.rotation.set(0, 0, 0);
      } else {
        // Ready stance: arms out and slightly bent, like a keeper set for a shot.
        arm.position.set(side * 0.46, 1.4, 0.05);
        arm.rotation.set(0, 0, -side * 1.0);
      }
    }
  }
}
