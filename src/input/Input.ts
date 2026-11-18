import { InputState, emptyInput } from '../sim/Sim';

const DEAD = 0.18;
const dz = (v: number) => (Math.abs(v) < DEAD ? 0 : v);

/**
 * Keyboard + Gamepad API -> the abstract InputState the sim consumes.
 * Contact point: gamepad right stick is absolute; on keyboard IJKL move a
 * cursor that stays put when released (O re-centres it).
 */
export class Input {
  private keys = new Set<string>();
  private cursor = { x: 0, y: 0 };
  private cameraEdge = false;
  private prevCamera = false;
  usingPad = false;

  constructor() {
    window.addEventListener('keydown', (e) => {
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) e.preventDefault();
      if (e.repeat) return;
      this.keys.add(e.code);
      if (e.code === 'KeyC') this.cameraEdge = true;
      if (e.code === 'KeyO') this.cursor = { x: 0, y: 0 };
    });
    window.addEventListener('keyup', (e) => this.keys.delete(e.code));
    window.addEventListener('blur', () => this.keys.clear());
  }

  private k(...codes: string[]): boolean {
    return codes.some((c) => this.keys.has(c));
  }

  /** Returns the sim input for this frame. `cameraToggle` is a one-shot flag for the view. */
  poll(dt: number): { input: InputState; cameraToggle: boolean } {
    const i = emptyInput();

    i.aimX = (this.k('KeyD', 'ArrowRight') ? 1 : 0) - (this.k('KeyA', 'ArrowLeft') ? 1 : 0);
    i.aimY = (this.k('KeyW', 'ArrowUp') ? 1 : 0) - (this.k('KeyS', 'ArrowDown') ? 1 : 0);

    const rate = 1.8;
    this.cursor.x += ((this.k('KeyL') ? 1 : 0) - (this.k('KeyJ') ? 1 : 0)) * rate * dt;
    this.cursor.y += ((this.k('KeyI') ? 1 : 0) - (this.k('KeyK') ? 1 : 0)) * rate * dt;
    const m = Math.hypot(this.cursor.x, this.cursor.y);
    if (m > 1) {
      this.cursor.x /= m;
      this.cursor.y /= m;
    }
    i.contactX = this.cursor.x;
    i.contactY = this.cursor.y;

    i.shoot = this.k('Space');
    i.driven = this.k('ShiftLeft', 'ShiftRight');
    i.low = this.k('ControlLeft', 'ControlRight', 'KeyZ');
    i.toggleFoot = this.k('KeyF');
    i.retry = this.k('KeyR');
    i.newSpot = this.k('KeyN');

    let cam = this.cameraEdge;
    this.cameraEdge = false;

    const pad = navigator.getGamepads?.().find((g) => g && g.connected);
    if (pad) {
      const b = (n: number) => !!pad.buttons[n]?.pressed;
      const ax = (n: number) => dz(pad.axes[n] ?? 0);
      const lx = ax(0);
      const ly = ax(1);
      const rx = ax(2);
      const ry = ax(3);
      const active =
        lx !== 0 || ly !== 0 || rx !== 0 || ry !== 0 || pad.buttons.some((x) => x.pressed);
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
      i.shoot = i.shoot || b(1);
      i.driven = i.driven || b(5);
      i.low = i.low || b(4);
      i.toggleFoot = i.toggleFoot || b(3);
      i.newSpot = i.newSpot || b(2);
      i.retry = i.retry || b(9);
      const camBtn = b(8);
      if (camBtn && !this.prevCamera) cam = true;
      this.prevCamera = camBtn;
    }

    return { input: i, cameraToggle: cam };
  }
}
