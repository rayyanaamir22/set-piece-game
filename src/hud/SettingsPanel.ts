import {
  SimState,
  applySettings,
  randomSpot,
  setSpot,
} from '../sim/Sim';
import {
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
  note?: string;
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

export function buildSettingsPanel(
  s: SimState,
  onChange: () => void,
  onReset: () => void,
): SettingsPanel {
  // Ball placement is edited locally and applied with a button.
  const place = {
    distance: Math.hypot(s.spot.x, s.spot.z),
    angle: (Math.atan2(s.spot.x, s.spot.z) * 180) / Math.PI,
  };
  const st = s.settings;
  const changed = () => {
    applySettings(s);
    onChange();
  };
  const taker = (k: 'accuracy' | 'curve' | 'power'): Pick<RangeRow, 'get' | 'set'> => ({
    get: () => s.taker[k],
    set: (v) => {
      s.taker[k] = v;
      s.previewKey = '';
      onChange();
    },
  });

  const sections: Section[] = [
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
        { kind: 'toggle', label: 'Wall jumps on the strike', soon: true, get: () => st.wallJumps, set: (v) => { st.wallJumps = v; changed(); } },
      ],
    },
    {
      title: 'Goalkeeper',
      rows: [
        { kind: 'toggle', label: 'Goalkeeper present', soon: true, get: () => st.keeperPresent, set: (v) => { st.keeperPresent = v; changed(); } },
      ],
    },
    {
      title: 'Conditions',
      rows: [
        { kind: 'toggle', label: 'Wind', soon: true, get: () => st.windEnabled, set: (v) => { st.windEnabled = v; changed(); } },
        {
          kind: 'range', label: 'Wind speed', soon: true, min: 0, max: WIND_SPEED_MAX, step: 0.5,
          get: () => st.windSpeed, set: (v) => { st.windSpeed = v; changed(); }, fmt: (v) => `${v.toFixed(1)} m/s`,
        },
        {
          kind: 'range', label: 'Wind direction', soon: true, min: 0, max: 360, step: 5,
          get: () => st.windDirection, set: (v) => { st.windDirection = v; changed(); }, fmt: (v) => `${Math.round(v)}°`,
          note: '0° blows towards goal, 90° towards the right.',
        },
      ],
    },
    {
      title: 'Taker',
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
    {
      title: '',
      rows: [
        {
          kind: 'button', label: 'Defaults', text: 'Reset all settings',
          onClick: () => {
            Object.assign(s.settings, defaultSettings());
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

  for (const sec of sections) {
    if (sec.title) root.append(mk('h3', '', sec.title));
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
      if (r.note) row.append(mk('div', 'set-note', r.note));
      root.append(row);
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
