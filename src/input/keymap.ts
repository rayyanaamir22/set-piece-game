export type ActionId =
  | 'aimLeft' | 'aimRight' | 'aimUp' | 'aimDown'
  | 'contactLeft' | 'contactRight' | 'contactUp' | 'contactDown' | 'recenter'
  | 'shoot' | 'driven' | 'low' | 'dummy'
  | 'runupWider' | 'runupNarrower' | 'runupLonger' | 'runupShorter'
  | 'toggleFoot' | 'newSpot' | 'retry' | 'camera' | 'replay' | 'placeBall';

export interface ActionDef {
  id: ActionId;
  label: string;
  group: string;
  /** KeyboardEvent.code values; the first is the "primary" one a rebind replaces. */
  defaults: string[];
}

export const ACTIONS: ActionDef[] = [
  { id: 'aimLeft', label: 'Aim left', group: 'Aim', defaults: ['KeyA', 'ArrowLeft'] },
  { id: 'aimRight', label: 'Aim right', group: 'Aim', defaults: ['KeyD', 'ArrowRight'] },
  { id: 'aimUp', label: 'Aim up', group: 'Aim', defaults: ['KeyW', 'ArrowUp'] },
  { id: 'aimDown', label: 'Aim down', group: 'Aim', defaults: ['KeyS', 'ArrowDown'] },
  { id: 'contactLeft', label: 'Contact left', group: 'Contact point', defaults: ['KeyJ'] },
  { id: 'contactRight', label: 'Contact right', group: 'Contact point', defaults: ['KeyL'] },
  { id: 'contactUp', label: 'Contact up', group: 'Contact point', defaults: ['KeyI'] },
  { id: 'contactDown', label: 'Contact down', group: 'Contact point', defaults: ['KeyK'] },
  { id: 'recenter', label: 'Re-centre contact', group: 'Contact point', defaults: ['KeyO'] },
  { id: 'shoot', label: 'Shoot (hold, release)', group: 'Shoot', defaults: ['Space'] },
  { id: 'driven', label: 'Driven shot (hold)', group: 'Shot type', defaults: ['ShiftLeft', 'ShiftRight'] },
  { id: 'low', label: 'Low drive (hold)', group: 'Shot type', defaults: ['ControlLeft', 'KeyZ'] },
  { id: 'dummy', label: 'Dummy runner (hold at release)', group: 'Shot type', defaults: ['KeyX'] },
  { id: 'runupWider', label: 'Run-up wider', group: 'Run-up', defaults: ['KeyE'] },
  { id: 'runupNarrower', label: 'Run-up straighter', group: 'Run-up', defaults: ['KeyQ'] },
  { id: 'runupLonger', label: 'Run-up longer', group: 'Run-up', defaults: ['BracketRight'] },
  { id: 'runupShorter', label: 'Run-up shorter', group: 'Run-up', defaults: ['BracketLeft'] },
  { id: 'toggleFoot', label: 'Switch foot', group: 'Free kick', defaults: ['KeyF'] },
  { id: 'newSpot', label: 'New spot / next', group: 'Free kick', defaults: ['KeyN'] },
  { id: 'retry', label: 'Retry / restart', group: 'Free kick', defaults: ['KeyR'] },
  { id: 'placeBall', label: 'Place ball (practice)', group: 'Free kick', defaults: ['KeyP'] },
  { id: 'camera', label: 'Near / far camera', group: 'View', defaults: ['KeyC'] },
  { id: 'replay', label: 'Instant replay', group: 'View', defaults: ['KeyV'] },
];

export type Keymap = Record<ActionId, string[]>;

/** Keys that can never be bound: Esc always closes menus / frees the mouse. */
export const RESERVED_KEYS = ['Escape'];

export function defaultKeymap(): Keymap {
  return Object.fromEntries(ACTIONS.map((a) => [a.id, [...a.defaults]])) as Keymap;
}

/** Accepts anything (e.g. from storage) and returns a complete, valid keymap. */
export function sanitizeKeymap(raw: unknown): Keymap {
  const out = defaultKeymap();
  if (!raw || typeof raw !== 'object') return out;
  for (const a of ACTIONS) {
    const v = (raw as Record<string, unknown>)[a.id];
    if (Array.isArray(v) && v.length > 0 && v.every((c) => typeof c === 'string' && c.length > 0 && !RESERVED_KEYS.includes(c))) {
      out[a.id] = [...new Set(v as string[])];
    }
  }
  return out;
}

/** Which actions currently use `code` (optionally ignoring one). */
export function conflicts(map: Keymap, code: string, except?: ActionId): ActionId[] {
  return ACTIONS.filter((a) => a.id !== except && map[a.id].includes(code)).map((a) => a.id);
}

/**
 * Make `code` the primary key of `id`. Any other action that used the key loses it;
 * if that leaves it with no key at all, it takes over the key `id` just gave up (a swap).
 */
export function rebind(map: Keymap, id: ActionId, code: string): Keymap {
  if (RESERVED_KEYS.includes(code)) return map;
  const given = map[id][0];
  const out = {} as Keymap;
  for (const a of ACTIONS) {
    let keys = map[a.id].filter((c) => a.id === id || c !== code);
    if (a.id === id) keys = [code, ...keys.filter((c) => c !== code).slice(1)];
    if (keys.length === 0) keys = [given];
    out[a.id] = keys;
  }
  return out;
}

export function isHeld(map: Keymap, down: ReadonlySet<string>, id: ActionId): boolean {
  return map[id].some((c) => down.has(c));
}

const SPECIAL: Record<string, string> = {
  Space: 'Space', ShiftLeft: 'Shift', ShiftRight: 'Shift', ControlLeft: 'Ctrl', ControlRight: 'Ctrl', AltLeft: 'Alt', AltRight: 'Alt',
  ArrowLeft: '←', ArrowRight: '→', ArrowUp: '↑', ArrowDown: '↓', BracketLeft: '[', BracketRight: ']', Enter: 'Enter',
  Tab: 'Tab', Backspace: '⌫', Comma: ',', Period: '.', Slash: '/', Semicolon: ';', Quote: "'", Minus: '-', Equal: '=', Backquote: '`', Backslash: '\\',
};

export function keyLabel(code: string): string {
  if (SPECIAL[code]) return SPECIAL[code];
  if (/^Key[A-Z]$/.test(code)) return code.slice(3);
  if (/^Digit\d$/.test(code)) return code.slice(5);
  if (/^Numpad/.test(code)) return `Num ${code.slice(6)}`;
  return code;
}
