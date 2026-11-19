# Set Piece Game

A free-kick shooting game for the browser, built with Three.js. Aim a reticle at the goal, pick a contact point on the ball for curl, dip or a knuckleball, then time the power meter to beat a jumping wall and a goalkeeper.

![Set Piece gameplay demo](docs/demo.gif)

## What's in it

- **Shooting:** aim a reticle on the goal, pick a contact point on the ball for curl, dip or a knuckleball, shape the run-up, and time a power meter.
- **Defenders:** a wall that jumps (and can be fooled by a dummy runner), a lying man, creeping players that force retakes, and a goalkeeper with different types and skill levels.
- **Modes:** free kicks, practice (instant retries, click to place the ball) and five scored skill challenges with medals.
- **Conditions:** wind, day / dusk / night with floodlights, difficulty presets and a roster of six takers.
- **Presentation:** instant replays from several cameras, synthesised sound, vanishing spray.
- **Controls:** keyboard, mouse (click the pitch to aim) or gamepad, with every key remappable.

## Setup

Requires [Node.js](https://nodejs.org) 20 or newer.

```bash
npm install
```

## Launch

```bash
npm run dev
```

Open the URL Vite prints (usually http://localhost:5173). Click the gamepad icon in the top-right corner for the controls, and the gear icon for settings such as wall height, wall jumping and goalkeeper skill.

Other commands:

```bash
npm test         # unit tests for the physics, shot model, wall and keeper
npm run build    # type-check and produce a production build in dist/
npm run preview  # serve the production build locally
npm run golden   # regenerate the golden fixtures the native port must match
npm run contract # regenerate docs/porting-contract.md from the simulation source
```

## License

[MIT](LICENSE)
