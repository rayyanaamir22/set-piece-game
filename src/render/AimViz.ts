import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { kickerPositions } from '../sim/layout';

/** Reticle on the goal plane, arrow pointing out of the ball, optional predicted path. */
export class AimViz {
  readonly group = new THREE.Group();
  private reticle: THREE.Group;
  private arrow: THREE.Mesh;
  private pathLine: THREE.Line;
  private ringMat: THREE.MeshBasicMaterial;
  private runupLine: THREE.Line;
  private runupRing: THREE.Mesh;

  constructor() {
    this.ringMat = new THREE.MeshBasicMaterial({
      color: 0xffd23f,
      depthTest: false,
      transparent: true,
      opacity: 0.95,
    });
    this.reticle = new THREE.Group();
    const ring = new THREE.Mesh(new THREE.RingGeometry(0.26, 0.34, 40), this.ringMat);
    const dot = new THREE.Mesh(new THREE.CircleGeometry(0.06, 16), this.ringMat);
    const tick = new THREE.PlaneGeometry(0.1, 0.2);
    for (let i = 0; i < 4; i++) {
      const t = new THREE.Mesh(tick, this.ringMat);
      t.position.set(Math.cos((i * Math.PI) / 2) * 0.46, Math.sin((i * Math.PI) / 2) * 0.46, 0);
      t.rotation.z = (i * Math.PI) / 2 + Math.PI / 2;
      this.reticle.add(t);
    }
    for (const m of [ring, dot]) {
      m.renderOrder = 10;
      this.reticle.add(m);
    }
    this.reticle.children.forEach((c) => (c.renderOrder = 10));
    this.group.add(this.reticle);

    // Flat arrow along local -z (before rotation), tip at z = -L.
    const L = 5;
    const shape = new THREE.Shape();
    shape.moveTo(-0.12, 0);
    shape.lineTo(0.12, 0);
    shape.lineTo(0.12, L - 0.9);
    shape.lineTo(0.4, L - 0.9);
    shape.lineTo(0, L);
    shape.lineTo(-0.4, L - 0.9);
    shape.lineTo(-0.12, L - 0.9);
    shape.closePath();
    const geo = new THREE.ShapeGeometry(shape);
    geo.rotateX(-Math.PI / 2); // shape +y -> world -z
    this.arrow = new THREE.Mesh(
      geo,
      new THREE.MeshBasicMaterial({ color: 0xffd23f, transparent: true, opacity: 0.8, depthWrite: false }),
    );
    this.arrow.renderOrder = 5;
    this.group.add(this.arrow);

    // Run-up marker: dashed line from where the taker starts to the ball, with a ring at the start.
    this.runupLine = new THREE.Line(
      new THREE.BufferGeometry(),
      new THREE.LineDashedMaterial({ color: 0xffffff, dashSize: 0.3, gapSize: 0.22, transparent: true, opacity: 0.8 }),
    );
    this.runupLine.frustumCulled = false;
    this.group.add(this.runupLine);
    this.runupRing = new THREE.Mesh(
      new THREE.RingGeometry(0.25, 0.34, 28),
      new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.85, depthWrite: false, side: THREE.DoubleSide }),
    );
    this.runupRing.rotation.x = -Math.PI / 2;
    this.group.add(this.runupRing);

    this.pathLine = new THREE.Line(
      new THREE.BufferGeometry(),
      new THREE.LineBasicMaterial({ color: 0x4ee1ff }),
    );
    this.pathLine.frustumCulled = false;
    this.group.add(this.pathLine);
  }

  update(s: SimState, camPos: THREE.Vector3): void {
    const aiming = s.phase === 'aim' || s.phase === 'charge';
    this.group.visible = aiming || s.phase === 'runup';
    if (!this.group.visible) return;

    // Reticle: constant apparent size.
    this.reticle.position.set(s.aim.x, s.aim.y, 0.08);
    const dist = camPos.distanceTo(this.reticle.position);
    this.reticle.scale.setScalar(Math.max(1, dist / 22));
    this.reticle.lookAt(camPos);
    this.ringMat.color.set(s.preview?.knuckle ? 0xff6b6b : 0xffd23f);

    // Run-up marker (only while lining up).
    const { runupStart, strike } = kickerPositions(s.spot, s.foot, s.runup);
    this.runupLine.visible = this.runupRing.visible = aiming;
    this.runupLine.geometry.setFromPoints([
      new THREE.Vector3(runupStart.x, 0.04, runupStart.z),
      new THREE.Vector3(strike.x, 0.04, strike.z),
    ]);
    this.runupLine.computeLineDistances();
    this.runupRing.position.set(runupStart.x, 0.045, runupStart.z);

    // Arrow pointing along the launch direction (azimuth only; it lies on the grass).
    this.arrow.visible = aiming && !!s.preview;
    if (s.preview) {
      this.arrow.position.set(s.spot.x, 0.035, s.spot.z);
      this.arrow.rotation.y = -s.preview.azimuth;
      const sc = 0.6 + 0.4 * (s.phase === 'charge' ? s.meter : 0.5);
      this.arrow.scale.set(1, 1, sc);
    }

    const path = s.preview?.path;
    this.pathLine.visible = !!path && s.showPath;
    if (path && s.showPath) {
      this.pathLine.geometry.setFromPoints(path.map((p) => new THREE.Vector3(p.x, p.y, p.z)));
    }
  }
}
