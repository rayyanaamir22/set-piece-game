import GUI from 'lil-gui';
import { SimState, setSpot } from '../sim/Sim';

/** lil-gui tuning panel (press ` to hide/show). */
export function createDebugPanel(s: SimState): GUI {
  const gui = new GUI({ title: 'Debug' });
  const spot = { distance: Math.hypot(s.spot.x, s.spot.z), angle: (Math.atan2(s.spot.x, s.spot.z) * 180) / Math.PI };

  const f = gui.addFolder('Ball spot');
  f.add(spot, 'distance', 17, 40, 0.5);
  f.add(spot, 'angle', -60, 60, 1);
  f.add(
    {
      place: () => {
        if (s.phase !== 'aim') return;
        const a = (spot.angle * Math.PI) / 180;
        setSpot(s, spot.distance * Math.sin(a), spot.distance * Math.cos(a));
      },
    },
    'place',
  ).name('Place ball');

  const g = gui.addFolder('Aim helpers');
  g.add(s, 'showPath').name('Show predicted path').onChange(() => (s.previewKey = ''));
  g.add(s, 'foot', ['R', 'L']);

  const t = gui.addFolder('Taker');
  t.add(s.taker, 'accuracy', 0, 1, 0.01);
  t.add(s.taker, 'curve', 0, 1.5, 0.01);
  t.add(s.taker, 'power', 0.7, 1.2, 0.01);

  gui.close();
  let hidden = false;
  window.addEventListener('keydown', (e) => {
    if (e.key !== '`') return;
    hidden = !hidden;
    if (hidden) gui.hide();
    else gui.show();
  });
  return gui;
}
