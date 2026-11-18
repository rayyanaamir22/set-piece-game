interface Row {
  action: string;
  keys: string[][]; // each inner array is a group of keycaps ("or" between groups)
  pad: string;
  note?: string;
}

interface Group {
  title: string;
  rows: Row[];
}

const GROUPS: Group[] = [
  {
    title: 'Aim',
    rows: [
      { action: 'Move target reticle', keys: [['W', 'A', 'S', 'D'], ['←', '↑', '↓', '→']], pad: 'Left stick', note: 'Where the ball should arrive. The arrow shows the launch direction.' },
    ],
  },
  {
    title: 'Contact point',
    rows: [
      { action: 'Move contact point', keys: [['I', 'J', 'K', 'L']], pad: 'Right stick', note: 'Side = curl · top = dip · bottom = lift · dead centre = knuckleball.' },
      { action: 'Re-centre contact', keys: [['O']], pad: 'Release stick' },
    ],
  },
  {
    title: 'Shoot',
    rows: [
      { action: 'Hold to charge, release to strike', keys: [['Space']], pad: 'B / ○', note: 'Release inside the green zone for an accurate shot. Too early or late adds error; overcharging lifts it.' },
    ],
  },
  {
    title: 'Shot type',
    rows: [
      { action: 'Driven shot (hold)', keys: [['Shift']], pad: 'RB / R1', note: 'Faster and flatter, much less spin.' },
      { action: 'Low shot (hold)', keys: [['Ctrl'], ['Z']], pad: 'LB / L1', note: 'Keeps the target near the ground.' },
    ],
  },
  {
    title: 'Free kick',
    rows: [
      { action: 'Switch footedness', keys: [['F']], pad: 'Y / △' },
      { action: 'New random spot', keys: [['N']], pad: 'X / □' },
      { action: 'Retry same spot (after a shot)', keys: [['R']], pad: 'Start' },
    ],
  },
  {
    title: 'View',
    rows: [
      { action: 'Near / far camera', keys: [['C']], pad: 'Back / Select' },
      { action: 'Close menu', keys: [['Esc']], pad: '' },
    ],
  },
];

export function buildControlsPanel(): HTMLElement {
  const root = document.createElement('div');
  root.className = 'sheet-body';
  for (const g of GROUPS) {
    const h = document.createElement('h3');
    h.textContent = g.title;
    root.append(h);
    for (const r of g.rows) {
      const row = document.createElement('div');
      row.className = 'ctl-row';
      const left = document.createElement('div');
      left.className = 'ctl-action';
      left.textContent = r.action;
      if (r.note) {
        const n = document.createElement('div');
        n.className = 'ctl-note';
        n.textContent = r.note;
        left.append(n);
      }
      const keys = document.createElement('div');
      keys.className = 'ctl-keys';
      r.keys.forEach((grp, i) => {
        if (i > 0) keys.append(Object.assign(document.createElement('span'), { className: 'or', textContent: 'or' }));
        const wrap = document.createElement('span');
        wrap.className = 'caps';
        for (const k of grp) wrap.append(Object.assign(document.createElement('kbd'), { textContent: k }));
        keys.append(wrap);
      });
      const pad = document.createElement('div');
      pad.className = 'ctl-pad';
      pad.textContent = r.pad;
      row.append(left, keys, pad);
      root.append(row);
    }
  }
  return root;
}
