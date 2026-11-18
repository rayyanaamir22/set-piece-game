# Set-Piece Game: Free Kick Plan

A shooting-only free-kick game whose mechanics match EA FIFA / FC free kicks (FC 24+ control scheme), built in two phases:

1. **Phase A: web.** Three.js + TypeScript + Vite. The fastest way to find out how the game should feel and tune it.
2. **Phase B: native.** An improved desktop build in C/C++ with OpenGL, ported from the Phase A simulation once the mechanics are settled.

## Status (Phase A)
| Step | State |
|---|---|
| 0 Fresh start (Vite + TS + Vitest + ESLint boundary rule) | done |
| 1 Scene: pitch, markings, goal + net, stands, ad boards, set-piece camera | done |
| 2 Ball physics: RK4, drag crisis, Magnus, knuckle wobble, bounce/roll, post/bar/net | done |
| 3 Shot input: reticle + arrow, contact widget, power meter with sweet spot, driven/low | done |
| 4 Outcomes: goal / wide / over / short, woodwork flag, result toast, auto next spot | done |
| 4b Menu: Controls and Settings sheets behind top-right icons, pausing the game, settings saved to localStorage | done |
| 5 Wall: auto/explicit player count, placement covering the near post, capsule collisions, tunable height, "blocked" result | done |
| 5b Wall jump: per-player randomised jump on the strike, lifts the capsules, arms-up animation | done |
| 6 Goalkeeper: positioning, reaction, partial spin read, timed dive, reach ellipse, hold / parry / block, result "saved" | done |
| 7-13 | see "Remaining Phase A steps" below |

### Decisions made while building
- **Fixed step is 240 Hz, not 120 Hz.** At 34 m/s the ball moves 0.14 m per step, which is under the post radius, so it can't tunnel through a post.
- **The reticle is where the ball arrives.** A shooting-method solver (`ShotModel.computeShot`) picks the launch angles so the spinning, dragged ball reaches the reticle. The contact point therefore decides the *shape* of the path (curl round a wall, dip over it) and the arrow shows the initial direction, which diverges from the reticle when there's curl. Timing and taker accuracy add error on top. The solver ignores the wall, so the player has to choose a path that clears it.
- **Contact point follows the physics.** Striking the right of the ball curls it left, striking the top dips it, and a dead-centre strike at high power is a knuckleball (low spin plus a seeded lateral wobble).
- **The wall covers the near post**, like a real one: its near edge sits on the ball→near-post line and it extends towards the centre of goal. Shots to the far corner go round it; central and near-side shots need to go over it. Measured at 25 m: a flat shot is about 1.6-1.9 m high at the wall (blocked by the default 1.85 m wall), a dipping shot about 2.3 m (clears all wall heights).
- **The keeper is beatable by design.** He (a) covers the far post and reaches only an ellipse around his stance, so top corners and the near post are out of range; (b) reacts after 0.28-0.48 s depending on skill; (c) reads the flight with only 50-95% of the spin, so curl can fool him, and never reads a knuckle wobble; (d) times his dive to arrive with the ball, with a second, better read just before he leaves the ground; (e) holds only slow shots (8-18 m/s depending on skill), otherwise he parries. Measured at skill 0.7 from 24 m, no wall: central mid-height shots are saved about 70%, far top corners score about 90%, near-post shots always score. Everything is tunable in `data/ai.json`.
- **The wall jumps by about 0.4 m** (each player with a random 0-0.12 s delay and 0-15% less height, 10% chance to stay down), so with jumping on a flat shot is blocked and a central dip often clips the jumpers and goes over. The player's answers are a higher arc (less power plus dip), going round the end of the wall, or turning jumping off.
- **Attempts end quickly after a deflection** (2.5 s after a wall hit, 1.2 s after a keeper touch) instead of waiting for the ball to stop rolling.
- **Settings live in `SimState.settings` (`src/sim/settings.ts`)**, so the native build can load the same values. Anything not simulated yet is shown greyed out with a "soon" badge, so the final settings screen is already in place.
- **Controls:** keyboard + gamepad are in. Mouse aim is not yet.

### Settings reference
| Setting | Range | State |
|---|---|---|
| Distance to goal / angle / place ball / random spot / taker foot | 17-40 m, ±60° | active |
| Wall on/off | bool | active |
| Wall height | 1.60-2.10 m, default 1.85 | active |
| Players in wall | 0 (auto) to 6 | active |
| Taker accuracy / curve / power | 0-1 / 0-1.5 / 0.7-1.2 | active |
| Show predicted path | bool | active |
| Wall jumps on the strike | bool | active |
| Goalkeeper present | bool | active |
| Keeper skill | 0-100%, default 70% | active |
| Wind on/off, speed (0-15 m/s), direction (0-360°) | bool + sliders | soon (step 7) |

### Remaining Phase A steps
Each step lists what to build, the setting that controls it, and how it is tested.

5c. **Wall extras**
   - A "lying man" behind the wall that blocks low shots (toggle).
   - Occasional early charge-off (encroachment) that gives a retake (toggle).
   - Wall AI refinements: jump only on shots it judges dangerous, small shuffle on the run-up.
6b. **Goalkeeper extras**
   - Better dive/save animation (gloves, rotation about the body, rolling after the dive) and a keeper-specific camera replay.
   - Rebounds: a parry should sometimes drop to the taker for a follow-up (needs a follow-up shot mode).
   - Early-guess behaviour (rarely commits before the strike).
   - Keeper stats per team (separate reach, reaction and hold values) via `data/ai.json` presets.

7. **Wind** (`windEnabled`, `windSpeed`, `windDirection`)
   - Add wind to `BallPhysics.acceleration` (drag acts on velocity relative to the air). It is the only change needed in the sim, and the native port gets it for free.
   - HUD wind indicator (arrow and speed).
   - Design decision to confirm: the aim solver ignores wind, so the player has to allow for it (it makes the ball miss the reticle). The alternative is to include it so the reticle stays truthful.
   - Tests: a crosswind pushes the ball the right way and the deflection grows with speed.
8. **Run-up adjustment**
   - Move the taker's start point along an arc around the ball.
   - A straighter approach gives more power and less curl, a wider one more curl, feeding into `ShotModel`.
   - The render layer already has the run-up animation.
9. **Mouse aiming**
   - Drag or pointer-lock to move the reticle and contact point.
   - Sensitivity slider in settings.
10. **Presentation**
    - Vanishing spray line for the ball spot and the 9.15 m wall line.
    - Goal replay from 2-3 cameras (ring buffer of sim snapshots).
    - Audio: crowd, strike, post, net and whistle sounds with a volume slider.
    - Better crowd texture and some stadium lighting.
    - Graphics quality setting (shadows, pixel ratio).
11. **Modes**
    - Practice arena (ball placement already exists in settings).
    - Skill challenges with target rings and bronze/silver/gold scoring.
    - Taker roster picker.
    - Dummy run-up.
12. **Settings and UI polish**
    - Difficulty presets (accuracy, keeper skill, wall).
    - Key remapping.
    - Camera distance slider.
    - Touch/mobile layout for the menu.
13. **Freeze the simulation:** record golden trajectories and outcomes for a fixed set of shots, then write down the `Sim` state and input structs as the porting contract for Phase B. Includes wall and keeper shots.

## 1. Tech choice
**Phase A: Three.js + TypeScript, bundled with Vite (no Next.js, no React).** The game has one ball, about 6 to 8 low-poly figures and a static stadium. That workload is small for WebGL. The hard part is ball physics and game feel, and the web gives the fastest loop for tuning those: hot reload, a debug panel, and a link anyone can play. A plain Vite app keeps the page to one canvas with a lightweight DOM overlay for the on-screen display. A framework only becomes worth it if the menus get complex.

**Phase B: native C/C++ + OpenGL 4.1.** 4.1 is the highest version macOS supports. Phase B brings better rendering (proper shadows, post-processing, higher-poly models, a dense crowd) and lower input latency. Suggested stack:
- GLFW (window and input)
- glad (OpenGL loader)
- cglm, or glm if C++
- cgltf (model loading)
- Dear ImGui (debug panel and on-screen display)
- miniaudio (sound)
- SDL3 instead of GLFW if gamepad support through GLFW isn't good enough

If you want Dear ImGui and glm without C bindings, use **C++17 written mostly C-style**. Decide C or C++ when Phase B starts.

**Designing for the port:** the simulation must not depend on the renderer.
- `src/sim/` holds physics, shot model, wall and keeper AI, and the state machine. It is pure TypeScript with no Three.js imports, its own small `vec3` maths, and deterministic seeded randomness.
- `src/render/` is a thin Three.js layer that reads the simulation state and draws it.
- Tuning values (physics constants, stats, AI parameters) live in `data/*.json`, and both versions load the same files.
- Phase A records reference shot trajectories (`tests/golden/*.json`). The native port must reproduce them within a tolerance, which proves the C version behaves the same.

The physics, shot model and AI are therefore written once in their final form, and porting them is mostly mechanical.

- **Physics:** write our own ball integrator instead of using a full engine. A custom one controls the Magnus effect, drag and knuckle behaviour exactly, and the shapes it collides with are simple: ground plane, posts and crossbar as cylinders, net, wall players as capsules, and the keeper. A custom integrator also ports easily to C, which a physics engine would not.
- **Assets:** glTF (`.glb`) for player and keeper models, with animations from Mixamo or similar. Start with capsule placeholders.
- **Units:** SI throughout (metres, seconds, kg) so real pitch dimensions and physics constants can be used directly.

## 2. Scene: final third of the pitch
- Pitch about 68 m wide and 40 m deep from the goal line. Mown stripe texture, correct markings: goal line, 18-yard box (16.5 × 40.32 m), 6-yard box (5.5 × 18.32 m), penalty spot at 11 m, arc of radius 9.15 m, corner arcs.
- Goal 7.32 × 2.44 m, with posts and crossbar 12 cm in diameter. The net is a mesh that ripples when hit (vertex displacement), with a depth of about 2 m.
- Backdrop: stands behind the goal (a billboard crowd texture to start), advertising boards, sky/HDRI, floodlights. Shadows from the main light.
- The free kick is placed outside the box. Its position can be set or randomised: distance 18–35 m, angle −45° to +45°.

## 3. FIFA/FC-style feature list
### Core shooting (MVP)
1. **Set-piece camera:** third-person, behind and slightly above the taker. It frames the ball, the wall and the goal, and you can toggle between near and far views.
2. **Taker placement by preferred foot:** a right-footed taker stands left of the ball, a left-footed taker stands right of it. You can switch the taker's foot.
3. **Run-up adjustment:** before the shot you can move the taker's starting spot along an arc around the ball, changing run-up distance and angle. A straighter run-up gives more power and less curl. A wider angle gives more curl.
4. **Aim reticle + arrow:** the left stick, WASD or the mouse moves a reticle across the goal frame. An arrow from the ball points toward it. The reticle drifts slightly or sways less depending on the taker's free-kick accuracy stat.
5. **Contact point selection (FC 24-style):** the right stick or IJKL picks a point on a ball widget.
   - Side → sidespin (curl).
   - Below centre → backspin/lift.
   - Above centre → topspin/dip.
   - Dead centre with high power → **knuckleball**: low spin and an erratic late wobble.
6. **Power meter with timing:** hold shoot to charge and release to fire. The bar has a highlighted sweet spot. Releasing inside it gives full accuracy. Releasing outside it adds aim error that grows with the miss. Overcharging sends the shot over the bar.
7. **Shot types:**
   - Normal: placed, curling shot.
   - Driven/power: modifier button, flatter and faster, less curl.
   - Low shot under the wall: modifier button, kept on the ground.
   - Chip/lob: modifier button, for short free kicks over the keeper (stretch goal).
8. **Run-up and strike animation:** the taker steps in. The ball is launched at the contact frame, not when the button is released.
9. **Ball physics:** gravity, quadratic drag (Cd ≈ 0.25, with a drag-crisis drop at high speed), Magnus lift (ω × v), spin decay, a bounce/roll model with restitution and friction, and post/crossbar deflection. Fixed 120 Hz physics step, rendering interpolated between steps.
10. **Defensive wall:**
    - The number of players (1–5) depends on distance and angle, the way FIFA's AI decides.
    - Standing 9.15 m from the ball, lined up to cover the near post.
    - They jump on the strike.
    - Sometimes one player lies down behind the wall to block low shots.
    - Occasional early charge-off (encroachment).
11. **Goalkeeper:**
    - Takes up position covering the far side, about one step off centre.
    - Reacts after a short reaction delay that depends on stats.
    - Predicts where the ball will land and picks a dive type: a step-across for close shots, a full dive, a tip over the bar, or a parry.
    - Has a reach envelope, so top corners plus dip can beat him.
    - Can guess early (rare cheating step).
12. **Outcomes:**
    - Goal (with net detection).
    - Saved, parried or held.
    - Hit the post or bar.
    - Hit the wall.
    - Wide or over.
    The result is shown with a short label, for example "Top bins!" or "Off the woodwork".

### Presentation
13. **Vanishing spray:** a line on the ground marking the ball spot and the 9.15 m wall line.
14. **Distance readout:** the distance to goal shown before the shot, as in FIFA ("27 m").
15. **Goal replay:** record ball and figure transforms for a few seconds and play them back from 2–3 camera angles.
16. **Audio:** crowd ambience, a reaction that builds as the shot approaches goal, the strike sound, net and post sounds, the referee's whistle.
17. **HUD:** taker name and stats (FK accuracy, curve, shot power), ball contact widget, power meter, attempts and score.

### Game modes and progression (post-MVP)
18. **Practice arena:** place the ball anywhere and retry instantly.
19. **Skill challenges (FIFA "skill games"):** score through target rings in the corners, or curl around a wall of mannequins, with bronze/silver/gold scoring.
20. **Taker roster:** fictional players with different stats and preferred feet, which changes how spin and accuracy behave.
21. **Dummy taker:** two players over the ball, one of whom does a fake run (stretch goal).

## 4. Controls (default)
| Action | Keyboard/mouse | Gamepad |
|---|---|---|
| Move aim reticle | WASD / mouse | Left stick |
| Pick contact point | IJKL / right-mouse drag | Right stick |
| Adjust run-up | Q/E + WASD | LT/RT + left stick |
| Shoot (hold/release) | Space / left click | B/Circle |
| Driven shot modifier | Shift | RB/R1 |
| Low shot modifier | Ctrl | LB/L1 |
| Switch foot / taker | F / Tab | Y/Triangle |
| Camera toggle | C | Select/View |
| Reset | R | Start |

Every action goes through an `InputManager` that maps it to abstract actions, so devices can be swapped or remapped.

## 5. Architecture (Phase A)
```
index.html                  one <canvas> + HUD overlay root
data/                       shared with Phase B
  physics.json              drag, Magnus, restitution, friction, spin decay
  takers.json               roster: foot, FK accuracy, curve, power
  ai.json                   wall/keeper tuning
src/
  main.ts                   bootstrap: load data, create Sim + Renderer + Input, run loop
  loop.ts                   fixed 120 Hz sim step, render with interpolation
  sim/                      ── no Three.js imports; portable to C ──
    math/vec3.ts, rng.ts    small vector maths, seeded PRNG
    constants.ts            pitch/goal/ball dimensions (SI)
    BallPhysics.ts          integrator, drag (+ drag crisis), Magnus, knuckle wobble
    Collisions.ts           ground, posts/bar, net, capsules (wall, keeper)
    ShotModel.ts            (aim, contact point, power, timing, stats, modifiers) → v₀, ω₀
    settings.ts             SimSettings: wall, keeper, wind… + legal ranges
    Wall.ts                 wall layout + (later) jump AI
    KeeperAI.ts
    FreeKickState.ts        Setup → Aim → Charge → RunUp → Flight → Result → Reset
    Sim.ts                  world state + step(input, dt) + event queue (goal, save, post…)
  render/                   Three.js only reads Sim state
    Scene.ts, Pitch.ts, Goal.ts, Net.ts, Stadium.ts, Figures.ts
    CameraRig.ts            set-piece, follow-ball, replay cams
    AimViz.ts               reticle, arrow, spray lines
  input/Input.ts            keyboard, mouse, Gamepad API → abstract action struct
  hud/                      plain DOM/CSS: power meter, contact widget, result toast,
                            Menu (top-right icons), ControlsPanel, SettingsPanel, storage
  audio/Audio.ts            Web Audio
  replay/Recorder.ts        ring buffer of Sim snapshots
tests/                      Vitest unit tests + golden/*.json trajectories
```
The simulation steps once per frame through `Sim.step(input, dt)`, with input passed in as a plain struct. That makes it deterministic and testable, and it maps one-to-one onto a C `sim_step(Sim*, const Input*, float dt)`.

### Phase B layout (for reference)
`native/` holds the C/C++ version:
- `sim/` is a direct port of `src/sim/`.
- `render/` is OpenGL 4.1 with shadow maps, PBR and post-processing.
- `platform/` contains GLFW or SDL3, ImGui and miniaudio.

It builds with CMake, loads the same `data/` and is tested against `tests/golden/`.

## 6. Milestones
### Phase A: web
0. **Fresh start:**
   - Delete the Next.js scaffold: `src/app`, `next.config.ts`, the Tailwind/PostCSS config and `eslint.config.mjs`.
   - Set up Vite + TypeScript + `three` + Vitest + lil-gui.
   - Create the empty `sim/` and `render/` folders with an ESLint rule that blocks `three` imports inside `sim/`.
1. **Scene:** pitch, markings, goal, net, sky, lights, set-piece camera. Ball placed at a configurable spot.
2. **Ball physics:** integrator, drag, Magnus, bounce. The settings panel can place the ball and draw the predicted flight path.
3. **Shot input:** InputManager, aim reticle and arrow, contact-point widget, power meter with sweet spot. ShotModel turns these into a launch.
4. **Collisions and outcomes:** posts, bar, net, goal-line detection, result toast, reset.
5. **Wall:** layout, collisions, height setting. Done. Jump and extras: see 5b-5c above.
6-13. See "Remaining Phase A steps" at the top of this file.

### Phase B: native OpenGL
11. **Platform shell:** CMake, a GLFW or SDL3 window, an OpenGL 4.1 context, ImGui, the fixed-step loop and gamepad input.
12. **Port `sim/` to C:** done when it passes every golden trajectory test.
13. **Renderer:** pitch, goal and net, then shadow maps, PBR materials, an HDR sky, glTF figures with skeletal animation, instanced crowd, and post-processing (tone mapping, bloom, motion blur on the ball).
14. **Parity and polish:** the on-screen display in ImGui or a custom UI, miniaudio sound, replays, and builds for macOS, Windows and Linux.

## 7. Verification
- Unit tests for ShotModel and BallPhysics:
  - No spin → a straight parabola.
  - Sidespin → curl of the expected sign.
  - Topspin → shorter flight.
  - Knuckle → bounded lateral jitter.
- Tuning harness (debug panel): check that a 25 m shot at about 28 m/s with typical spin curls 1.5–3 m and stays below about 2.4 m when it reaches the goal line, which is roughly real-world behaviour.
- Manual playtests with `npm run dev` on keyboard and on a gamepad, checking each outcome type from 3 distances × 3 angles.
- Performance: a steady 60 fps in Chrome and Safari on a laptop, with no leaks across resets (renderer.info).
- Phase B: a CTest run replays `tests/golden/*.json` through the C simulation. Position error must stay under 1 cm along the whole flight, and every outcome must match.

