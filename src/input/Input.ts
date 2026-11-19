import { InputState, emptyInput } from '../sim/Sim';
import { Keymap, RESERVED_KEYS, defaultKeymap, isHeld } from './keymap';
import { aimDeltaFromMouse, clampSens, moveContactCursor } from './mouse';

const DEAD = 0.18;
const dz = (v: number) => (Math.abs(v) < DEAD ? 0 : v);

export interface Polled {
  input: InputState;
  /** One-shot edges for things the view/app handles rather than the sim. */
  cameraToggle: boolean;
  replayToggle: boolean;
  placeToggle: boolean;
}

/**
 * Keyboard + mouse + Gamepad API -> the abstract InputState the sim consumes.
 *
 * Mouse: click the pitch to capture the pointer; moving the mouse then moves the
 * reticle, holding the right button moves the contact point, left button is
 * shoot (hold to charge, release to strike). Esc releases the pointer. Where
 * pointer lock is unavailable you can drag instead (left = reticle, right = contact).
 *
 * Contact point: the gamepad right stick is absolute; the keyboard moves a
 * cursor that stays put when released, and the mouse moves the same cursor.
 */
export class Input {
  private down = new Set<string>();
  /** Keys pressed since the last poll: a tap shorter than one frame still counts. */
  private latched = new Set<string>();
  private cursor = { x: 0, y: 0 };
  private edges = { camera: false, replay: false, place: false };
  private prevPad = { camera: false, replay: false };
  private acc = { dx: 0, dy: 0 };
  private buttons = { left: false, right: false };
  private dragging = false;
  private enabled = true;
  usingPad = false;

  keymap: Keymap = defaultKeymap();
  mouseAim = true;
  sensitivity = 1;
  /** In practice mode: a click on the pitch places the ball instead of capturing the mouse. */
  placing = false;
  onGroundClick?: (clientX: number, clientY: number) => void;
  onLockChange?: (locked: boolean) => void;

  constructor(private canvas: HTMLElement) {
    window.addEventListener('keydown', (e) => {
      if (!this.enabled) return;
      if (RESERVED_KEYS.includes(e.code)) return;
      const km = this.keymap;
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].includes(e.code)) e.preventDefault();
      if (e.repeat) return;
      this.down.add(e.code);
      this.latched.add(e.code);
      if (km.camera.includes(e.code)) this.edges.camera = true;
      if (km.replay.includes(e.code)) this.edges.replay = true;
      if (km.placeBall.includes(e.code)) this.edges.place = true;
      if (km.recenter.includes(e.code)) this.cursor = { x: 0, y: 0 };
    });
    window.addEventListener('keyup', (e) => this.down.delete(e.code));
    window.addEventListener('blur', () => {
      this.down.clear();
      this.buttons = { left: false, right: false };
      this.dragging = false;
    });

    canvas.addEventListener('contextmenu', (e) => e.preventDefault());
    canvas.addEventListener('mousedown', (e) => {
      if (!this.enabled) return;
      if (e.button === 0) this.buttons.left = true;
      if (e.button === 2) this.buttons.right = true;
      this.dragging = true;
    });
    window.addEventListener('mouseup', (e) => {
      if (e.button === 0) this.buttons.left = false;
      if (e.button === 2) this.buttons.right = false;
      if (!this.buttons.left && !this.buttons.right) this.dragging = false;
    });
    canvas.addEventListener('click', (e) => {
      if (!this.enabled || e.button !== 0) return;
      if (this.placing) this.onGroundClick?.(e.clientX, e.clientY);
      else if (this.mouseAim && !this.locked) this.lock();
    });
    document.addEventListener('pointerlockchange', () => {
      if (!this.locked) {
        this.buttons = { left: false, right: false };
        this.dragging = false;
      }
      this.onLockChange?.(this.locked);
    });
    window.addEventListener('mousemove', (e) => {
      if (!this.enabled || !this.mouseAim) return;
      if (this.locked) this.mouseMove(e.movementX, e.movementY, this.buttons.right);
      else if (this.dragging) this.mouseMove(e.movementX, e.movementY, this.buttons.right && !this.buttons.left);
    });
  }

  get locked(): boolean {
    return document.pointerLockElement === this.canvas;
  }

  lock(): void {
    try {
      void (this.canvas.requestPointerLock() as unknown as Promise<void> | undefined)?.catch?.(() => undefined);
    } catch {
      /* unsupported: dragging still works */
    }
  }

  release(): void {
    if (this.locked) document.exitPointerLock();
  }

  private mouseMove(dx: number, dy: number, contact: boolean): void {
    if (contact) this.cursor = moveContactCursor(this.cursor, dx, dy, this.sensitivity);
    else {
      this.acc.dx += dx;
      this.acc.dy += dy;
    }
  }

  /** Disable while a UI panel is open so typing/sliders don't steer the game. */
  setEnabled(on: boolean): void {
    this.enabled = on;
    if (!on) {
      this.down.clear();
      this.latched.clear();
      this.edges = { camera: false, replay: false, place: false };
      this.acc = { dx: 0, dy: 0 };
      this.buttons = { left: false, right: false };
      this.dragging = false;
      this.release();
    }
  }

  setSensitivity(s: number): void {
    this.sensitivity = clampSens(s);
  }

  private held(id: Parameters<typeof isHeld>[2]): boolean {
    return isHeld(this.keymap, this.down, id) || isHeld(this.keymap, this.latched, id);
  }

  /** Returns the sim input for this frame, plus one-shot flags for the app. */
  poll(dt: number): Polled {
    const i = emptyInput();
    const h = (id: Parameters<typeof isHeld>[2]) => (this.held(id) ? 1 : 0);

    i.aimX = h('aimRight') - h('aimLeft');
    i.aimY = h('aimUp') - h('aimDown');
    i.runupAngle = h('runupWider') - h('runupNarrower');
    i.runupDist = h('runupLonger') - h('runupShorter');

    const rate = 1.8;
    this.cursor.x += (h('contactRight') - h('contactLeft')) * rate * dt;
    this.cursor.y += (h('contactUp') - h('contactDown')) * rate * dt;
    const m = Math.hypot(this.cursor.x, this.cursor.y);
    if (m > 1) {
      this.cursor.x /= m;
      this.cursor.y /= m;
    }
    i.contactX = this.cursor.x;
    i.contactY = this.cursor.y;

    // Mouse reticle movement: applied once per frame by the app (not per sim step).
    const d = aimDeltaFromMouse(this.acc.dx, this.acc.dy, this.sensitivity);
    i.aimDeltaX = d.x;
    i.aimDeltaY = d.y;
    this.acc = { dx: 0, dy: 0 };

    i.shoot = this.held('shoot') || (this.locked && this.buttons.left);
    i.driven = this.held('driven');
    i.low = this.held('low');
    i.dummy = this.held('dummy');
    i.toggleFoot = this.held('toggleFoot');
    i.retry = this.held('retry');
    i.newSpot = this.held('newSpot');

    let cam = this.edges.camera;
    let rep = this.edges.replay;
    const place = this.edges.place;
    this.edges = { camera: false, replay: false, place: false };

    const pad = navigator.getGamepads?.().find((g) => g && g.connected);
    if (pad) {
      const b = (n: number) => !!pad.buttons[n]?.pressed;
      const ax = (n: number) => dz(pad.axes[n] ?? 0);
      const lx = ax(0);
      const ly = ax(1);
      const rx = ax(2);
      const ry = ax(3);
      const active = lx !== 0 || ly !== 0 || rx !== 0 || ry !== 0 || pad.buttons.some((x) => x.pressed);
      if (active) this.usingPad = true;
      if (lx !== 0 || ly !== 0) {
        i.aimX = lx;
        i.aimY = -ly;
      }
      if (this.usingPad) {
        // Absolute contact point from the right stick.
        i.contactX = rx;
        i.contactY = -ry;
        this.cursor = { x: rx, y: -ry };
      }
      const trig = (pad.buttons[7]?.value ?? 0) - (pad.buttons[6]?.value ?? 0);
      if (Math.abs(trig) > 0.1) i.runupAngle = trig;
      if (b(12) || b(13)) i.runupDist = (b(12) ? 1 : 0) - (b(13) ? 1 : 0);
      i.shoot = i.shoot || b(1);
      i.driven = i.driven || b(5);
      i.low = i.low || b(4);
      i.dummy = i.dummy || b(10);
      i.toggleFoot = i.toggleFoot || b(3);
      i.newSpot = i.newSpot || b(2);
      i.retry = i.retry || b(9);
      const camBtn = b(8);
      if (camBtn && !this.prevPad.camera) cam = true;
      this.prevPad.camera = camBtn;
      const repBtn = b(11);
      if (repBtn && !this.prevPad.replay) rep = true;
      this.prevPad.replay = repBtn;
    }

    this.latched.clear();
    return { input: i, cameraToggle: cam, replayToggle: rep, placeToggle: place };
  }
}
