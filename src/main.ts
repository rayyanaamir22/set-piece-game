import physics from '../data/physics.json';
import shot from '../data/shot.json';
import takers from '../data/takers.json';
import { Hud } from './hud/Hud';
import { Menu } from './hud/Menu';
import { buildSettingsPanel } from './hud/SettingsPanel';
import { clearPrefs, loadPrefs, savePrefs } from './hud/storage';
import { Input } from './input/Input';
import { GameView } from './render/GameView';
import { PhysicsParams } from './sim/BallPhysics';
import { ShotParams, Taker } from './sim/ShotModel';
import { SimConfig, applySettings, createSim, step } from './sim/Sim';
import { defaultSettings } from './sim/settings';

const cfg: SimConfig = {
  physics: physics as PhysicsParams,
  shot: shot as ShotParams,
  takers: (takers as Taker[]).map((t) => ({ ...t })),
};

const SIM_DT = 1 / 240;

const canvas = document.getElementById('game') as HTMLCanvasElement;
const hudRoot = document.getElementById('hud')!;
const sim = createSim(cfg, (Math.random() * 1e9) | 0);
const defaultTaker = { ...sim.taker };

// Restore saved preferences.
const prefs = loadPrefs();
if (prefs) {
  Object.assign(sim.settings, prefs.settings);
  if (prefs.taker) Object.assign(sim.taker, prefs.taker);
  if (prefs.foot) sim.foot = prefs.foot;
  if (prefs.showPath) sim.showPath = true;
}
applySettings(sim);

function persist() {
  savePrefs({
    settings: { ...sim.settings },
    taker: { accuracy: sim.taker.accuracy, curve: sim.taker.curve, power: sim.taker.power },
    foot: sim.foot,
    showPath: sim.showPath,
  });
}

const view = new GameView(canvas, cfg.physics.radius);
const hud = new Hud(hudRoot, cfg.shot.sweetMin, cfg.shot.sweetMax);
const input = new Input();

let paused = false;
const settings = buildSettingsPanel(
  sim,
  persist,
  () => {
    // "Reset all settings"
    Object.assign(sim.settings, defaultSettings());
    Object.assign(sim.taker, defaultTaker);
    sim.showPath = false;
    sim.previewKey = '';
    applySettings(sim);
    clearPrefs();
    settings.sync();
  },
);
new Menu(
  hudRoot,
  settings.el,
  (open) => {
    paused = open;
    input.setEnabled(!open);
  },
  () => settings.sync(),
);

// Exposed for tuning/debugging from the console.
(window as unknown as { sim: unknown }).sim = sim;

let last = performance.now();
let acc = 0;

function frame(now: number) {
  const frameDt = Math.min(0.1, (now - last) / 1000);
  last = now;

  if (!paused) {
    const { input: inp, cameraToggle } = input.poll(frameDt);
    if (cameraToggle) view.rig.toggle();

    acc += frameDt;
    while (acc >= SIM_DT) {
      step(sim, inp, SIM_DT);
      acc -= SIM_DT;
    }
  }

  for (const e of sim.events) view.onSimEvent(e);
  sim.events.length = 0;

  view.render(sim, acc / SIM_DT, paused ? 0 : frameDt);
  hud.update(sim);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
