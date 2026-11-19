import ai from '../data/ai.json';
import challenges from '../data/challenges.json';
import physics from '../data/physics.json';
import presets from '../data/presets.json';
import shot from '../data/shot.json';
import takers from '../data/takers.json';
import { GameAudio } from './audio/Audio';
import { crowdExcitement, cuesFor } from './audio/cues';
import { buildControlsPanel } from './hud/ControlsPanel';
import { Hud, HudUi, ReplayHud } from './hud/Hud';
import { Menu } from './hud/Menu';
import { ClientCtx, buildSettingsPanel } from './hud/SettingsPanel';
import { clearPrefs, loadPrefs, savePrefs } from './hud/storage';
import { Input } from './input/Input';
import { keyLabel } from './input/keymap';
import { loadClientPrefs, saveClientPrefs } from './prefs';
import { CAM_LABEL, CamKind, Recorder, ReplayPlayer, camerasFor, replayCamera, viewStateFor } from './replay/Replay';
import { GameView } from './render/GameView';
import { PhysicsParams } from './sim/BallPhysics';
import { AiParams } from './sim/ai';
import { ChallengeDef, Medal } from './sim/challenge';
import { DifficultyName, DifficultyPreset } from './sim/presets';
import { ShotParams, Taker } from './sim/ShotModel';
import { SimConfig, applyPreset, applySettings, createSim, selectTaker, setMode, setSpot, startChallenge, step } from './sim/Sim';
import { defaultSettings } from './sim/settings';

const cfg: SimConfig = {
  physics: physics as PhysicsParams,
  shot: shot as ShotParams,
  ai: ai as AiParams,
  takers: (takers as Taker[]).map((t) => ({ ...t })),
  challenges: challenges as ChallengeDef[],
  presets: presets as Record<DifficultyName, DifficultyPreset>,
};

const SIM_DT = 1 / 240;
const MEDAL_RANK: Record<Medal, number> = { bronze: 1, silver: 2, gold: 3 };

const canvas = document.getElementById('game') as HTMLCanvasElement;
const hudRoot = document.getElementById('hud')!;
const sim = createSim(cfg, (Math.random() * 1e9) | 0);
const defaultTakerStats = cfg.takers.map((t) => ({ accuracy: t.accuracy, curve: t.curve, power: t.power }));

// ------------------------------------------------------------------ preferences
const prefs = loadClientPrefs();
const saved = loadPrefs();
if (saved) {
  Object.assign(sim.settings, saved.settings);
  if (saved.taker) Object.assign(sim.taker, saved.taker);
  if (saved.foot) sim.foot = saved.foot;
  if (saved.runup && Number.isFinite(saved.runup.angle) && Number.isFinite(saved.runup.dist)) {
    sim.runup.angle = Math.min(cfg.shot.runupAngleMax, Math.max(0, saved.runup.angle));
    sim.runup.dist = Math.min(cfg.shot.runupDistMax, Math.max(cfg.shot.runupDistMin, saved.runup.dist));
  }
  if (saved.showPath) sim.showPath = true;
}
selectTaker(sim, prefs.takerIndex % cfg.takers.length);
if (saved?.foot) sim.foot = saved.foot;
sim.difficulty = prefs.difficulty;
if (prefs.difficulty !== 'custom') sim.accuracyBonus = cfg.presets[prefs.difficulty].accuracyBonus;
applySettings(sim);
if (prefs.mode === 'challenge') startChallenge(sim, prefs.challengeIndex);
else setMode(sim, prefs.mode);

function persist(): void {
  // While a challenge is running its rules are temporary: remember the player's own settings instead.
  const settings = sim.challenge ? sim.challenge.backup : sim.settings;
  savePrefs({
    settings: { ...settings },
    taker: { accuracy: sim.taker.accuracy, curve: sim.taker.curve, power: sim.taker.power },
    foot: sim.foot,
    runup: { ...sim.runup },
    showPath: sim.showPath,
  });
  prefs.mode = sim.mode;
  prefs.takerIndex = sim.takerIndex;
  prefs.challengeIndex = sim.challenge ? sim.challenge.index : prefs.challengeIndex;
  prefs.difficulty = sim.difficulty;
  saveClientPrefs(prefs);
}

// ------------------------------------------------------------------ subsystems
const view = new GameView(canvas, cfg.physics.radius);
const input = new Input(canvas);
const audio = new GameAudio();
const recorder = new Recorder(1 / SIM_DT);
const hud = new Hud(hudRoot, cfg.shot.sweetMin, cfg.shot.sweetMax);

function applyClientPrefs(): void {
  view.setQuality(prefs.quality);
  view.setTimeOfDay(prefs.timeOfDay);
  view.rig.distanceMul = prefs.cameraDistance;
  input.keymap = prefs.keymap;
  input.mouseAim = prefs.mouseAim;
  input.setSensitivity(prefs.mouseSensitivity);
  audio.setVolumes({ master: prefs.masterVolume, crowd: prefs.crowdVolume, muted: prefs.muted });
  if (!prefs.mouseAim) input.release();
}
applyClientPrefs();

// Browsers only allow audio after a user gesture.
const unlock = () => audio.resume();
window.addEventListener('pointerdown', unlock);
window.addEventListener('keydown', unlock);

// ------------------------------------------------------------------ UI wiring
let paused = false;
let placing = false;
const setPlacing = (on: boolean) => {
  placing = on && sim.mode === 'practice';
  input.placing = placing;
};
input.onGroundClick = (x, y) => {
  const p = view.pickGround(x, y);
  if (p && sim.phase === 'aim' && sim.mode === 'practice') {
    setSpot(sim, Math.max(-34, Math.min(34, p.x)), Math.max(2.5, Math.min(48, p.z)));
    persist();
  }
  setPlacing(false);
};

const ctx: ClientCtx = {
  prefs,
  takers: cfg.takers,
  challenges: cfg.challenges,
  presets: cfg.presets,
  apply() {
    applyClientPrefs();
    persist();
  },
  setMode(mode) {
    setMode(sim, mode, prefs.challengeIndex);
    setPlacing(false);
    persist();
    settings.sync();
  },
  startChallenge(index) {
    startChallenge(sim, index);
    setPlacing(false);
    persist();
    settings.sync();
  },
  selectTaker(index) {
    selectTaker(sim, index);
    persist();
    settings.sync();
  },
  applyDifficulty(name) {
    applyPreset(sim, name);
    persist();
    settings.sync();
  },
  togglePlacing() {
    setPlacing(!placing);
  },
  isPlacing: () => placing,
};

const controls = buildControlsPanel({
  getKeymap: () => prefs.keymap,
  setKeymap(k) {
    prefs.keymap = k;
    applyClientPrefs();
    saveClientPrefs(prefs);
  },
});

const settings = buildSettingsPanel(
  sim,
  persist,
  () => {
    // "Reset all settings"
    Object.assign(sim.settings, defaultSettings());
    cfg.takers.forEach((t, i) => Object.assign(t, defaultTakerStats[i]));
    sim.accuracyBonus = 0;
    sim.runup.angle = cfg.shot.runupAngleDefault;
    sim.runup.dist = cfg.shot.runupDistDefault;
    sim.showPath = false;
    sim.previewKey = '';
    applySettings(sim);
    clearPrefs();
    persist();
    settings.sync();
  },
  ctx,
);

const menu = new Menu(
  hudRoot,
  controls.el,
  settings.el,
  (open) => {
    paused = open;
    input.setEnabled(!open);
  },
  () => settings.sync(),
  () => startReplay(),
  () => controls.refresh(),
);

// ------------------------------------------------------------------ replay
interface ReplayState {
  player: ReplayPlayer;
  cams: CamKind[];
  cam: number;
  /** Cycles through the cameras on its own until the player picks one. */
  auto: boolean;
  camT: number;
  autoStartedFor: unknown;
}
let replay: ReplayState | null = null;
let lastAutoReplay: unknown = null;

function startReplay(): void {
  const rec = recorder.recording;
  if (!rec || paused) return;
  const cams = camerasFor(rec.meta);
  replay = { player: new ReplayPlayer(rec, prefs.replaySpeed, true), cams, cam: 0, auto: true, camT: 0, autoStartedFor: rec };
  view.setReplay(true);
  input.release();
}

function stopReplay(): void {
  replay = null;
  view.setReplay(false);
  view.rig.setShot(null);
}

window.addEventListener('keydown', (e) => {
  if (!replay) return;
  const r = replay;
  if (e.code === 'Escape' || prefs.keymap.replay.includes(e.code)) {
    stopReplay();
  } else if (/^Digit[1-9]$/.test(e.code)) {
    const i = Number(e.code.slice(5)) - 1;
    if (i < r.cams.length) {
      r.cam = i;
      r.auto = false;
    }
  } else if (e.code === 'Space') {
    r.player.paused = !r.player.paused;
    e.preventDefault();
  } else if (e.code === 'ArrowLeft' || e.code === 'ArrowRight') {
    const speeds = [0.25, 0.5, 1];
    const i = speeds.indexOf(r.player.speed);
    r.player.speed = speeds[Math.max(0, Math.min(speeds.length - 1, i + (e.code === 'ArrowRight' ? 1 : -1)))];
    e.preventDefault();
  } else return;
  e.stopPropagation();
}, true);

// Exposed for tuning/debugging from the console.
Object.assign(window, { sim, simApi: { setSpot, applySettings }, prefs });

// ------------------------------------------------------------------ main loop
let last = performance.now();
let acc = 0;
let pendDX = 0;
let pendDY = 0;
let prevPhase = sim.phase;
let recordedChallenge: unknown = null;

function recordChallengeResult(): void {
  const ch = sim.challenge;
  if (!ch || !ch.done || recordedChallenge === ch) return;
  recordedChallenge = ch;
  const id = ch.def.id;
  if (ch.points > (prefs.best[id] ?? -1)) prefs.best[id] = ch.points;
  const had = prefs.medals[id];
  if (ch.medal && (!had || MEDAL_RANK[ch.medal] > MEDAL_RANK[had])) prefs.medals[id] = ch.medal;
  saveClientPrefs(prefs);
  settings.sync();
}

function frame(now: number): void {
  const frameDt = Math.min(0.1, (now - last) / 1000);
  last = now;

  const polled = input.poll(frameDt);
  if (polled.replayToggle && !paused) {
    if (replay) stopReplay();
    else startReplay();
  }
  if (polled.placeToggle && !paused) setPlacing(!placing);

  menu.setReplayAvailable(!!recorder.recording);
  let replayHud: ReplayHud | null = null;

  if (replay) {
    // ---- instant replay: the sim is frozen, the recording is redrawn from several cameras
    const r = replay;
    r.player.advance(frameDt, (e) => {
      view.onSimEvent(e);
      cuesFor(e).forEach((c) => audio.play(c));
    });
    if (r.auto && !r.player.paused) {
      r.camT += frameDt;
      if (r.camT > 2.4) {
        r.camT = 0;
        r.cam = (r.cam + 1) % r.cams.length;
      }
    }
    const rec = r.player.rec;
    const sample = r.player.sample();
    const kind = r.cams[r.cam];
    view.rig.setShot(replayCamera(kind, rec.meta, sample));
    view.render(viewStateFor(sim, rec, sample), 1, r.player.paused ? 0 : frameDt * r.player.speed);
    replayHud = {
      camLabel: CAM_LABEL[kind],
      camIndex: r.cam + 1,
      camCount: r.cams.length,
      speed: r.player.speed,
      paused: r.player.paused,
      progress: rec.duration > 0 ? r.player.time / rec.duration : 0,
    };
  } else {
    // ---- live game
    if (!paused) {
      if (polled.cameraToggle) view.rig.toggle();
      pendDX += polled.input.aimDeltaX;
      pendDY += polled.input.aimDeltaY;
      acc += frameDt;
      while (acc >= SIM_DT) {
        // Mouse movement is applied once, on the first step after it happened.
        step(sim, { ...polled.input, aimDeltaX: pendDX, aimDeltaY: pendDY }, SIM_DT);
        pendDX = pendDY = 0;
        recorder.onStep(sim, SIM_DT);
        acc -= SIM_DT;
      }
    }

    for (const e of sim.events) {
      view.onSimEvent(e);
      cuesFor(e).forEach((c) => audio.play(c));
    }
    sim.events.length = 0;
    recorder.frameDone();

    // A whistle as the next attempt is set up.
    if (prevPhase === 'result' && sim.phase === 'aim') audio.play({ name: 'whistle', gain: 0.55 });
    prevPhase = sim.phase;
    recordChallengeResult();

    // Optional: replay every goal automatically once the recording is complete.
    const rec = recorder.recording;
    if (prefs.autoReplay && rec && rec !== lastAutoReplay && rec.meta.resultType === 'goal' && !paused && sim.phase === 'result') {
      lastAutoReplay = rec;
      startReplay();
    }
    view.render(sim, acc / SIM_DT, paused ? 0 : frameDt);
  }

  audio.setExcitement(crowdExcitement(sim));
  const ui: HudUi = {
    mouseAim: prefs.mouseAim,
    mouseLocked: input.locked,
    placing,
    canReplay: !!recorder.recording,
    replayKey: keyLabel(prefs.keymap.replay[0]),
    retryKey: keyLabel(prefs.keymap.retry[0]),
    nextKey: keyLabel(prefs.keymap.newSpot[0]),
    replay: replayHud,
  };
  hud.update(sim, ui);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
