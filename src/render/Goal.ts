import * as THREE from 'three';
import { GOAL_DEPTH, GOAL_HALF_WIDTH, GOAL_HEIGHT, POST_RADIUS } from '../sim/constants';

export interface GoalView {
  group: THREE.Group;
  /** Make the net billow where the ball hit it. */
  ripple(pos: THREE.Vector3, strength: number): void;
  update(dt: number): void;
}

const STEP = 0.18;

export function createGoal(): GoalView {
  const group = new THREE.Group();

  const frameMat = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 0.4 });
  const postGeo = new THREE.CylinderGeometry(POST_RADIUS, POST_RADIUS, GOAL_HEIGHT, 16);
  for (const sx of [-1, 1]) {
    const post = new THREE.Mesh(postGeo, frameMat);
    post.position.set(sx * GOAL_HALF_WIDTH, GOAL_HEIGHT / 2, 0);
    post.castShadow = true;
    group.add(post);
  }
  const bar = new THREE.Mesh(
    new THREE.CylinderGeometry(POST_RADIUS, POST_RADIUS, GOAL_HALF_WIDTH * 2 + POST_RADIUS * 2, 16),
    frameMat,
  );
  bar.rotation.z = Math.PI / 2;
  bar.position.set(0, GOAL_HEIGHT, 0);
  bar.castShadow = true;
  group.add(bar);

  // Rear stanchions.
  const backMat = new THREE.MeshStandardMaterial({ color: 0xdddddd });
  for (const sx of [-1, 1]) {
    const s = new THREE.Mesh(new THREE.CylinderGeometry(0.03, 0.03, GOAL_DEPTH, 8), backMat);
    s.rotation.x = Math.PI / 2;
    s.position.set(sx * GOAL_HALF_WIDTH, GOAL_HEIGHT, -GOAL_DEPTH / 2);
    group.add(s);
  }

  // Net: grid lines on back, top and the two sides, stored as a flat segment list
  // so the vertices can be displaced around an impact.
  const pts: number[] = [];
  const seg = (a: number[], b: number[]) => pts.push(...a, ...b);
  const nx = Math.round((GOAL_HALF_WIDTH * 2) / STEP);
  const ny = Math.round(GOAL_HEIGHT / STEP);
  const nz = Math.round(GOAL_DEPTH / STEP);
  const W = GOAL_HALF_WIDTH;
  const H = GOAL_HEIGHT;
  const D = GOAL_DEPTH;
  // back
  for (let i = 0; i <= nx; i++) seg([-W + (i * 2 * W) / nx, 0, -D], [-W + (i * 2 * W) / nx, H, -D]);
  for (let j = 0; j <= ny; j++) seg([-W, (j * H) / ny, -D], [W, (j * H) / ny, -D]);
  // top
  for (let i = 0; i <= nx; i++) seg([-W + (i * 2 * W) / nx, H, 0], [-W + (i * 2 * W) / nx, H, -D]);
  for (let k = 0; k <= nz; k++) seg([-W, H, -(k * D) / nz], [W, H, -(k * D) / nz]);
  // sides
  for (const sx of [-1, 1]) {
    for (let j = 0; j <= ny; j++) seg([sx * W, (j * H) / ny, 0], [sx * W, (j * H) / ny, -D]);
    for (let k = 0; k <= nz; k++) seg([sx * W, 0, -(k * D) / nz], [sx * W, H, -(k * D) / nz]);
  }
  const rest = new Float32Array(pts);
  const geo = new THREE.BufferGeometry();
  const live = new Float32Array(rest);
  geo.setAttribute('position', new THREE.BufferAttribute(live, 3));
  const net = new THREE.LineSegments(
    geo,
    new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.55 }),
  );
  net.frustumCulled = false;
  group.add(net);

  // Ripple state.
  let impact: THREE.Vector3 | null = null;
  let dir = new THREE.Vector3();
  let amp = 0;
  let age = 0;

  const view: GoalView = {
    group,
    ripple(pos, strength) {
      impact = pos.clone();
      dir = new THREE.Vector3(0, 0, -1);
      amp = Math.min(0.9, strength * 0.04);
      age = 0;
    },
    update(dt) {
      if (!impact || amp < 0.002) return;
      age += dt;
      const decay = Math.exp(-2.6 * age);
      for (let i = 0; i < rest.length; i += 3) {
        const dx = rest[i] - impact.x;
        const dy = rest[i + 1] - impact.y;
        const dz = rest[i + 2] - impact.z;
        const d = Math.hypot(dx, dy, dz);
        const w = Math.exp(-d * d * 0.6) * Math.cos(age * 22 - d * 4) * decay * amp;
        live[i] = rest[i] + dir.x * w;
        live[i + 1] = rest[i + 1] + dir.y * w;
        live[i + 2] = rest[i + 2] + dir.z * w;
      }
      geo.attributes.position.needsUpdate = true;
      if (decay < 0.01) {
        live.set(rest);
        geo.attributes.position.needsUpdate = true;
        impact = null;
      }
    },
  };
  return view;
}
