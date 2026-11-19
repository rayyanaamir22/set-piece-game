import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { AimViz } from './AimViz';
import { CameraRig } from './CameraRig';
import type { Quality, TimeOfDay } from '../prefs';
import { DecoyView, KickerView } from './Figures';
import { createGoal } from './Goal';
import { createPitch } from './Pitch';
import { KeeperView } from './KeeperView';
import { SprayView } from './Spray';
import { StadiumView, createStadium } from './Stadium';
import { TargetRings } from './TargetRings';
import { WallView } from './WallView';

function ballTexture(): THREE.CanvasTexture {
  const c = document.createElement('canvas');
  c.width = 256;
  c.height = 128;
  const g = c.getContext('2d')!;
  g.fillStyle = '#f5f5f5';
  g.fillRect(0, 0, 256, 128);
  g.fillStyle = '#1b1b1b';
  // Rough pentagon patches so spin is visible.
  let seed = 3;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  for (let i = 0; i < 22; i++) {
    const x = rnd() * 256;
    const y = 14 + rnd() * 100;
    g.beginPath();
    for (let k = 0; k < 5; k++) {
      const a = (k / 5) * Math.PI * 2 - Math.PI / 2;
      g.lineTo(x + Math.cos(a) * 11, y + Math.sin(a) * 11);
    }
    g.closePath();
    g.fill();
  }
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

/** Owns the Three.js scene. Reads sim state, never writes it. */
interface Look {
  bg: number;
  fog: number;
  fogNear: number;
  fogFar: number;
  sky: number;
  ground: number;
  hemi: number;
  sun: number;
  sunI: number;
  sunPos: [number, number, number];
}

const LOOKS: Record<TimeOfDay, Look> = {
  day: { bg: 0x9ec9f0, fog: 0x9ec9f0, fogNear: 60, fogFar: 180, sky: 0xdfeaff, ground: 0x3b5e2b, hemi: 1.0, sun: 0xffffff, sunI: 2.2, sunPos: [-18, 40, 30] },
  dusk: { bg: 0xdf8c63, fog: 0xd48a68, fogNear: 50, fogFar: 170, sky: 0xffd2a8, ground: 0x2a3320, hemi: 0.75, sun: 0xffa868, sunI: 1.5, sunPos: [-34, 16, 24] },
  night: { bg: 0x070b1a, fog: 0x0a1022, fogNear: 40, fogFar: 150, sky: 0x6f86c8, ground: 0x0c140c, hemi: 0.35, sun: 0x8fa6ff, sunI: 0.15, sunPos: [-18, 40, 30] },
};

export class GameView {
  readonly renderer: THREE.WebGLRenderer;
  readonly scene = new THREE.Scene();
  readonly rig = new CameraRig();
  private ball: THREE.Mesh;
  private ballShadow: THREE.Mesh;
  private kicker = new KickerView();
  private wall = new WallView();
  private keeper = new KeeperView();
  private aim = new AimViz();
  private spray = new SprayView();
  private rings = new TargetRings();
  private decoy = new DecoyView();
  private stadium: StadiumView;
  private hemi: THREE.HemisphereLight;
  private sun: THREE.DirectionalLight;
  private replay = false;

  private goal = createGoal();
  private tmp = new THREE.Vector3();
  private radius: number;
  private goalRipple = false;

  constructor(canvas: HTMLCanvasElement, ballRadius: number) {
    this.radius = ballRadius;
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;

    this.scene.background = new THREE.Color(0x9ec9f0);
    this.scene.fog = new THREE.Fog(0x9ec9f0, 60, 180);

    this.hemi = new THREE.HemisphereLight(0xdfeaff, 0x3b5e2b, 1.0);
    this.scene.add(this.hemi);
    const sun = new THREE.DirectionalLight(0xffffff, 2.2);
    this.sun = sun;
    sun.position.set(-18, 40, 30);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    const sc = sun.shadow.camera;
    sc.left = -30;
    sc.right = 30;
    sc.top = 30;
    sc.bottom = -30;
    sc.near = 5;
    sc.far = 120;
    sun.shadow.bias = -0.0004;
    this.scene.add(sun);
    this.scene.add(sun.target);
    sun.target.position.set(0, 0, 15);

    this.scene.add(createPitch());
    this.stadium = createStadium();
    this.scene.add(this.stadium.group);
    this.scene.add(this.goal.group);
    this.scene.add(this.kicker.group);
    this.scene.add(this.wall.group);
    this.scene.add(this.keeper.group);
    this.scene.add(this.aim.group);
    this.scene.add(this.spray.group);
    this.scene.add(this.rings.group);
    this.scene.add(this.decoy.group);

    this.ball = new THREE.Mesh(
      new THREE.SphereGeometry(ballRadius, 24, 16),
      new THREE.MeshStandardMaterial({ map: ballTexture(), roughness: 0.55 }),
    );
    this.ball.castShadow = true;
    this.scene.add(this.ball);

    // Soft blob under the ball so height reads even when the real shadow is tiny.
    this.ballShadow = new THREE.Mesh(
      new THREE.CircleGeometry(ballRadius * 1.5, 16),
      new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.35, depthWrite: false }),
    );
    this.ballShadow.rotation.x = -Math.PI / 2;
    this.scene.add(this.ballShadow);

    this.resize();
    window.addEventListener('resize', () => this.resize());
  }

  /** Pixel ratio and shadow quality. */
  setQuality(q: Quality): void {

    const dpr = window.devicePixelRatio || 1;
    this.renderer.setPixelRatio(q === 'low' ? 1 : q === 'medium' ? Math.min(dpr, 1.5) : Math.min(dpr, 2));
    const on = q !== 'low';
    if (this.renderer.shadowMap.enabled !== on) {
      this.renderer.shadowMap.enabled = on;
      this.scene.traverse((o) => {
        const m = (o as THREE.Mesh).material as THREE.Material | THREE.Material[] | undefined;
        if (Array.isArray(m)) m.forEach((x) => (x.needsUpdate = true));
        else if (m) m.needsUpdate = true;
      });
    }
    const size = q === 'high' ? 2048 : 1024;
    if (this.sun.shadow.mapSize.x !== size) {
      this.sun.shadow.mapSize.set(size, size);
      this.sun.shadow.map?.dispose();
      (this.sun.shadow as unknown as { map: unknown }).map = null;
    }
    this.stadium.lights.forEach((l, i) => (l.castShadow = on && i === 0));
    this.resize();
  }

  /** Day / dusk / night: sky, light colours and the stadium floodlights. */
  setTimeOfDay(t: TimeOfDay): void {
    const l = LOOKS[t];
    (this.scene.background as THREE.Color).setHex(l.bg);
    this.scene.fog = new THREE.Fog(l.fog, l.fogNear, l.fogFar);
    this.hemi.color.setHex(l.sky);
    this.hemi.groundColor.setHex(l.ground);
    this.hemi.intensity = l.hemi;
    this.sun.color.setHex(l.sun);
    this.sun.intensity = l.sunI;
    this.sun.position.set(...l.sunPos);
    this.stadium.setTime(t);
  }

  /** While replaying, the aiming aids are hidden. */
  setReplay(on: boolean): void {
    this.replay = on;
  }

  /** World x/z on the grass under a screen position (for placing the ball), or null. */
  pickGround(clientX: number, clientY: number): { x: number; z: number } | null {
    const r = this.renderer.domElement.getBoundingClientRect();
    const ndc = new THREE.Vector2(((clientX - r.left) / r.width) * 2 - 1, -((clientY - r.top) / r.height) * 2 + 1);
    const rc = new THREE.Raycaster();
    rc.setFromCamera(ndc, this.rig.camera);
    const pt = new THREE.Vector3();
    return rc.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), pt) ? { x: pt.x, z: pt.z } : null;
  }

  resize(): void {
    const w = window.innerWidth;
    const h = window.innerHeight;
    this.renderer.setSize(w, h, false);
    this.rig.resize(w, h);
  }

  /** Impact events from the sim that need a visual reaction. */
  onSimEvent(e: { type: string; speed?: number; pos?: { x: number; y: number; z: number } }): void {
    if (e.type === 'net' && e.pos) {
      this.goal.ripple(new THREE.Vector3(e.pos.x, e.pos.y, e.pos.z), e.speed ?? 10);
    } else if (e.type === 'goal') {
      this.goalRipple = true; // billow the net as the ball crosses the line
      this.stadium.cheer();
    }
  }

  render(s: SimState, alpha: number, dt: number): void {
    const p = s.prevBallPos;
    const q = s.ball.pos;
    this.tmp.set(p.x + (q.x - p.x) * alpha, p.y + (q.y - p.y) * alpha, p.z + (q.z - p.z) * alpha);
    this.ball.position.copy(this.tmp);

    // Visual spin: integrate the angular velocity.
    const w = s.ball.spin;
    const wl = Math.hypot(w.x, w.y, w.z);
    if (wl > 1e-3) {
      this.ball.rotateOnWorldAxis(new THREE.Vector3(w.x / wl, w.y / wl, w.z / wl), wl * dt);
    }

    this.ballShadow.position.set(this.tmp.x, 0.02, this.tmp.z);
    const h = Math.max(0, this.tmp.y - this.radius);
    this.ballShadow.scale.setScalar(1 + h * 0.15);
    (this.ballShadow.material as THREE.MeshBasicMaterial).opacity = Math.max(0.08, 0.35 - h * 0.05);

    if (this.goalRipple) {
      this.goalRipple = false;
      this.goal.ripple(this.tmp.clone().setZ(-0.3), 14);
    }
    this.kicker.update(s);
    this.decoy.update(s);
    this.wall.update(s);
    this.keeper.update(s);
    this.rig.update(s, this.tmp, dt);
    if (this.replay) {
      this.aim.group.visible = false;
      this.spray.group.visible = false;
    } else {
      this.aim.update(s, this.rig.camera.position);
      this.spray.update(s);
    }
    this.rings.update(s, dt);
    this.stadium.update(dt);
    this.goal.update(dt);
    this.renderer.render(this.scene, this.rig.camera);
  }
}
