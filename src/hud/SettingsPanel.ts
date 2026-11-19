import type { ClientPrefs, Quality, TimeOfDay } from '../prefs';
import { REPLAY_SPEEDS } from '../prefs';
import { SENS_MAX, SENS_MIN } from '../input/mouse';
import type { ChallengeDef } from '../sim/challenge';
import { DIFFICULTIES, DifficultyName, DifficultyPreset } from '../sim/presets';
import type { Taker } from '../sim/ShotModel';
import {
  Mode,
  SimState,
  applySettings,
  randomSpot,
  setSpot,
} from '../sim/Sim';
import {
  Frequency,
  KeeperType,
  WALL_COUNT_MAX,
  WALL_HEIGHT_MAX,
  WALL_HEIGHT_MIN,
  WIND_SPEED_MAX,
  defaultSettings,
} from '../sim/settings';

interface Base {
  label: string;
  /** Shown as a "soon" badge; the control is greyed out because the feature isn't simulated yet. */
  soon?: boolean;
  /** Help text under the control; a function is re-read whenever the panel refreshes. */
  note?: string | (() => string);
}
interface RangeRow extends Base {
  kind: 'range';
  min: number;
  max: number;
  step: number;
  get(): number;
  set(v: number): void;
  fmt(v: number): string;
}
interface ToggleRow extends Base {
  kind: 'toggle';
  get(): boolean;
  set(v: boolean): void;
}
interface SegRow extends Base {
  kind: 'segmented';
  options: { label: string; value: string }[];
  get(): string;
  set(v: string): void;
}
interface ButtonRow extends Base {
  kind: 'button';
  text: string;
  onClick(): void;
}
type Row = RangeRow | ToggleRow | SegRow | ButtonRow;
interface Section {
  title: string;
  rows: Row[];
}

export interface SettingsPanel {
  el: HTMLElement;
  /** Re-read all values from the sim (call when the panel opens). */
  sync(): void;
}

/** Everything about the player's own setup (not the rules of the free kick) that the panel needs. */
export interface ClientCtx {
  prefs: ClientPrefs;
  takers: Taker[];
  challenges: ChallengeDef[];
  presets: Record<DifficultyName, DifficultyPreset>;
  /** Persist the client prefs and push them into the running subsystems (audio, graphics, mouse…). */
  apply(): void;
  setMode(mode: Mode): void;
  startChallenge(index: number): void;
  selectTaker(index: number): void;
  applyDifficulty(name: DifficultyName): void;
  /** Practice: toggle click-to-place mode. */
  togglePlacing(): void;
  isPlacing(): boolean;
}

export function buildSettingsPanel(
  s: SimState,
  onChange: () => void,
  onReset: () => void,
  ctx: ClientCtx,
): SettingsPanel {
  // Ball placement is edited locally and applied with a button.
  const place = {
    distance: Math.hypot(s.spot.x, s.spot.z),
    angle: (Math.atan2(s.spot.x, s.spot.z) * 180) / Math.PI,
  };
  const st = s.settings;
  const P = ctx.prefs;
  const changed = () => {
    applySettings(s);
    // Hand-editing a rule means it is no longer one of the presets.
    s.difficulty = 'custom';
    P.difficulty = 'custom';
    onChange();
  };
  const medalIcon = (id: string) => ({ gold: '🥇', silver: '🥈', bronze: '🥉' } as Record<string, string>)[P.medals[id] ?? ''] ?? '';
  const client = <K extends keyof ClientPrefs>(k: K) => ({
    get: () => P[k] as never,
    set: (v: never) => {
      P[k] = v;
      ctx.apply();
    },
  });

  const pre: Section[] = [
    {
      title: 'Game mode',
      rows: [
        {
          kind: 'segmented', label: 'Mode',
          options: [{ label: 'Free kicks', value: 'free' }, { label: 'Practice', value: 'practice' }, { label: 'Challenge', value: 'challenge' }],
          get: () => s.mode,
          set: (v) => ctx.setMode(v as Mode),
          note: () =>
            s.mode === 'free' ? 'A new spot every kick. Scored by goals.'
              : s.mode === 'practice' ? 'The ball stays where it is for instant retries. N picks a random spot; P places it with a click.'
                : 'Pick a challenge below: a fixed spot, rules and target rings. Gold needs most of the rings.',
        },
        {
          kind: 'segmented', label: 'Challenge',
          options: ctx.challenges.map((_c, i) => ({ label: `${i + 1}`, value: String(i) })),
          get: () => (s.challenge ? String(s.challenge.index) : ''),
          set: (v) => ctx.startChallenge(Number(v)),
          note: () => {
            const c = ctx.challenges[s.challenge ? s.challenge.index : P.challengeIndex];
            const best = P.best[c.id];
            return `${c.name}: ${c.desc} Medals ${c.medals.bronze} / ${c.medals.silver} / ${c.medals.gold}.${best !== undefined ? ` Best ${best} ${medalIcon(c.id)}` : ''}`;
          },
        },
        {
          kind: 'button', label: 'Practice', text: 'Place ball with a click (P)',
          onClick: () => { ctx.setMode('practice'); ctx.togglePlacing(); },
        },
      ],
    },
    {
      title: 'Difficulty',
      rows: [
        {
          kind: 'segmented', label: 'Preset',
          options: DIFFICULTIES.map((d) => ({ label: ctx.presets[d].label, value: d })),
          get: () => s.difficulty,
          set: (v) => ctx.applyDifficulty(v as DifficultyName),
          note: 'Sets the keeper, wall, wind and your accuracy together. Editing any of those afterwards makes it "custom".',
        },
      ],
    },
    {
      title: 'Taker',
      rows: [
        {
          kind: 'segmented', label: 'Roster',
          options: ctx.takers.map((_t, i) => ({ label: `${i + 1}`, value: String(i) })),
          get: () => String(s.takerIndex),
          set: (v) => ctx.selectTaker(Number(v)),
          note: () => {
            const t = s.taker;
            return `${t.name}: ${t.style ?? ''}, ${t.foot === 'R' ? 'right' : 'left'}-footed. Accuracy ${t.accuracy.toFixed(2)}, curve ${t.curve.toFixed(2)}, power ${t.power.toFixed(2)}.`;
          },
        },
      ],
    },
  ];

  const post: Section[] = [
    {
      title: 'Input',
      rows: [
        { kind: 'toggle', label: 'Mouse aiming', ...client('mouseAim'), note: 'Click the pitch to capture the mouse: move = reticle, right button = contact point, left button = shoot. Esc frees it.' },
        {
          kind: 'range', label: 'Mouse sensitivity', min: SENS_MIN, max: SENS_MAX, step: 0.05,
          ...client('mouseSensitivity'), fmt: (v) => `${v.toFixed(2)}×`,
        },
      ],
    },
    {
      title: 'Audio',
      rows: [
        { kind: 'range', label: 'Master volume', min: 0, max: 1, step: 0.01, ...client('masterVolume'), fmt: (v) => `${Math.round(v * 100)}%` },
        { kind: 'range', label: 'Crowd volume', min: 0, max: 1, step: 0.01, ...client('crowdVolume'), fmt: (v) => `${Math.round(v * 100)}%` },
        { kind: 'toggle', label: 'Mute', ...client('muted') },
      ],
    },
    {
      title: 'Graphics',
      rows: [
        {
          kind: 'segmented', label: 'Quality',
          options: [{ label: 'Low', value: 'low' }, { label: 'Medium', value: 'medium' }, { label: 'High', value: 'high' }],
          get: () => P.quality, set: (v) => { P.quality = v as Quality; ctx.apply(); },
          note: 'Low: no shadows, 1× pixels. Medium: shadows, up to 1.5× pixels. High: sharp shadows, up to 2× pixels.',
        },
        {
          kind: 'segmented', label: 'Time of day',
          options: [{ label: 'Day', value: 'day' }, { label: 'Dusk', value: 'dusk' }, { label: 'Night', value: 'night' }],
          get: () => P.timeOfDay, set: (v) => { P.timeOfDay = v as TimeOfDay; ctx.apply(); },
          note: 'Dusk and night switch the stadium floodlights on.',
        },
      ],
    },
    {
      title: 'Camera',
      rows: [
        {
          kind: 'range', label: 'Camera distance', min: 0.6, max: 1.8, step: 0.05,
          ...client('cameraDistance'), fmt: (v) => `${v.toFixed(2)}×`,
          note: 'How far behind the ball the camera sits (C still toggles near / far).',
        },
      ],
    },
    {
      title: 'Replay',
      rows: [
        {
          kind: 'segmented', label: 'Replay speed',
          options: REPLAY_SPEEDS.map((v) => ({ label: `${v}×`, value: String(v) })),
          get: () => String(P.replaySpeed), set: (v) => { P.replaySpeed = Number(v); ctx.apply(); },
          note: 'Press V after a kick for an instant replay from several cameras.',
        },
        { kind: 'toggle', label: 'Auto-replay goals', ...client('autoReplay') },
      ],
    },
  ];

  const taker = (k: 'accuracy' | 'curve' | 'power'): Pick<RangeRow, 'get' | 'set'> => ({
    get: () => s.taker[k],
    set: (v) => {
      s.taker[k] = v;
      s.previewKey = '';
      onChange();
    },
  });

  const sections: Section[] = [
    ...pre,
    {
      title: 'Free kick',
      rows: [
        {
          kind: 'range', label: 'Distance to goal', min: 17, max: 40, step: 0.5,
          get: () => place.distance, set: (v) => (place.distance = v), fmt: (v) => `${v.toFixed(1)} m`,
        },
        {
          kind: 'range', label: 'Angle', min: -60, max: 60, step: 1,
          get: () => place.angle, set: (v) => (place.angle = v), fmt: (v) => `${v > 0 ? '+' : ''}${Math.round(v)}°`,
        },
        {
          kind: 'button', label: 'Ball position', text: 'Place ball here',
          onClick: () => {
            if (s.phase !== 'aim') return;
            const a = (place.angle * Math.PI) / 180;
            setSpot(s, place.distance * Math.sin(a), place.distance * Math.cos(a));
            onChange();
          },
        },
        {
          kind: 'button', label: 'Ball position', text: 'Random spot',
          onClick: () => {
            if (s.phase !== 'aim') return;
            const p = randomSpot(s);
            setSpot(s, p.x, p.z);
            place.distance = Math.hypot(p.x, p.z);
            place.angle = (Math.atan2(p.x, p.z) * 180) / Math.PI;
            onChange();
          },
        },
        {
          kind: 'segmented', label: 'Taker foot',
          options: [{ label: 'Right', value: 'R' }, { label: 'Left', value: 'L' }],
          get: () => s.foot, set: (v) => { s.foot = v as 'R' | 'L'; onChange(); },
        },
      ],
    },
    {
      title: 'Run-up',
      rows: [
        {
          kind: 'range', label: 'Approach angle', min: 0, max: s.cfg.shot.runupAngleMax, step: 1,
          get: () => s.runup.angle, set: (v) => { s.runup.angle = v; onChange(); }, fmt: (v) => `${Math.round(v)}°`,
          note: 'Straight = more power, less curl. Wide = more curl, less power. Past 45° a centre strike is no longer a knuckleball.',
        },
        {
          kind: 'range', label: 'Run-up length', min: s.cfg.shot.runupDistMin, max: s.cfg.shot.runupDistMax, step: 0.1,
          get: () => s.runup.dist, set: (v) => { s.runup.dist = v; onChange(); }, fmt: (v) => `${v.toFixed(1)} m`,
          note: 'A longer run-up adds a little power.',
        },
        {
          kind: 'button', label: 'Run-up', text: 'Reset run-up',
          onClick: () => {
            s.runup.angle = s.cfg.shot.runupAngleDefault;
            s.runup.dist = s.cfg.shot.runupDistDefault;
            onChange();
          },
        },
      ],
    },
    {
      title: 'Defensive wall',
      rows: [
        { kind: 'toggle', label: 'Wall', get: () => st.wallEnabled, set: (v) => { st.wallEnabled = v; changed(); } },
        {
          kind: 'range', label: 'Wall height', min: WALL_HEIGHT_MIN, max: WALL_HEIGHT_MAX, step: 0.01,
          get: () => st.wallHeight, set: (v) => { st.wallHeight = v; changed(); }, fmt: (v) => `${v.toFixed(2)} m`,
          note: 'Taller walls force a higher, steeper dip.',
        },
        {
          kind: 'range', label: 'Players in wall', min: 0, max: WALL_COUNT_MAX, step: 1,
          get: () => st.wallCount, set: (v) => { st.wallCount = Math.round(v); changed(); },
          fmt: (v) => (v === 0 ? 'Auto' : `${v}`),
          note: 'Auto: more players when closer and more central.',
        },
        {
          kind: 'toggle', label: 'Wall jumps on the strike', get: () => st.wallJumps, set: (v) => { st.wallJumps = v; changed(); },
          note: 'Jumpers raise the wall by about 0.4 m as the ball arrives, so go over with extra dip or round the end.',
        },
      ],
    },
    {
      title: 'Wall extras',
      rows: [
        {
          kind: 'segmented', label: 'Lying man',
          options: [{ label: 'Off', value: 'off' }, { label: 'Sometimes', value: 'sometimes' }, { label: 'Always', value: 'always' }],
          get: () => st.lyingMan, set: (v) => { st.lyingMan = v as Frequency; changed(); },
          note: 'A player lies behind the wall and stops low shots that slide under the jumpers.',
        },
        {
          kind: 'segmented', label: 'Encroachment',
          options: [{ label: 'Off', value: 'off' }, { label: 'Sometimes', value: 'sometimes' }, { label: 'Always', value: 'always' }],
          get: () => st.encroachment, set: (v) => { st.encroachment = v as Frequency; changed(); },
          note: 'A wall player creeps forward before the kick. If the shot does not score, you retake it.',
        },
      ],
    },
    {
      title: 'Goalkeeper',
      rows: [
        { kind: 'toggle', label: 'Goalkeeper present', get: () => st.keeperPresent, set: (v) => { st.keeperPresent = v; changed(); } },
        {
          kind: 'range', label: 'Keeper skill', min: 0, max: 1, step: 0.01,
          get: () => st.keeperSkill, set: (v) => { st.keeperSkill = v; changed(); }, fmt: (v) => `${Math.round(v * 100)}%`,
          note: 'Reaction time, how well he reads curl, dive speed and how hard a shot he can hold. He covers the far post, cannot reach the top corners and never accounts for a knuckleball.',
        },
        {
          kind: 'segmented', label: 'Keeper type',
          options: [
            { label: 'Balanced', value: 'balanced' },
            { label: 'Quick', value: 'quick' },
            { label: 'Tall', value: 'tall' },
            { label: 'Strong', value: 'strong' },
          ],
          get: () => st.keeperType, set: (v) => { st.keeperType = v as KeeperType; changed(); },
          note: 'Quick: fast reactions, short reach. Tall: long reach, slow to react. Strong: holds harder shots but reads curl worse.',
        },
        {
          kind: 'toggle', label: 'Rebounds', get: () => st.rebounds, set: (v) => { st.rebounds = v; changed(); },
          note: 'A parry that drops in the box becomes a follow-up shot.',
        },
      ],
    },
    {
      title: 'Conditions',
      rows: [
        { kind: 'toggle', label: 'Wind', get: () => st.windEnabled, set: (v) => { st.windEnabled = v; changed(); } },
        {
          kind: 'range', label: 'Wind speed', min: 0, max: WIND_SPEED_MAX, step: 0.5,
          get: () => st.windSpeed, set: (v) => { st.windSpeed = v; changed(); }, fmt: (v) => `${v.toFixed(1)} m/s`,
        },
        {
          kind: 'range', label: 'Wind direction', min: 0, max: 360, step: 5,
          get: () => st.windDirection, set: (v) => { st.windDirection = v; changed(); }, fmt: (v) => `${Math.round(v)}°`,
          note: '0° blows towards goal, 90° towards the right. The aim solver ignores wind, so you have to allow for it; "Show predicted path" includes it.',
        },
      ],
    },
    {
      title: 'Taker tuning',
      rows: [
        { kind: 'range', label: 'Accuracy', min: 0, max: 1, step: 0.01, ...taker('accuracy'), fmt: (v) => v.toFixed(2) },
        { kind: 'range', label: 'Curve', min: 0, max: 1.5, step: 0.01, ...taker('curve'), fmt: (v) => v.toFixed(2) },
        { kind: 'range', label: 'Power', min: 0.7, max: 1.2, step: 0.01, ...taker('power'), fmt: (v) => v.toFixed(2) },
      ],
    },
    {
      title: 'Aids',
      rows: [
        {
          kind: 'toggle', label: 'Show predicted path', get: () => s.showPath,
          set: (v) => { s.showPath = v; s.previewKey = ''; onChange(); },
          note: 'Practice aid: draws the ideal flight of the current aim.',
        },
      ],
    },
    ...post,
    {
      title: '',
      rows: [
        {
          kind: 'button', label: 'Defaults', text: 'Reset all settings',
          onClick: () => {
            Object.assign(s.settings, defaultSettings());
            s.difficulty = 'normal';
            P.difficulty = 'normal';
            applySettings(s);
            onReset();
          },
        },
      ],
    },
  ];

  const root = document.createElement('div');
  root.className = 'sheet-body';
  const syncs: (() => void)[] = [];

  const mk = <K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = '') => {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text) e.textContent = text;
    return e;
  };

  // Sections that are rarely touched start collapsed; the player's own choice is remembered.
  const COLLAPSED_BY_DEFAULT = ['Wall extras', 'Taker tuning', 'Input', 'Audio', 'Graphics', 'Camera', 'Replay', 'Aids', 'Run-up', 'Conditions'];
  for (const sec of sections) {
    const wrap = mk('section', 'sec');
    const body = mk('div', 'sec-body');
    if (sec.title) {
      const h = mk('button', 'sec-title', sec.title);
      h.type = 'button';
      const collapsed = () => P.collapsed[sec.title] ?? COLLAPSED_BY_DEFAULT.includes(sec.title);
      const apply = () => wrap.classList.toggle('collapsed', collapsed());
      h.addEventListener('click', () => {
        P.collapsed[sec.title] = !collapsed();
        apply();
        ctx.apply();
        h.blur();
      });
      apply();
      wrap.append(h);
    }
    wrap.append(body);
    root.append(wrap);
    for (const r of sec.rows) {
      const row = mk('div', 'set-row' + (r.soon ? ' soon' : ''));
      const head = mk('div', 'set-head');
      const label = mk('span', 'set-label', r.kind === 'button' ? r.label : r.label);
      head.append(label);
      if (r.soon) head.append(mk('span', 'badge', 'soon'));
      const valueEl = mk('span', 'set-value');
      row.append(head);

      if (r.kind === 'range') {
        const input = mk('input');
        input.type = 'range';
        input.min = String(r.min);
        input.max = String(r.max);
        input.step = String(r.step);
        input.disabled = !!r.soon;
        input.addEventListener('input', () => {
          r.set(Number(input.value));
          valueEl.textContent = r.fmt(r.get());
        });
        head.append(valueEl);
        row.append(input);
        syncs.push(() => {
          input.value = String(r.get());
          valueEl.textContent = r.fmt(r.get());
        });
      } else if (r.kind === 'toggle') {
        const sw = mk('label', 'switch');
        const cb = mk('input');
        cb.type = 'checkbox';
        cb.disabled = !!r.soon;
        cb.addEventListener('change', () => r.set(cb.checked));
        sw.append(cb, mk('span', 'slider'));
        head.append(sw);
        syncs.push(() => (cb.checked = r.get()));
      } else if (r.kind === 'segmented') {
        const seg = mk('div', 'seg');
        const btns = r.options.map((o) => {
          const b = mk('button', '', o.label);
          b.type = 'button';
          b.addEventListener('click', () => {
            r.set(o.value);
            syncs.forEach((f) => f());
            b.blur();
          });
          seg.append(b);
          return { b, o };
        });
        head.append(seg);
        syncs.push(() => btns.forEach(({ b, o }) => b.classList.toggle('on', r.get() === o.value)));
      } else {
        label.textContent = '';
        const b = mk('button', 'btn', r.text);
        b.type = 'button';
        b.addEventListener('click', () => {
          r.onClick();
          syncs.forEach((f) => f());
          b.blur();
        });
        row.classList.add('button-row');
        head.append(b);
      }
      if (r.note) {
        const n = mk('div', 'set-note', typeof r.note === 'string' ? r.note : r.note());
        row.append(n);
        if (typeof r.note === 'function') {
          const fn = r.note;
          syncs.push(() => (n.textContent = fn()));
        }
      }
      body.append(row);
    }
  }

  const sync = () => {
    place.distance = Math.hypot(s.spot.x, s.spot.z);
    place.angle = (Math.atan2(s.spot.x, s.spot.z) * 180) / Math.PI;
    syncs.forEach((f) => f());
  };
  sync();
  return { el: root, sync };
}
