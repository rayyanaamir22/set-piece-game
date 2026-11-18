import physics from '../data/physics.json';
import shot from '../data/shot.json';
import takers from '../data/takers.json';
import { createDebugPanel } from './debug/Panel';
import { Hud } from './hud/Hud';
import { Input } from './input/Input';
import { GameView } from './render/GameView';
import { PhysicsParams } from './sim/BallPhysics';
import { ShotParams, Taker } from './sim/ShotModel';
import { SimConfig, createSim, step } from './sim/Sim';

const cfg: SimConfig = {
  physics: physics as PhysicsParams,
  shot: shot as ShotParams,
  takers: takers as Taker[],
};

const SIM_DT = 1 / 240;

const canvas = document.getElementById('game') as HTMLCanvasElement;
const sim = createSim(cfg, (Math.random() * 1e9) | 0);
const view = new GameView(canvas, cfg.physics.radius);
const hud = new Hud(document.getElementById('hud')!, cfg.shot.sweetMin, cfg.shot.sweetMax);
const input = new Input();
createDebugPanel(sim);

// Exposed for tuning/debugging from the console.
(window as unknown as { sim: unknown }).sim = sim;

let last = performance.now();
let acc = 0;

function frame(now: number) {
  const frameDt = Math.min(0.1, (now - last) / 1000);
  last = now;

  const { input: inp, cameraToggle } = input.poll(frameDt);
  if (cameraToggle) view.rig.toggle();

  acc += frameDt;
  while (acc >= SIM_DT) {
    step(sim, inp, SIM_DT);
    acc -= SIM_DT;
  }

  for (const e of sim.events) view.onSimEvent(e);
  sim.events.length = 0;

  view.render(sim, acc / SIM_DT, frameDt);
  hud.update(sim);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
