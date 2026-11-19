import { Cue, Volumes, cueGain, CROWD_CUES } from './cues';

type Bus = GainNode;

/**
 * Everything is synthesised with Web Audio (no audio files): a looping crowd
 * murmur whose level follows how exciting the moment is, plus short one-shot
 * sounds for the strike, woodwork, net, keeper and the crowd's reactions.
 * The AudioContext is created lazily on the first user gesture (browser autoplay rules).
 */
export class GameAudio {
  private ctx: AudioContext | null = null;
  private master!: GainNode;
  private fx!: Bus;
  private crowd!: Bus;
  private crowdLoop: GainNode | null = null;
  private noise: AudioBuffer | null = null;
  vol: Volumes = { master: 0.7, crowd: 0.7, muted: false };
  private excitement = 0.12;

  /** Call from a user gesture (key / click). Safe to call repeatedly. */
  resume(): void {
    try {
      if (!this.ctx) this.init();
      void this.ctx?.resume();
    } catch {
      /* no audio available: the game just stays silent */
    }
  }

  get ready(): boolean {
    return !!this.ctx && this.ctx.state === 'running';
  }

  setVolumes(v: Volumes): void {
    this.vol = v;
    if (!this.ctx) return;
    const t = this.ctx.currentTime;
    this.master.gain.setTargetAtTime(v.muted ? 0 : v.master, t, 0.03);
    this.crowd.gain.setTargetAtTime(v.crowd, t, 0.05);
  }

  /** 0..1: how worked up the crowd is. */
  setExcitement(x: number): void {
    this.excitement = x;
    if (this.ctx && this.crowdLoop) {
      this.crowdLoop.gain.setTargetAtTime(0.05 + 0.3 * x, this.ctx.currentTime, 0.25);
    }
  }

  play(cue: Cue): void {
    if (!this.ctx || this.ctx.state !== 'running') return;
    const g = cueGain(cue, this.vol);
    if (g <= 0.001) return;
    // Level inside the bus: the master/crowd sliders are already applied by the buses,
    // so use the raw cue gain here.
    const level = cue.gain;
    const bus = CROWD_CUES.includes(cue.name) ? this.crowd : this.fx;
    switch (cue.name) {
      case 'strike':
        this.tone(bus, 'sine', 120, 45, 0.16, level * 0.9, 0.003);
        this.noiseBurst(bus, 'lowpass', 1800, 600, 0.05, level * 0.5, 0.001);
        break;
      case 'bounce':
        this.tone(bus, 'sine', 140, 70, 0.1, level * 0.6, 0.002);
        break;
      case 'post':
        this.ring(bus, [1250, 2080, 3010], 0.7, level * 0.55);
        this.noiseBurst(bus, 'bandpass', 2500, 2500, 0.03, level * 0.3, 0.001);
        break;
      case 'bar':
        this.ring(bus, [820, 1480, 2250], 0.8, level * 0.55);
        this.noiseBurst(bus, 'bandpass', 1800, 1800, 0.03, level * 0.3, 0.001);
        break;
      case 'net':
        this.noiseBurst(bus, 'bandpass', 900, 350, 0.4, level * 0.5, 0.01);
        break;
      case 'wall':
        this.noiseBurst(bus, 'lowpass', 700, 300, 0.12, level * 0.7, 0.002);
        this.tone(bus, 'sine', 90, 55, 0.12, level * 0.5, 0.002);
        break;
      case 'keeperCatch':
        this.noiseBurst(bus, 'bandpass', 1100, 700, 0.09, level * 0.7, 0.002);
        break;
      case 'keeperParry':
        this.noiseBurst(bus, 'bandpass', 900, 500, 0.12, level * 0.6, 0.002);
        this.tone(bus, 'sine', 110, 60, 0.1, level * 0.4, 0.002);
        break;
      case 'whistle':
        this.whistle(bus, level * 0.35);
        break;
      case 'roar':
        this.noiseBurst(bus, 'bandpass', 500, 1300, 2.8, level * 0.9, 0.35);
        this.noiseBurst(bus, 'highpass', 1600, 1600, 1.8, level * 0.25, 0.4);
        break;
      case 'groan':
        this.noiseBurst(bus, 'lowpass', 520, 220, 1.3, level * 0.8, 0.12);
        break;
      case 'gasp':
        this.noiseBurst(bus, 'highpass', 1400, 2400, 0.55, level * 0.6, 0.18);
        break;
      case 'ooh':
        this.noiseBurst(bus, 'bandpass', 420, 640, 1.1, level * 0.8, 0.18);
        break;
    }
  }

  // ------------------------------------------------------------ internals

  private init(): void {
    const Ctor: typeof AudioContext | undefined =
      window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!Ctor) return;
    const ctx = new Ctor();
    this.ctx = ctx;
    this.master = ctx.createGain();
    this.master.gain.value = this.vol.muted ? 0 : this.vol.master;
    this.master.connect(ctx.destination);
    this.fx = ctx.createGain();
    this.fx.connect(this.master);
    this.crowd = ctx.createGain();
    this.crowd.gain.value = this.vol.crowd;
    this.crowd.connect(this.master);

    // 2 s of white noise reused by everything.
    const len = ctx.sampleRate * 2;
    const buf = ctx.createBuffer(1, len, ctx.sampleRate);
    const d = buf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    this.noise = buf;
    this.startCrowd();
  }

  private startCrowd(): void {
    const ctx = this.ctx!;
    const src = ctx.createBufferSource();
    src.buffer = this.noise;
    src.loop = true;
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = 520;
    bp.Q.value = 0.6;
    const lfo = ctx.createOscillator();
    lfo.frequency.value = 0.22;
    const lfoGain = ctx.createGain();
    lfoGain.gain.value = 0.02;
    const g = ctx.createGain();
    g.gain.value = 0.05 + 0.3 * this.excitement;
    lfo.connect(lfoGain).connect(g.gain);
    src.connect(bp).connect(g).connect(this.crowd);
    src.start();
    lfo.start();
    this.crowdLoop = g;
  }

  private env(g: GainNode, peak: number, attack: number, dur: number): void {
    const t = this.ctx!.currentTime;
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(Math.max(0.0002, peak), t + attack);
    g.gain.exponentialRampToValueAtTime(0.0001, t + attack + dur);
  }

  private tone(bus: Bus, type: OscillatorType, f0: number, f1: number, dur: number, peak: number, attack: number): void {
    const ctx = this.ctx!;
    const o = ctx.createOscillator();
    o.type = type;
    o.frequency.setValueAtTime(f0, ctx.currentTime);
    o.frequency.exponentialRampToValueAtTime(Math.max(20, f1), ctx.currentTime + dur);
    const g = ctx.createGain();
    this.env(g, peak, attack, dur);
    o.connect(g).connect(bus);
    o.start();
    o.stop(ctx.currentTime + attack + dur + 0.05);
  }

  private noiseBurst(bus: Bus, type: BiquadFilterType, f0: number, f1: number, dur: number, peak: number, attack: number): void {
    const ctx = this.ctx!;
    const src = ctx.createBufferSource();
    src.buffer = this.noise;
    src.loop = true;
    const f = ctx.createBiquadFilter();
    f.type = type;
    f.frequency.setValueAtTime(f0, ctx.currentTime);
    f.frequency.exponentialRampToValueAtTime(Math.max(40, f1), ctx.currentTime + attack + dur);
    f.Q.value = type === 'bandpass' ? 0.9 : 0.5;
    const g = ctx.createGain();
    this.env(g, peak, attack, dur);
    src.connect(f).connect(g).connect(bus);
    src.start(ctx.currentTime, Math.random());
    src.stop(ctx.currentTime + attack + dur + 0.05);
  }

  /** Metallic ring: a few inharmonic partials with a fast attack and a long decay. */
  private ring(bus: Bus, freqs: number[], dur: number, peak: number): void {
    freqs.forEach((f, i) => this.tone(bus, 'sine', f, f * 0.995, dur * (1 - i * 0.2), peak / (1 + i * 0.7), 0.002));
  }

  /** Referee's pea whistle: a high tone with a fast trill. */
  private whistle(bus: Bus, peak: number): void {
    const ctx = this.ctx!;
    const o = ctx.createOscillator();
    o.type = 'sine';
    o.frequency.value = 2850;
    const trill = ctx.createOscillator();
    trill.frequency.value = 38;
    const depth = ctx.createGain();
    depth.gain.value = 90;
    trill.connect(depth).connect(o.frequency);
    const g = ctx.createGain();
    const t = ctx.currentTime;
    g.gain.setValueAtTime(0.0001, t);
    g.gain.linearRampToValueAtTime(peak, t + 0.03);
    g.gain.setValueAtTime(peak, t + 0.34);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.45);
    o.connect(g).connect(bus);
    o.start();
    trill.start();
    o.stop(t + 0.5);
    trill.stop(t + 0.5);
  }
}
