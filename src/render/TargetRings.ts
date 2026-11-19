import * as THREE from 'three';
import type { SimState } from '../sim/Sim';

const COLORS = [0xffffff, 0xffd23f, 0xff8a3d, 0xff4d6d];

function label(text: string): THREE.Sprite {
  const c = document.createElement('canvas');
  c.width = c.height = 128;
  const g = c.getContext('2d')!;
  g.font = 'bold 84px sans-serif';
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  g.lineWidth = 10;
  g.strokeStyle = 'rgba(0,0,0,0.6)';
  g.strokeText(text, 64, 70);
  g.fillStyle = '#fff';
  g.fillText(text, 64, 70);
  const m = new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(c), transparent: true, depthTest: false });
  const sp = new THREE.Sprite(m);
  sp.renderOrder = 12;
  return sp;
}

/** Challenge targets: rings on the goal plane, coloured by value; the one you hit lights up. */
export class TargetRings {
  readonly group = new THREE.Group();
  private key = '';
  private rings: { mesh: THREE.Group; mat: THREE.MeshBasicMaterial; base: number }[] = [];
  private t = 0;

  update(s: SimState, dt: number): void {
    const ch = s.challenge;
    this.group.visible = !!ch;
    if (!ch) {
      this.key = '';
      return;
    }
    const key = `${ch.index}`;
    if (key !== this.key) {
      this.key = key;
      this.group.clear();
      this.rings = [];
      const values = [...new Set(ch.def.targets.map((x) => x.points))].sort((a, b) => a - b);
      ch.def.targets.forEach((tg) => {
        const color = COLORS[Math.min(COLORS.length - 1, values.indexOf(tg.points))];
        const mat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.9, depthTest: false, side: THREE.DoubleSide });
        const g = new THREE.Group();
        const ring = new THREE.Mesh(new THREE.RingGeometry(tg.r * 0.8, tg.r, 48), mat);
        const fill = new THREE.Mesh(
          new THREE.CircleGeometry(tg.r * 0.8, 40),
          new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.12, depthTest: false, side: THREE.DoubleSide }),
        );
        ring.renderOrder = fill.renderOrder = 11;
        const sp = label(String(tg.points));
        sp.scale.setScalar(Math.min(0.9, tg.r * 1.1));
        g.add(ring, fill, sp);
        g.position.set(tg.x, tg.y, 0.15);
        this.group.add(g);
        this.rings.push({ mesh: g, mat, base: 0.9 });
      });
    }
    this.t += dt;
    const hit = s.phase === 'result' && s.result ? s.result.ring : -1;
    this.rings.forEach((r, i) => {
      const pulse = i === hit ? 1 + 0.18 * Math.sin(this.t * 14) : 1;
      r.mesh.scale.setScalar(pulse);
      r.mat.color.set(i === hit ? 0x46d369 : COLORS[Math.min(COLORS.length - 1, [...new Set(ch.def.targets.map((x) => x.points))].sort((a, b) => a - b).indexOf(ch.def.targets[i].points))]);
    });
  }
}
