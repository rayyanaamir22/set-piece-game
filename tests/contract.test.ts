import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

/**
 * Generates docs/porting-contract.md from the TypeScript sources, so the document the native (C++)
 * port is built against can never silently drift from the simulation it describes.
 * Regenerate with `npm run contract`; this test fails when the file is stale.
 */
const ROOT = join(__dirname, '..');
const OUT = join(ROOT, 'docs', 'porting-contract.md');
const read = (p: string) => readFileSync(join(ROOT, p), 'utf8');

/**
 * The declaration starting at the first match of `start`. Top-level code in this repo is formatted so that a
 * declaration ends at the first later line that begins at column 0 with a closer (`}`, `)`, `]`), or, for a
 * one-liner or a multi-line type union, at the line that ends with `;` once its braces balance.
 */
export function extractBlock(src: string, start: RegExp): string {
  const m = start.exec(src);
  if (!m) throw new Error(`not found: ${start}`);
  const lines = src.slice(m.index).split('\n');
  const count = (s: string, o: string, c: string) => s.split(o).length - s.split(c).length;
  let depth = 0;
  for (let i = 0; i < lines.length; i++) {
    depth += count(lines[i], '{', '}') + count(lines[i], '(', ')') + count(lines[i], '[', ']');
    if (/^export type /.test(lines[0]) || i === 0) {
      if (depth <= 0 && /;\s*$/.test(lines[i])) return lines.slice(0, i + 1).join('\n');
    }
    if (i > 0 && /^[})\]]/.test(lines[i])) return lines.slice(0, i + 1).join('\n');
  }
  return lines.join('\n');
}

const iface = (file: string, ...names: string[]) =>
  names.map((n) => extractBlock(read(file), new RegExp(`^export (interface|type) ${n}\\b`, 'm'))).join('\n\n');
const fn = (file: string, ...names: string[]) =>
  names.map((n) => extractBlock(read(file), new RegExp(`^(export )?(function|const) ${n}\\b`, 'm'))).join('\n\n');

const code = (s: string) => '```ts\n' + s + '\n```';

/** `export function name(args): ret` signatures of a module, as an inventory. */
function signatures(file: string): string {
  return read(file)
    .split('\n')
    .filter((l) => /^export (function|const) \w+/.test(l))
    .map((l) => l.replace(/\s*\{\s*$/, '').replace(/\s*=>\s*\{?\s*$/, ' => …'))
    .join('\n');
}

export function buildContract(): string {
  const sections: string[] = [];
  const add = (s: string) => sections.push(s.trim());

  add(`# Porting contract: the Set Piece simulation

> **Generated** by \`npm run contract\` from the TypeScript sources in \`src/sim/\`. Do not edit by hand:
> \`npm test\` fails when this file is out of date. The TypeScript code is the reference implementation;
> the native C++ build must reproduce it. The proof is \`tests/golden/*.json\` (see "Verification").

## 1. What is being ported

Everything under \`src/sim/\` and nothing else. The simulation has no dependency on rendering, the DOM,
input devices, audio or time: it is a pure function of \`(state, input, dt)\`. Phase B reads and renders
that state, and never writes to it.

| Module | Role |
|---|---|
| \`math/vec3.ts\`, \`math/rng.ts\` | vector maths; the seeded random generator |
| \`constants.ts\` | pitch/goal geometry |
| \`BallPhysics.ts\` | drag, Magnus, wind, knuckle wobble, RK4 step, ground contact |
| \`Collisions.ts\` | posts/bar, net, wall players, lying man, keeper capsules |
| \`Prediction.ts\` | ball look-ahead used by the keeper and the wall |
| \`ShotModel.ts\` | aim + contact point + power + run-up -> launch velocity and spin (shooting-method solver) |
| \`Wall.ts\` | wall layout, jump, encroachment, lying man |
| \`Keeper.ts\` | keeper AI: read, commit, dive, early guess |
| \`settings.ts\`, \`ai.ts\`, \`presets.ts\`, \`challenge.ts\` | rules, tuning types, difficulty, challenges |
| \`layout.ts\` | taker / run-up geometry |
| \`Sim.ts\` | the state machine and \`step\` |

Tuning values are *data*, shared by both builds: \`data/physics.json\`, \`data/shot.json\`, \`data/ai.json\`,
\`data/takers.json\`, \`data/presets.json\`, \`data/challenges.json\`.

## 2. Conventions

- **Units:** SI. Metres, seconds, kilograms, radians (the run-up angle is stored in *degrees*).
- **Axes:** origin = centre of the goal line on the ground. +x = to the right as seen from the pitch looking at
  goal, +y = up, +z = from the goal line out into the pitch (the ball starts at +z and goals are scored at z < 0).
- **Numbers:** IEEE-754 \`double\` everywhere (JavaScript numbers). Use \`double\` in C++, not \`float\`.
  Functions used: \`sin\`, \`cos\`, \`atan\`, \`atan2\`, \`sqrt\`, \`exp\`, \`hypot\`, \`min\`, \`max\`, \`abs\`, \`floor\`, \`round\`, \`sign\`.
  Keep the order of floating-point operations identical to the TypeScript.
- **Time:** one fixed tick of \`dt = 1/240\` s per \`step\`. Interpolation for rendering happens outside the sim
  (\`prevBallPos\` is saved at the start of every tick for that purpose).
- **Randomness:** only through \`rngNext\` / \`rngRange\` on \`SimState.rng\`. The *order and number of draws* is part of the
  contract. A port is correct only if the final \`rng\` state of every golden fixture matches exactly.
- **Input edges:** the sim detects edges itself by comparing the current \`InputState\` with \`SimState.prev\`. The app must
  pass the same held-state input on every tick of a frame, except the mouse deltas (\`aimDeltaX/Y\`), which are applied once.
- **Events:** \`SimState.events\` is an append-only list that the *app* clears once per rendered frame.

## 3. The random generator

${code(fn('src/sim/math/rng.ts', 'rngNext') + '\n\n' + fn('src/sim/math/rng.ts', 'rngRange'))}

\`>>>\`, \`|0\` and \`Math.imul\` are 32-bit integer operations: use \`uint32_t\` / \`int32_t\` with wrap-around.

## 4. Geometry constants

${code(read('src/sim/constants.ts').trim())}

## 5. State and input structures

### Input
${code(iface('src/sim/Sim.ts', 'InputState') + '\n\n' + fn('src/sim/Sim.ts', 'emptyInput'))}

### Events
${code(iface('src/sim/Sim.ts', 'SimEvent') + '\n\n' + iface('src/sim/Collisions.ts', 'CollisionEvent'))}

### Simulation state
${code(
  iface('src/sim/Sim.ts', 'Phase', 'ResultType', 'KeeperTouch', 'Mode') +
    '\n\n' +
    iface('src/sim/Sim.ts', 'SimConfig', 'ShotPreview', 'SimState'),
)}

### Ball
${code(iface('src/sim/BallPhysics.ts', 'PhysicsParams', 'Knuckle', 'BallState'))}

### Shot model
${code(iface('src/sim/ShotModel.ts', 'ShotParams', 'Taker', 'ShotRequest', 'Shot', 'RunupEffect'))}

### Wall and keeper
${code(iface('src/sim/Wall.ts', 'WallState', 'WallRolls') + '\n\n' + iface('src/sim/Keeper.ts', 'KeeperPhase', 'KeeperState'))}

### Tuning types
${code(iface('src/sim/ai.ts', 'WallAi', 'KeeperAi', 'AiParams') + '\n\n' + fn('src/sim/ai.ts', 'effectiveKeeperAi'))}

### Settings (with their legal ranges)
${code(
  iface('src/sim/settings.ts', 'Frequency', 'KeeperType', 'SimSettings') +
    '\n\n' +
    fn('src/sim/settings.ts', 'defaultSettings') +
    '\n\n' +
    fn('src/sim/settings.ts', 'sanitizeSettings') +
    '\n\n' +
    fn('src/sim/settings.ts', 'windVector'),
)}

### Modes, challenges, difficulty
${code(
  iface('src/sim/challenge.ts', 'ChallengeTarget', 'ChallengeDef', 'ChallengeShot', 'ChallengeRun') +
    '\n\n' +
    fn('src/sim/challenge.ts', 'scoreShot', 'medalFor') +
    '\n\n' +
    iface('src/sim/presets.ts', 'DifficultyPreset', 'DifficultyName'),
)}

## 6. The tick: order of operations

\`step(s, input, dt)\` is the only entry point that advances time. The order below matters.

${code(fn('src/sim/Sim.ts', 'step'))}

Supporting routines that \`step\` calls (their order of RNG draws is significant):

${code(
  fn('src/sim/Sim.ts', 'setSpot', 'resetAttempt', 'layoutDefenders', 'applySettings') +
    '\n\n' +
    fn('src/sim/Sim.ts', 'classifyCrossing', 'reboundAvailable', 'finish', 'startFollowUp', 'onKeeperContact'),
)}

${code(fn('src/sim/Sim.ts', 'createSim', 'selectTaker', 'applyPreset', 'startChallenge', 'setMode', 'randomSpot', 'newSpot'))}

## 7. Function inventory

Every exported function of the sim, per module (the full source is the specification).

${['BallPhysics', 'Collisions', 'Prediction', 'ShotModel', 'Wall', 'Keeper', 'layout', 'ai', 'settings', 'challenge']
  .map((m) => `**\`${m}.ts\`**\n${code(signatures(`src/sim/${m}.ts`))}`)
  .join('\n\n')}

## 8. Verification: the golden fixtures

\`tests/golden/*.json\` are produced by the TypeScript sim (\`npm run golden\`). Each file holds one scenario, the full
resolved settings, a sampled trace and the final state. A port must replay the scenario with the *driver* below and
reproduce the file.

**Driver** (the order matters because several calls draw from the RNG):

1. \`s = createSim(cfg, seed)\` with \`cfg.settings = fixture.settings\` and the shared \`data/*.json\`.
2. If \`scenario.challenge\` is set: \`startChallenge(s, scenario.challenge)\` (it sets the spot itself).
3. \`selectTaker(s, scenario.taker ?? 0)\`; if \`scenario.foot\`, set \`s.foot\`.
4. Unless it is a challenge: \`setSpot(s, spot.x, spot.z)\`.
5. \`s.aim = aim\`, \`s.contact = contact\`; if given, \`s.runup = runup\`.
6. Inputs, with \`extra = { driven, low, contactX: contact.x, contactY: contact.y }\`:
   - \`hold = emptyInput() + extra + shoot=true\`, \`idle = emptyInput() + extra\`, \`release = idle + dummy\`.
   - \`step(hold)\` once, then \`step(hold)\` until \`s.meter >= power\` (so \`inputs.holdSteps\` ticks), then \`step(release)\` once,
     then \`step(idle)\` until \`s.phase == 'result'\` or 10 s.
   - *Forced* scenarios (\`force\`) skip the strike: set \`phase = 'flight'\`, the keeper (if any) to \`phase = 'watching'\`, and the ball
     position/velocity (also \`prevBallPos\`), then \`step(idle)\` until the result.
   - \`continueAfterResult\`: keep stepping \`idle\` until the phase leaves \`'result'\` (at most 20 s in total).
7. Every 24th tick record a sample; at the end record \`final\` (\`atResult\` is the result and stats at the moment the attempt was decided).

**Tolerances** a port must meet: all strings, booleans, integers and the \`rng\` state **exactly**; every sampled position
within **1 cm** (and velocities/spins within 1e-3 relative) along the whole flight. The TypeScript self-check uses 1e-6.

## 9. Changing the simulation

Any change to \`src/sim/\` (or to \`data/*.json\`) must be made in the TypeScript and the C++ in the same change, followed by
\`npm run golden\` and \`npm run contract\`; CI fails if either committed file is stale or the native build disagrees.
`);

  return sections.join('\n\n') + '\n';
}

describe('Porting contract document', () => {
  const doc = buildContract();

  it('is generated from every structure the port needs', () => {
    for (const name of ['InputState', 'SimState', 'BallState', 'WallState', 'KeeperState', 'SimSettings', 'ShotParams', 'KeeperAi', 'ChallengeRun', 'rngNext', 'function step']) {
      expect(doc, name).toContain(name);
    }
    expect(doc.length).toBeGreaterThan(20000);
  });

  it('is up to date (run `npm run contract` after changing src/sim)', () => {
    if (process.env.UPDATE_CONTRACT) {
      mkdirSync(join(ROOT, 'docs'), { recursive: true });
      writeFileSync(OUT, doc);
    }
    expect(existsSync(OUT)).toBe(true);
    expect(readFileSync(OUT, 'utf8')).toBe(doc);
  });
});
