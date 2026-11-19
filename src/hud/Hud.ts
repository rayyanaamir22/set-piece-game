import { dirToGoal, rightOf } from '../sim/layout';
import { windVector } from '../sim/settings';
import { runupEffect } from '../sim/ShotModel';
import { SimState } from '../sim/Sim';

const el = <K extends keyof HTMLElementTagNameMap>(tag: K, cls = '', html = ''): HTMLElementTagNameMap[K] => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (html) e.innerHTML = html;
  return e;
};

function curlText(cx: number, cy: number, knuckle: boolean): string {
  if (knuckle) return '<b>Knuckleball</b><br>unpredictable late movement';
  const parts: string[] = [];
  if (Math.abs(cx) > 0.12) parts.push(`curls <b>${cx > 0 ? 'left' : 'right'}</b> ${Math.round(Math.abs(cx) * 100)}%`);
  if (Math.abs(cy) > 0.12) parts.push(`<b>${cy > 0 ? 'dips' : 'lifts'}</b> ${Math.round(Math.abs(cy) * 100)}%`);
  return parts.length ? parts.join('<br>') : '<b>Straight</b><br>centre contact';
}

/** Plain DOM overlay: distance, power meter, contact widget, result toast. */
export class Hud {
  private distance: HTMLElement;
  private info: HTMLElement;
  private stats: HTMLElement;
  private runupRow: HTMLElement;
  private windRow: HTMLElement;
  private windArrow: HTMLElement;
  private windText: HTMLElement;
  private fill: HTMLElement;
  private meterLabel: HTMLElement;
  private meterWrap: HTMLElement;
  private widget: HTMLCanvasElement;
  private curl: HTMLElement;
  private toast: HTMLElement;
  private lastToast = '';

  constructor(root: HTMLElement, sweetMin: number, sweetMax: number) {
    const tl = el('div', 'panel topleft');
    this.distance = el('div', 'big');
    this.info = el('div', 'row');
    this.stats = el('div', 'row');
    this.runupRow = el('div', 'row');
    this.windRow = el('div', 'row wind');
    this.windArrow = el(
      'span',
      'wind-arrow',
      '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor"><path d="M12 2 19 13h-5v9h-4v-9H5z"/></svg>',
    );
    this.windText = el('span');
    this.windRow.append(this.windArrow, this.windText);
    tl.append(this.distance, this.info, this.stats, this.runupRow, this.windRow);

    this.meterWrap = el('div', 'meter-wrap');
    const meter = el('div', 'meter');
    const sweet = el('div', 'sweet');
    sweet.style.left = `${sweetMin * 100}%`;
    sweet.style.width = `${(sweetMax - sweetMin) * 100}%`;
    this.fill = el('div', 'fill');
    meter.append(this.fill, sweet);
    this.meterLabel = el('div', 'meter-label');
    this.meterWrap.append(meter, this.meterLabel);

    const contact = el('div', 'panel contact');
    this.widget = el('canvas');
    this.widget.width = this.widget.height = 240;
    this.curl = el('div', 'txt');
    contact.append(this.widget, this.curl);

    this.toast = el('div', 'toast');
    root.append(tl, this.meterWrap, contact, this.toast);
  }

  private drawWidget(cx: number, cy: number, knuckle: boolean): void {
    const g = this.widget.getContext('2d')!;
    const S = 240;
    const c = S / 2;
    const R = 100;
    g.clearRect(0, 0, S, S);
    const grad = g.createRadialGradient(c - 25, c - 25, 10, c, c, R);
    grad.addColorStop(0, '#ffffff');
    grad.addColorStop(1, '#bdbdbd');
    g.fillStyle = grad;
    g.beginPath();
    g.arc(c, c, R, 0, Math.PI * 2);
    g.fill();
    g.strokeStyle = 'rgba(0,0,0,0.35)';
    g.lineWidth = 2;
    g.beginPath();
    g.moveTo(c - R, c);
    g.lineTo(c + R, c);
    g.moveTo(c, c - R);
    g.lineTo(c, c + R);
    g.stroke();
    g.setLineDash([5, 5]);
    g.beginPath();
    g.arc(c, c, R * 0.18, 0, Math.PI * 2);
    g.stroke();
    g.setLineDash([]);
    g.fillStyle = knuckle ? '#ff5a5a' : '#2a7bff';
    g.beginPath();
    g.arc(c + cx * R, c - cy * R, 12, 0, Math.PI * 2);
    g.fill();
    g.strokeStyle = '#fff';
    g.lineWidth = 3;
    g.stroke();
  }

  update(s: SimState): void {
    const dist = Math.hypot(s.spot.x, s.spot.z);
    this.distance.textContent = `${Math.round(dist)} m`;
    this.info.innerHTML =
      `<b>${s.taker.name}</b> · ${s.foot === 'R' ? 'right' : 'left'}-footed` + (s.followUp ? ' · <b class="rebound">Rebound!</b>' : '');

    const fx = runupEffect(s.runup, s.cfg.shot);
    const pct = (m: number) => `${m >= 1 ? '+' : '−'}${Math.round(Math.abs(m - 1) * 100)}%`;
    this.runupRow.innerHTML =
      `Run-up <b>${Math.round(s.runup.angle)}°</b> · <b>${s.runup.dist.toFixed(1)} m</b> ` +
      `<span class="dim">power ${pct(fx.power)}, curl ${pct(fx.curl)}${fx.knuckleOk ? '' : ', no knuckle'}</span>`;

    const w = windVector(s.settings);
    const wspeed = Math.hypot(w.x, w.z);
    this.windRow.style.display = wspeed > 0 ? '' : 'none';
    if (wspeed > 0) {
      // Arrow relative to the player's view: up = blowing towards goal.
      const f = dirToGoal(s.spot);
      const r = rightOf(f);
      const up = w.x * f.x + w.z * f.z;
      const right = w.x * r.x + w.z * r.z;
      this.windArrow.style.transform = `rotate(${(Math.atan2(right, up) * 180) / Math.PI}deg)`;
      const kind = Math.abs(up) > Math.abs(right) * 1.7 ? (up > 0 ? 'tailwind' : 'headwind') : Math.abs(right) > Math.abs(up) * 1.7 ? 'crosswind' : 'quartering';
      this.windText.innerHTML = `Wind <b>${wspeed.toFixed(1)} m/s</b> <span class="dim">${kind}</span>`;
    }
    this.stats.innerHTML = `Goals <b>${s.stats.goals}</b> / ${s.stats.attempts}`;

    const charging = s.phase === 'charge';
    const showMeter = s.phase === 'aim' || charging;
    this.meterWrap.style.opacity = showMeter ? '1' : '0';
    this.fill.style.width = `${(charging ? s.meter : 0) * 100}%`;
    const sp = s.cfg.shot;
    this.meterLabel.textContent = charging
      ? s.meter > sp.sweetMax
        ? 'Overpowered!'
        : s.meter >= sp.sweetMin
          ? 'NOW!'
          : 'Charging…'
      : 'Hold SPACE to charge';

    const knuckle = !!s.preview?.knuckle;
    this.drawWidget(s.contact.x, s.contact.y, knuckle);
    const mods = [s.driven ? 'driven' : '', s.low ? 'low' : ''].filter(Boolean).join(' + ');
    this.curl.innerHTML = curlText(s.contact.x, s.contact.y, knuckle) + (mods ? `<br><i>${mods}</i>` : '');

    this.updateToast(s);
  }

  private updateToast(s: SimState): void {
    if (s.phase !== 'result' || !s.result) {
      if (this.lastToast) {
        this.toast.classList.remove('show');
        this.lastToast = '';
      }
      return;
    }
    const { type, woodwork, wall, keeper } = s.result;
    const wood = woodwork === 'post' ? 'the post' : woodwork === 'bar' ? 'the crossbar' : '';
    let title: string;
    let sub = '';
    let cls: string;
    if (type === 'goal') {
      title = 'GOAL!';
      sub = woodwork
        ? `Off ${wood} and in`
        : keeper
          ? 'Through the keeper\'s hands'
          : wall
            ? 'Deflected off the wall'
            : '';
      cls = 'goal';
    } else if (type === 'retake') {
      title = 'Retake!';
      sub = 'The wall encroached';
      cls = 'wood';
    } else if (type === 'saved') {
      title = 'Saved!';
      sub = keeper === 'hold' ? 'The keeper holds it' : s.result.rebound ? 'Spilled in the box: rebound!' : 'Pushed away';
      cls = s.result.rebound ? 'wood' : 'bad';
    } else if (woodwork) {
      title = woodwork === 'bar' ? 'Off the bar!' : 'Off the post!';
      sub = 'So close';
      cls = 'wood';
    } else if (type === 'miss' && wall) {
      title = 'Blocked!';
      sub = 'Hit the wall';
      cls = 'bad';
    } else if (type === 'over') {
      title = 'Over the bar';
      sub = wall ? 'Deflected off the wall' : '';
      cls = 'bad';
    } else if (type === 'wide') {
      title = 'Wide';
      sub = wall ? 'Deflected off the wall' : '';
      cls = 'bad';
    } else {
      title = 'Short';
      cls = 'bad';
    }
    const key = `${s.stats.attempts}:${title}`;
    if (key === this.lastToast) return;
    this.lastToast = key;
    this.toast.className = `toast ${cls} show`;
    this.toast.innerHTML = `${title}${sub ? `<small>${sub}</small>` : ''}`;
  }
}
