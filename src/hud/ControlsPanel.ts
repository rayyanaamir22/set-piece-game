import { ACTIONS, ActionId, Keymap, conflicts, defaultKeymap, keyLabel, rebind } from '../input/keymap';

export interface ControlsCtx {
  getKeymap(): Keymap;
  setKeymap(k: Keymap): void;
}

const PAD: Partial<Record<ActionId, string>> = {
  aimLeft: 'Left stick', aimRight: 'Left stick', aimUp: 'Left stick', aimDown: 'Left stick',
  contactLeft: 'Right stick', contactRight: 'Right stick', contactUp: 'Right stick', contactDown: 'Right stick',
  shoot: 'B / ○', driven: 'RB / R1', low: 'LB / L1', dummy: 'L-stick click',
  runupWider: 'RT / R2', runupNarrower: 'LT / L2', runupLonger: 'D-pad ↑', runupShorter: 'D-pad ↓',
  toggleFoot: 'Y / △', newSpot: 'X / □', retry: 'Start', camera: 'Back / Select', replay: 'R-stick click',
};

const NOTES: Partial<Record<ActionId, string>> = {
  aimLeft: 'Move the target reticle: where the ball should arrive. The arrow shows the launch direction.',
  contactLeft: 'Side = curl · top = dip · bottom = lift · dead centre = knuckleball.',
  shoot: 'Hold to charge, release in the green zone. Too early or late adds error; overcharging lifts it.',
  driven: 'Faster and flatter, much less spin.',
  low: 'A skidding ground drive: slips under a jumping wall, but a lying man stops it.',
  dummy: 'Hold as you release: a decoy runs over the ball first and part of the wall jumps too early.',
  runupWider: 'Straight = power, wide = curl. Past 45° a centre strike is no longer a knuckleball.',
  newSpot: 'New random spot (free kicks / practice), or the next challenge when one is finished.',
  retry: 'Skip the wait and retake, or restart a finished challenge.',
  replay: 'Watch the last attempt from several cameras (Esc or V to leave).',
  placeBall: 'Practice mode: click the pitch to put the ball there.',
};

const MOUSE: [string, string][] = [
  ['Capture the mouse', 'Click the pitch'],
  ['Move the reticle', 'Move the mouse'],
  ['Move the contact point', 'Right button + move'],
  ['Shoot (hold, release)', 'Left button'],
  ['Free the mouse / close menu', 'Esc'],
];

const REPLAY_KEYS: [string, string][] = [
  ['Switch camera', '1 – 4'],
  ['Pause / resume', 'Space'],
  ['Slower / faster', '← / →'],
  ['Leave the replay', 'Esc or V'],
];

export interface ControlsPanel {
  el: HTMLElement;
  refresh(): void;
}

export function buildControlsPanel(ctx: ControlsCtx): ControlsPanel {
  const root = document.createElement('div');
  root.className = 'sheet-body';
  let listening: ActionId | null = null;
  let message = '';

  const mk = <K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', text = '') => {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text) e.textContent = text;
    return e;
  };

  const stopListening = () => {
    listening = null;
    window.removeEventListener('keydown', onKey, true);
  };

  function onKey(e: KeyboardEvent): void {
    if (!listening) return;
    e.preventDefault();
    e.stopPropagation();
    if (e.code === 'Escape') {
      message = '';
      stopListening();
      render();
      return;
    }
    const km = ctx.getKeymap();
    const id = listening;
    const clash = conflicts(km, e.code, id);
    ctx.setKeymap(rebind(km, id, e.code));
    message = clash.length
      ? `${keyLabel(e.code)} was ${clash.map((c) => ACTIONS.find((a) => a.id === c)!.label).join(', ')}: that action took over ${keyLabel(km[id][0])}.`
      : '';
    stopListening();
    render();
  }

  function render(): void {
    root.replaceChildren();
    const km = ctx.getKeymap();
    if (message) root.append(mk('div', 'ctl-msg', message));

    let group = '';
    for (const a of ACTIONS) {
      if (a.group !== group) {
        group = a.group;
        root.append(mk('h3', '', group));
      }
      const row = mk('div', 'ctl-row');
      const left = mk('div', 'ctl-action', a.label);
      if (NOTES[a.id]) left.append(mk('div', 'ctl-note', NOTES[a.id]!));
      const keys = mk('div', 'ctl-keys');
      km[a.id].forEach((code, i) => {
        if (i > 0) keys.append(mk('span', 'or', 'or'));
        const kbd = mk('button', 'keycap' + (i === 0 ? ' primary' : ''), listening === a.id && i === 0 ? 'press a key…' : keyLabel(code));
        kbd.type = 'button';
        if (i === 0) {
          kbd.title = 'Click to rebind';
          kbd.addEventListener('click', () => {
            stopListening();
            listening = a.id;
            message = '';
            window.addEventListener('keydown', onKey, true);
            render();
            kbd.blur();
          });
        } else kbd.disabled = true;
        if (listening === a.id && i === 0) kbd.classList.add('listening');
        keys.append(kbd);
      });
      const pad = mk('div', 'ctl-pad', PAD[a.id] ?? '');
      row.append(left, keys, pad);
      root.append(row);
    }

    root.append(mk('h3', '', 'Mouse'));
    for (const [what, how] of MOUSE) {
      const row = mk('div', 'ctl-row');
      row.append(mk('div', 'ctl-action', what), mk('div', 'ctl-keys', how));
      root.append(row);
    }
    root.append(mk('h3', '', 'During a replay'));
    for (const [what, how] of REPLAY_KEYS) {
      const row = mk('div', 'ctl-row');
      row.append(mk('div', 'ctl-action', what), mk('div', 'ctl-keys', how));
      root.append(row);
    }
    const reset = mk('button', 'btn', 'Reset keys to defaults');
    reset.type = 'button';
    reset.style.marginTop = '14px';
    reset.addEventListener('click', () => {
      stopListening();
      message = '';
      ctx.setKeymap(defaultKeymap());
      render();
      reset.blur();
    });
    root.append(reset);
  }

  render();
  return { el: root, refresh: render };
}
