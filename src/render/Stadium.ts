import * as THREE from 'three';
import type { TimeOfDay } from '../prefs';

/** A crowd tile: rows of seats with shaded rows, varied shirts and hair, scarves, flags and raised arms. */
function crowdTexture(): THREE.CanvasTexture {
  const W = 1024;
  const H = 256;
  const c = document.createElement('canvas');
  c.width = W;
  c.height = H;
  const g = c.getContext('2d')!;
  let seed = 11;
  const rnd = () => {
    seed = (seed * 16807) % 2147483647;
    return seed / 2147483647;
  };
  const pick = <T,>(a: T[]) => a[Math.floor(rnd() * a.length)];
  const shirts = ['#d94b4b', '#e8e8e8', '#3b6fd1', '#f0c040', '#2d2d33', '#58a95a', '#b05bd1', '#e8793b', '#2aa7b8'];
  const skin = ['#f2d0b5', '#e8b894', '#c68c63', '#8d5a3b', '#5b3a29'];
  const hair = ['#1b1410', '#3b2a1c', '#6b4a2b', '#c9a15a', '#9a9a9a', '#111'];
  const ROW = 16;

  g.fillStyle = '#14161f';
  g.fillRect(0, 0, W, H);
  for (let row = 0; row < H / ROW; row++) {
    const y0 = row * ROW;
    // seat backs and a shadow under the row in front
    g.fillStyle = row % 2 ? '#262a38' : '#20232f';
    g.fillRect(0, y0, W, ROW);
    g.fillStyle = 'rgba(0,0,0,0.35)';
    g.fillRect(0, y0, W, 3);
    for (let x = row % 2 ? 0 : 7; x < W; x += 14) {
      if (rnd() < 0.06) continue; // an empty seat
      const shirt = pick(shirts);
      const sk = pick(skin);
      // body
      g.fillStyle = shirt;
      g.fillRect(x, y0 + 7, 11, 9);
      g.fillStyle = 'rgba(255,255,255,0.12)';
      g.fillRect(x, y0 + 7, 11, 2);
      // head and hair
      g.fillStyle = sk;
      g.beginPath();
      g.arc(x + 5.5, y0 + 5, 3.6, 0, Math.PI * 2);
      g.fill();
      g.fillStyle = pick(hair);
      g.fillRect(x + 2, y0 + 1.5, 7, 2.5);
      // raised arm / scarf / flag now and then
      const r = rnd();
      if (r < 0.07) {
        g.fillStyle = sk;
        g.fillRect(x + 9, y0 - 3, 2, 8);
      } else if (r < 0.12) {
        g.fillStyle = pick(['#d94b4b', '#f0c040', '#3b6fd1']);
        g.fillRect(x + 1, y0 + 8, 9, 2.5);
        g.fillStyle = '#fff';
        g.fillRect(x + 3, y0 + 8, 1.5, 2.5);
      } else if (r < 0.135) {
        g.fillStyle = '#fff';
        g.fillRect(x + 9, y0 - 6, 1.5, 11);
        g.fillStyle = pick(['#d62828', '#1f4fbf', '#f4a300']);
        g.fillRect(x + 10.5, y0 - 6, 6, 4);
      }
    }
  }
  // a few banners
  for (let i = 0; i < 3; i++) {
    const by = Math.floor(rnd() * (H / ROW - 2)) * ROW + 4;
    const bx = Math.floor(rnd() * (W - 140));
    g.fillStyle = pick(['#d62828', '#0b3d91', '#f4a300', '#1a7f37']);
    g.fillRect(bx, by, 130, 9);
    g.fillStyle = 'rgba(255,255,255,0.85)';
    g.font = 'bold 8px sans-serif';
    g.fillText(pick(['SET PIECE', 'ULTRAS', 'CURL IT', 'TOP BINS']), bx + 6, by + 7.5);
  }
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.magFilter = THREE.LinearFilter;
  t.minFilter = THREE.LinearMipmapLinearFilter;
  t.anisotropy = 4;
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

function adTexture(): THREE.CanvasTexture {
  const c = document.createElement('canvas');
  c.width = 1024;
  c.height = 64;
  const g = c.getContext('2d')!;
  const cols = ['#0b3d91', '#d62828', '#f4a300', '#111', '#1a7f37'];
  const words = ['SET PIECE', 'FREE KICK', 'CURL IT', 'TOP BINS', 'KNUCKLE', 'DIP'];
  for (let i = 0; i < 6; i++) {
    g.fillStyle = cols[i % cols.length];
    g.fillRect((i * 1024) / 6, 0, 1024 / 6, 64);
    g.fillStyle = '#fff';
    g.font = 'bold 28px sans-serif';
    g.textAlign = 'center';
    g.textBaseline = 'middle';
    g.fillText(words[i], (i * 1024) / 6 + 1024 / 12, 34);
  }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

function glowTexture(): THREE.CanvasTexture {
  const c = document.createElement('canvas');
  c.width = c.height = 128;
  const g = c.getContext('2d')!;
  const grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  grad.addColorStop(0, 'rgba(255,248,225,1)');
  grad.addColorStop(0.25, 'rgba(255,240,200,0.55)');
  grad.addColorStop(1, 'rgba(255,240,200,0)');
  g.fillStyle = grad;
  g.fillRect(0, 0, 128, 128);
  return new THREE.CanvasTexture(c);
}

/** A textured quad from four corners (bottom-left, bottom-right, top-right, top-left). */
function quad(
  a: THREE.Vector3,
  b: THREE.Vector3,
  c: THREE.Vector3,
  d: THREE.Vector3,
  tex: THREE.Texture,
  repeatU: number,
  repeatV: number,
  tint = 0xcccccc,
): THREE.Mesh {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute([...a.toArray(), ...b.toArray(), ...c.toArray(), ...d.toArray()], 3));
  geo.setAttribute('uv', new THREE.Float32BufferAttribute([0, 0, repeatU, 0, repeatU, repeatV, 0, repeatV], 2));
  geo.setIndex([0, 1, 2, 0, 2, 3]);
  return new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ map: tex, color: tint, side: THREE.DoubleSide }));
}

/** Crowd brightness per time of day (the crowd is unlit, so it is tinted instead). */
const CROWD_TINT: Record<TimeOfDay, number> = { day: 0xcccccc, dusk: 0xa89890, night: 0x6c7390 };

export interface StadiumView {
  group: THREE.Group;
  /** Floodlight spot lights (off by day). */
  lights: THREE.SpotLight[];
  setTime(t: TimeOfDay): void;
  /** The crowd jumps for a moment. */
  cheer(): void;
  update(dt: number): void;
}

export function createStadium(): StadiumView {
  const group = new THREE.Group();
  const crowd = crowdTexture();
  const V = (x: number, y: number, z: number) => new THREE.Vector3(x, y, z);
  const stands: THREE.Mesh[] = [];

  // Main stand behind the goal, rising away from the pitch.
  stands.push(quad(V(-45, 0.9, -9), V(45, 0.9, -9), V(45, 20, -34), V(-45, 20, -34), crowd, 90 / 30, 3));
  // Side stands along both touchlines.
  for (const sx of [-1, 1]) {
    const x0 = sx * 38;
    const x1 = sx * 62;
    stands.push(quad(V(x0, 0.9, -9), V(x0, 0.9, 61), V(x1, 18, 61), V(x1, 18, -9), crowd, 70 / 30, 3, 0xb0b0b0));
  }
  stands.forEach((s) => group.add(s));

  // Dark concourse wall under the stand.
  const wall = new THREE.Mesh(new THREE.PlaneGeometry(90, 3), new THREE.MeshBasicMaterial({ color: 0x15171f }));
  wall.position.set(0, 1.5, -8.8);
  group.add(wall);

  // Advertising boards (lit boards: always bright).
  const ads = adTexture();
  const ad = new THREE.Mesh(new THREE.PlaneGeometry(60, 1.0), new THREE.MeshBasicMaterial({ map: ads }));
  ad.position.set(0, 0.5, -7);
  group.add(ad);
  for (const sx of [-1, 1]) {
    const side = new THREE.Mesh(new THREE.PlaneGeometry(40, 1.0), new THREE.MeshBasicMaterial({ map: ads }));
    side.rotation.y = -sx * (Math.PI / 2);
    side.position.set(sx * 38, 0.5, 20);
    group.add(side);
  }

  // Floodlight towers behind the stands, each with a lamp bank, a glow and a spot light.
  const lights: THREE.SpotLight[] = [];
  const lamps: THREE.MeshBasicMaterial[] = [];
  const glows: THREE.SpriteMaterial[] = [];
  const glow = glowTexture();
  const pole = new THREE.MeshStandardMaterial({ color: 0x555a66, roughness: 0.7 });
  const towers: [number, number][] = [[-68, -14], [68, -14], [-68, 50], [68, 50]];
  towers.forEach(([x, z], i) => {
    const h = 34;
    const p = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.8, h, 8), pole);
    p.position.set(x, h / 2, z);
    group.add(p);
    const lampMat = new THREE.MeshBasicMaterial({ color: 0x9a9a8a });
    lamps.push(lampMat);
    const bank = new THREE.Mesh(new THREE.BoxGeometry(7, 3.4, 0.6), lampMat);
    bank.position.set(x, h + 1.5, z);
    bank.lookAt(0, h, 14);
    group.add(bank);
    const gm = new THREE.SpriteMaterial({ map: glow, transparent: true, opacity: 0, depthWrite: false, blending: THREE.AdditiveBlending });
    glows.push(gm);
    const sprite = new THREE.Sprite(gm);
    sprite.scale.set(22, 22, 1);
    sprite.position.set(x, h + 1.5, z);
    group.add(sprite);

    const light = new THREE.SpotLight(0xfff1d6, 0, 0, 0.55, 0.5, 2);
    light.position.set(x, h + 1, z);
    light.target.position.set(0, 0, 14);
    if (i === 0) {
      light.castShadow = true;
      light.shadow.mapSize.set(1024, 1024);
      light.shadow.bias = -0.0005;
    }
    group.add(light);
    group.add(light.target);
    lights.push(light);
  });

  let cheerT = 0;
  const view: StadiumView = {
    group,
    lights,
    setTime(t) {
      const on = t === 'night' ? 1 : t === 'dusk' ? 0.45 : 0;
      lights.forEach((l) => (l.intensity = on * 5200));
      lamps.forEach((m) => m.color.setHex(t === 'day' ? 0x8a8a7e : 0xfff4d8));
      glows.forEach((m) => (m.opacity = t === 'night' ? 0.95 : t === 'dusk' ? 0.4 : 0));
      stands.forEach((s, i) => {
        const c = new THREE.Color(CROWD_TINT[t]);
        if (i > 0) c.multiplyScalar(0.92);
        (s.material as THREE.MeshBasicMaterial).color.copy(c);
      });
    },
    cheer() {
      cheerT = 2.6;
    },
    update(dt) {
      if (cheerT > 0) {
        cheerT = Math.max(0, cheerT - dt);
        const k = cheerT / 2.6;
        crowd.offset.y = Math.abs(Math.sin(cheerT * 11)) * 0.012 * k;
      } else if (crowd.offset.y !== 0) crowd.offset.y = 0;
    },
  };
  view.setTime('day');
  return view;
}
