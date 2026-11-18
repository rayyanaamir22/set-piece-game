import * as THREE from 'three';
import {
  CENTRE_ARC_RADIUS,
  PENALTY_BOX_HALF,
  PENALTY_BOX_Z,
  PENALTY_SPOT_Z,
  PITCH_HALF_WIDTH,
  SIX_YARD_HALF,
  SIX_YARD_Z,
} from '../sim/constants';

const LINE_W = 0.12;
const LINE_Y = 0.012;

function grassTexture(): THREE.CanvasTexture {
  const c = document.createElement('canvas');
  c.width = 64;
  c.height = 128;
  const g = c.getContext('2d')!;
  g.fillStyle = '#2f7d32';
  g.fillRect(0, 0, 64, 64);
  g.fillStyle = '#378a3a';
  g.fillRect(0, 64, 64, 64);
  // fine mowing noise
  for (let i = 0; i < 900; i++) {
    g.fillStyle = Math.random() < 0.5 ? 'rgba(255,255,255,0.025)' : 'rgba(0,0,0,0.03)';
    g.fillRect(Math.random() * 64, Math.random() * 128, 1, 2);
  }
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 8;
  return t;
}

export function createPitch(): THREE.Group {
  const group = new THREE.Group();

  // Grass, mown in 5.5 m bands parallel to the goal line (11 m per texture tile).
  const z0 = -8;
  const z1 = 58;
  const width = 96;
  const tex = grassTexture();
  tex.repeat.set(1, (z1 - z0) / 11);
  const grass = new THREE.Mesh(
    new THREE.PlaneGeometry(width, z1 - z0),
    new THREE.MeshLambertMaterial({ map: tex }),
  );
  grass.rotation.x = -Math.PI / 2;
  grass.position.set(0, 0, (z0 + z1) / 2);
  grass.receiveShadow = true;
  group.add(grass);

  const mat = new THREE.MeshBasicMaterial({ color: 0xf4f4f0 });

  const line = (x1: number, z1_: number, x2: number, z2: number) => {
    const dx = x2 - x1;
    const dz = z2 - z1_;
    const L = Math.hypot(dx, dz);
    const m = new THREE.Mesh(new THREE.PlaneGeometry(L + LINE_W, LINE_W), mat);
    m.rotation.x = -Math.PI / 2;
    m.rotation.z = Math.atan2(-dz, dx) * -1; // plane lies in XZ after the x-rotation
    m.position.set((x1 + x2) / 2, LINE_Y, (z1_ + z2) / 2);
    group.add(m);
  };

  const H = PITCH_HALF_WIDTH;
  // Goal line and touchlines.
  line(-H, 0, H, 0);
  line(-H, 0, -H, 58);
  line(H, 0, H, 58);
  // 18-yard box.
  line(-PENALTY_BOX_HALF, 0, -PENALTY_BOX_HALF, PENALTY_BOX_Z);
  line(PENALTY_BOX_HALF, 0, PENALTY_BOX_HALF, PENALTY_BOX_Z);
  line(-PENALTY_BOX_HALF, PENALTY_BOX_Z, PENALTY_BOX_HALF, PENALTY_BOX_Z);
  // 6-yard box.
  line(-SIX_YARD_HALF, 0, -SIX_YARD_HALF, SIX_YARD_Z);
  line(SIX_YARD_HALF, 0, SIX_YARD_HALF, SIX_YARD_Z);
  line(-SIX_YARD_HALF, SIX_YARD_Z, SIX_YARD_HALF, SIX_YARD_Z);

  // Penalty spot.
  const spot = new THREE.Mesh(new THREE.CircleGeometry(0.15, 20), mat);
  spot.rotation.x = -Math.PI / 2;
  spot.position.set(0, LINE_Y, PENALTY_SPOT_Z);
  group.add(spot);

  // "D": the part of the 9.15 m circle around the spot that lies outside the box.
  const phi = Math.acos((PENALTY_BOX_Z - PENALTY_SPOT_Z) / CENTRE_ARC_RADIUS);
  const arc = new THREE.Mesh(
    new THREE.RingGeometry(CENTRE_ARC_RADIUS - LINE_W / 2, CENTRE_ARC_RADIUS + LINE_W / 2, 64, 1, -Math.PI / 2 - phi, 2 * phi),
    mat,
  );
  arc.rotation.x = -Math.PI / 2;
  arc.position.set(0, LINE_Y, PENALTY_SPOT_Z);
  group.add(arc);

  // Corner arcs (r = 1 m).
  for (const sx of [-1, 1]) {
    const corner = new THREE.Mesh(
      new THREE.RingGeometry(1 - LINE_W / 2, 1 + LINE_W / 2, 16, 1, sx > 0 ? -Math.PI : -Math.PI / 2, Math.PI / 2),
      mat,
    );
    corner.rotation.x = -Math.PI / 2;
    corner.position.set(sx * H, LINE_Y, 0);
    group.add(corner);
  }

  return group;
}
