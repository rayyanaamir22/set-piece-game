import { buildControlsPanel } from './ControlsPanel';

const ICON_CONTROLS =
  '<svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><rect x="2.5" y="6.5" width="19" height="11" rx="5.5"/><path d="M7.5 10v4M5.5 12h4"/><circle cx="15.5" cy="11" r="1" fill="currentColor"/><circle cx="18" cy="13.2" r="1" fill="currentColor"/></svg>';
const ICON_SETTINGS =
  '<svg viewBox="0 0 24 24" width="22" height="22" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.9-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.9.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.9 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.9l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.9.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.9-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.9V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></svg>';

type PanelId = 'controls' | 'settings';

interface Sheet {
  id: PanelId;
  title: string;
  button: HTMLButtonElement;
  sheet: HTMLElement;
  onOpen?: () => void;
}

/** Top-right icon buttons that open the Controls and Settings sheets. The game pauses while one is open. */
export class Menu {
  private sheets: Sheet[] = [];
  private openId: PanelId | null = null;

  constructor(
    root: HTMLElement,
    settingsBody: HTMLElement,
    private onToggle: (open: boolean) => void,
    onSettingsOpen: () => void,
  ) {
    const bar = document.createElement('div');
    bar.className = 'menu-bar';
    root.append(bar);

    const add = (id: PanelId, title: string, icon: string, body: HTMLElement, onOpen?: () => void) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'icon-btn';
      button.innerHTML = icon;
      button.title = title;
      button.setAttribute('aria-label', title);
      button.addEventListener('click', () => {
        this.toggle(id);
        button.blur();
      });
      bar.append(button);

      const sheet = document.createElement('section');
      sheet.className = 'sheet';
      sheet.hidden = true;
      const head = document.createElement('header');
      const h = document.createElement('h2');
      h.textContent = title;
      const paused = document.createElement('span');
      paused.className = 'paused';
      paused.textContent = 'game paused';
      const close = document.createElement('button');
      close.type = 'button';
      close.className = 'close';
      close.setAttribute('aria-label', 'Close');
      close.textContent = '×';
      close.addEventListener('click', () => this.close());
      head.append(h, paused, close);
      sheet.append(head, body);
      root.append(sheet);
      this.sheets.push({ id, title, button, sheet, onOpen });
    };

    add('controls', 'Controls', ICON_CONTROLS, buildControlsPanel());
    add('settings', 'Settings', ICON_SETTINGS, settingsBody, onSettingsOpen);

    window.addEventListener('keydown', (e) => {
      if (e.code === 'Escape' && this.openId) this.close();
    });
  }

  get isOpen(): boolean {
    return this.openId !== null;
  }

  toggle(id: PanelId): void {
    if (this.openId === id) this.close();
    else this.open(id);
  }

  open(id: PanelId): void {
    this.openId = id;
    for (const s of this.sheets) {
      const on = s.id === id;
      s.sheet.hidden = !on;
      s.button.classList.toggle('active', on);
      if (on) s.onOpen?.();
    }
    this.onToggle(true);
  }

  close(): void {
    if (!this.openId) return;
    this.openId = null;
    for (const s of this.sheets) {
      s.sheet.hidden = true;
      s.button.classList.remove('active');
    }
    this.onToggle(false);
  }
}
