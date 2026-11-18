import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { kickerPositions } from '../sim/layout';

/** Placeholder low-poly taker: capsule body + head + two swinging legs. */
export class KickerView {
  readonly group = new THREE.Group();
  private kickLeg: THREE.Group;
  private plantLeg: THREE.Group;
  private armL: THREE.Mesh;
  private armR: THREE.Mesh;
  private body: THREE.Group;

  constructor() {
    const shirt = new THREE.MeshStandardMaterial({ color: 0xd62828, roughness: 0.7 });
    const shorts = new THREE.MeshStandardMaterial({ color: 0xf4f4f4, roughness: 0.7 });
    const skin = new THREE.MeshStandardMaterial({ color: 0xc68c63, roughness: 0.8 });
    const boot = new THREE.MeshStandardMaterial({ color: 0x111111 });

    this.body = new THREE.Group();
    this.group.add(this.body);

    const torso = new THREE.Mesh(new THREE.CapsuleGeometry(0.22, 0.45, 4, 10), shirt);
    torso.position.y = 1.25;
    torso.castShadow = true;
    this.body.add(torso);

    const head = new THREE.Mesh(new THREE.SphereGeometry(0.14, 12, 10), skin);
    head.position.y = 1.78;
    head.castShadow = true;
    this.body.add(head);

    const mkLeg = (x: number) => {
      const hip = new THREE.Group();
      hip.position.set(x, 0.95, 0);
      const leg = new THREE.Mesh(new THREE.CapsuleGeometry(0.09, 0.62, 4, 8), shorts);
      leg.position.y = -0.4;
      leg.castShadow = true;
      hip.add(leg);
      const foot = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.08, 0.28), boot);
      foot.position.set(0, -0.82, 0.07);
      hip.add(foot);
      this.body.add(hip);
      return hip;
    };
    // Local forward = +z. "Right" leg is at -x for a body facing +z.
    this.plantLeg = mkLeg(0.12);
    this.kickLeg = mkLeg(-0.12);

    const mkArm = (x: number) => {
      const a = new THREE.Mesh(new THREE.CapsuleGeometry(0.06, 0.4, 4, 8), shirt);
      a.position.set(x, 1.3, 0);
      this.body.add(a);
      return a;
    };
    this.armL = mkArm(0.3);
    this.armR = mkArm(-0.3);
  }

  update(s: SimState): void {
    const { runupStart, strike } = kickerPositions(s.spot, s.foot);
    // A right-footed taker kicks with the leg that ends up nearest the ball.
    const kickingRight = s.foot === 'R';
    const kick = kickingRight ? this.kickLeg : this.plantLeg;
    const plant = kickingRight ? this.plantLeg : this.kickLeg;

    // t: <0 waiting, 0 at start of run-up, 1 at contact, >1 follow-through.
    const t =
      s.phase === 'aim' || s.phase === 'charge'
        ? -1
        : s.phase === 'runup'
          ? Math.min(1, s.runupT / s.cfg.shot.runupTime)
          : 1 + Math.min(1, s.phaseT / 0.5);

    const pos = new THREE.Vector3();
    if (t < 0) {
      // Waiting at the run-up start (a step or two behind the strike point).
      pos.set(runupStart.x, 0, runupStart.z);
      // Idle pose.
      kick.rotation.x = 0;
      plant.rotation.x = 0;
      this.armL.rotation.x = this.armR.rotation.x = 0;
    } else if (t <= 1) {
      const e = t * t * (3 - 2 * t);
      pos.set(
        runupStart.x + (strike.x - runupStart.x) * e,
        0,
        runupStart.z + (strike.z - runupStart.z) * e,
      );
      const stride = Math.sin(t * Math.PI * 3.2);
      plant.rotation.x = stride * 0.7 * (1 - t);
      // Wind the kicking leg back, then swing it through at contact.
      kick.rotation.x =
        t < 0.75 ? -stride * 0.7 * (1 - t) : THREE.MathUtils.lerp(0.9, -1.15, (t - 0.75) / 0.25);
      this.armL.rotation.x = -stride * 0.5;
      this.armR.rotation.x = stride * 0.5;
    } else {
      pos.set(strike.x, 0, strike.z);
      const settle = t - 1;
      kick.rotation.x = THREE.MathUtils.lerp(-1.15, 0, settle);
      plant.rotation.x = 0;
    }
    this.group.position.copy(pos);

    // Face the ball (and therefore the goal) once close, otherwise face down the run-up.
    const look = new THREE.Vector3(s.spot.x, 0, s.spot.z);
    const dx = look.x - pos.x;
    const dz = look.z - pos.z;
    let yaw = Math.atan2(dx, dz);
    if (Math.hypot(dx, dz) < 0.3) {
      yaw = Math.atan2(-s.spot.x, -s.spot.z);
    }
    this.group.rotation.y = yaw;
  }
}
