import * as THREE from 'three';
import { SimState } from '../sim/Sim';
import { AimViz } from './AimViz';
import { CameraRig } from './CameraRig';
import { KickerView } from './Figures';
import { createGoal } from './Goal';
import { createPitch } from './Pitch';
import { KeeperView } from './KeeperView';
import { createStadium } from './Stadium';
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

    this.scene.add(new THREE.HemisphereLight(0xdfeaff, 0x3b5e2b, 1.0));
    const sun = new THREE.DirectionalLight(0xffffff, 2.2);
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
    this.scene.add(createStadium());
    this.scene.add(this.goal.group);
    this.scene.add(this.kicker.group);
    this.scene.add(this.wall.group);
    this.scene.add(this.keeper.group);
    this.scene.add(this.aim.group);

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
    this.wall.update(s);
    this.keeper.update(s);
    this.rig.update(s, this.tmp, dt);
    this.aim.update(s, this.rig.camera.position);
    this.goal.update(dt);
    this.renderer.render(this.scene, this.rig.camera);
  }
}
