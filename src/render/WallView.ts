import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { WALL_COUNT_MAX } from '../sim/settings';
import { FIGURE_HEIGHT, buildFigure } from './Figures';

const SKINS = [0xc68c63, 0x8d5a3b, 0xf2d0b5, 0xe8b894, 0x6b4430, 0xd9a577];

/** Defensive wall: a pool of figures shown/hidden, scaled to the configured height. */
export class WallView {
  readonly group = new THREE.Group();
  private pool: THREE.Group[] = [];

  constructor() {
    for (let i = 0; i < WALL_COUNT_MAX; i++) {
      const f = buildFigure({ shirt: 0x1f4fbf, shorts: 0x14233f, skin: SKINS[i % SKINS.length] });
      // Hands cupped in front of the body.
      f.armA.rotation.set(-0.9, 0, 0.25);
      f.armA.position.set(0.2, 1.1, 0.18);
      f.armB.rotation.set(-0.9, 0, -0.25);
      f.armB.position.set(-0.2, 1.1, 0.18);
      f.group.visible = false;
      this.pool.push(f.group);
      this.group.add(f.group);
    }
  }

  update(s: SimState): void {
    const wall = s.wall;
    for (let i = 0; i < this.pool.length; i++) {
      const g = this.pool[i];
      const p = wall?.players[i];
      g.visible = !!p;
      if (!p || !wall) continue;
      g.position.set(p.x, 0, p.z);
      g.rotation.y = Math.atan2(wall.facing.x, wall.facing.z);
      g.scale.setScalar(wall.height / FIGURE_HEIGHT);
    }
  }
}
