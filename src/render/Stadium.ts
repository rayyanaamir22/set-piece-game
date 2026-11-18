import * as THREE from 'three';

function crowdTexture(): THREE.CanvasTexture {
  const c = document.createElement('canvas');
  c.width = 512;
  c.height = 128;
  const g = c.getContext('2d')!;
  g.fillStyle = '#1b1d28';
  g.fillRect(0, 0, 512, 128);
  const shirts = ['#d94b4b', '#e8e8e8', '#3b6fd1', '#f0c040', '#2a2a2a', '#58a95a', '#b05bd1'];
  const skin = ['#e8b894', '#c68c63', '#8d5a3b', '#f2d0b5'];
  let seed = 7;
  const rnd = () => {
    seed = (seed * 16807) % 2147483647;
    return seed / 2147483647;
  };
  for (let y = 4; y < 128; y += 8) {
    for (let x = (y / 8) % 2 ? 0 : 4; x < 512; x += 8) {
      g.fillStyle = shirts[Math.floor(rnd() * shirts.length)];
      g.fillRect(x, y + 3, 6, 5);
      g.fillStyle = skin[Math.floor(rnd() * skin.length)];
      g.fillRect(x + 1, y, 4, 3);
    }
  }
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.magFilter = THREE.NearestFilter;
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

export function createStadium(): THREE.Group {
  const group = new THREE.Group();
  const crowd = crowdTexture();
  const V = (x: number, y: number, z: number) => new THREE.Vector3(x, y, z);

  // Main stand behind the goal, rising away from the pitch.
  group.add(quad(V(-45, 0.9, -9), V(45, 0.9, -9), V(45, 20, -34), V(-45, 20, -34), crowd, 90 / 14, 6));

  // Side stands along both touchlines.
  for (const sx of [-1, 1]) {
    const x0 = sx * 38;
    const x1 = sx * 62;
    group.add(quad(V(x0, 0.9, -9), V(x0, 0.9, 61), V(x1, 18, 61), V(x1, 18, -9), crowd, 70 / 14, 6, 0xb0b0b0));
  }

  // Dark concourse wall under the stand.
  const wall = new THREE.Mesh(
    new THREE.PlaneGeometry(90, 3),
    new THREE.MeshBasicMaterial({ color: 0x15171f }),
  );
  wall.position.set(0, 1.5, -8.8);
  group.add(wall);

  // Advertising boards.
  const ads = adTexture();
  const ad = new THREE.Mesh(
    new THREE.PlaneGeometry(60, 1.0),
    new THREE.MeshBasicMaterial({ map: ads }),
  );
  ad.position.set(0, 0.5, -7);
  group.add(ad);
  for (const sx of [-1, 1]) {
    const side = new THREE.Mesh(new THREE.PlaneGeometry(40, 1.0), new THREE.MeshBasicMaterial({ map: ads }));
    side.rotation.y = -sx * (Math.PI / 2);
    side.position.set(sx * 38, 0.5, 20);
    group.add(side);
  }

  return group;
}
