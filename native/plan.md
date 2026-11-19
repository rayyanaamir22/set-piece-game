# Phase B: Set Piece native (C++20 / OpenGL 4.1)

## Goal
A desktop build of the same game that looks and feels like a modern football game. It runs at a high refresh rate, uses the controller fully, and plays bit-for-bit like the web build, because the gameplay simulation is a direct port verified by golden tests.

## What Phase B adds over the web build
**Visuals**
- **Materials and lighting:**
  - PBR materials with image-based lighting from an HDR sky.
  - Cascaded shadow maps (3 cascades) for players, wall, net and posts.
  - HDR rendering with ACES tone mapping.
  - Day, dusk and night presets; at night, four shadowed floodlights.
- **Pitch:** grass with real depth, shell-textured or instanced blades near the camera. It sways in the wind and is scuffed where the ball skids, and the mowing stripes catch the light.
- **Net:** a cloth simulation on the CPU (about 2-3k particles, mass-spring, collision with the ball). It bulges and swings back after a goal instead of rippling lines.
- **Stadium:** a full 3D stadium with floodlight towers and 10-40k instanced crowd members. They animate with vertex-animation textures and react: stand, cheer, groan.
- **Post-processing:**
  - Bloom and motion blur (strongest on the ball).
  - Depth of field in replays and photo mode.
  - Contact shadows / SSAO.
  - MSAA plus FXAA.
- **Weather:** rain particles, a wet sheen on the pitch, and corner flags bending in the wind (the sim already has wind).

**Characters and animation**
- **Players:** skinned glTF players with an animation state machine (idle, run-up, strike, follow-through, celebration).
- **Strike:** foot IK so the striking foot meets the ball at the sim's contact moment, and the plant foot doesn't slide.
- **Keeper:** dive, step, catch, parry and get-up clips, driven by the sim's capsule (feet `a`, hands `b`). The animation follows the hitbox, so what you see is what saves.
- **Wall:** jump and lying-man animations, and encroachers jogging off the line.

**Ball**
- A seamed ball whose rotation is integrated from the sim's spin, so knuckle and topspin are readable.
- It squashes at the moment of the strike.
- Slow-motion close-ups of the contact point in replays.

**Replays and cameras**
- A ring buffer of full sim snapshots for the last 10 s, so replays are exact.
- Replay cameras: broadcast, behind-goal, keeper's-eye, ball-follow and free cam.
- Slow motion with interpolation, plus a photo mode.

**Audio (miniaudio)**
- 3D positional strike, post "ding", net and bounce sounds, each scaled by impact speed.
- Crowd ambience, a reaction that builds with the ball's flight, whistle and goal roar.

**Feel and input (SDL3)**
- Raw keyboard and mouse, plus gamepads with rumble on the strike, post and save.
- **DualSense adaptive trigger:** the right trigger stiffens as the power meter charges, so the sweet spot can be felt.
- Uncapped or high-refresh frame rate (120-240 fps) and low input latency.

**New gameplay** (sim changes go into both builds, see "Sim change policy")
- **2-player mode:** player 2 controls the keeper with a second controller. They pick a dive side and height at the strike, or play the AI's read with their own reflexes.
- **Wet pitch:** rain lowers ground friction and raises skid, which changes how the low drive and bounces behave.
- **Gusts:** wind speed varies over time with a seeded noise, instead of staying steady.

**Tools**
- A Dear ImGui developer overlay: live editing of every `data/*.json` value with hot reload.
- Debug drawing of hitboxes, predicted paths and keeper reads.
- A frame profiler and a golden-test runner.

**Platforms**
- macOS (Apple Silicon and Intel), Windows and Linux, with a Steam Deck profile.

## Repo layout
```
data/                 shared tuning (unchanged, read by both builds)
assets/               shared art/audio (Git LFS): characters/, stadium/, ball/, hdri/, audio/
tests/golden/         parity fixtures produced by the web build (Phase A step 13)
src/, tests/ …        web build (unchanged)
native/
  plan.md
  CMakeLists.txt      CMake ≥ 3.25, deps via CPM.cmake (pinned versions)
  src/
    sim/              1:1 port of src/sim (vec3, rng, BallPhysics, Collisions, Prediction,
                      ShotModel, Wall, Keeper, settings, ai, layout, Sim) – plain structs + free functions
    platform/         window (SDL3), input mapping, timing, file paths, prefs (JSON in user dir)
    gfx/              thin GL 4.1 wrapper: buffers, textures, shaders (hot-reload), FBOs, state cache
    render/           frame graph-lite: depth prepass → CSM → forward PBR (MSAA) → post chain
    world/            pitch+grass, markings, goal+net cloth, stadium, crowd, weather, sky/IBL
    anim/             glTF skins (cgltf), clips, blend tree/state machine, two-bone IK
    game/             app loop, camera rig, replay buffer, HUD (custom 2D batcher + stb_truetype),
                      menus/settings (ImGui-styled first, custom later), audio events
    dev/              ImGui dev overlay, debug draw, profiler
  shaders/            GLSL 410 core
  tests/              doctest: golden parity, ported unit tests, math
```
**Dependencies:**
- SDL3 (window, input, haptics)
- glad (GL 4.1 core loader)
- glm
- cgltf, stb_image, stb_truetype
- Dear ImGui (SDL3 + OpenGL3 backends)
- miniaudio
- nlohmann/json
- doctest
- Tracy (optional profiler)

## Engineering rules
- **The sim is a port, not a rewrite.** It keeps the same names, the same state structs, the same order of RNG draws and the same 240 Hz step. It uses `double` everywhere, matching JS numbers. The sim must not include anything from `gfx/`, `render/`, `world/` or `anim/`, and a CMake target boundary enforces this.
- **Rendering reads the sim and never writes to it.** It interpolates between `prevBallPos` and the current position, just like the web build.
- **Sim change policy.** Any gameplay change (including the new 2P keeper, wet pitch and gusts) lands in `src/sim` (TS) and `native/src/sim` in the same change, with the goldens regenerated. CI fails if the native sim drifts from the goldens.
- **Assets** are CC0 or licensed for redistribution: Poly Haven HDRIs, Quaternius/Kenney models, and Mixamo animations retargeted, used in-game only. No real clubs, players, kits or league branding.

## Milestones (each ends with something playable or verifiable)
**B0. Prerequisites (in Phase A): done**
- `npm run golden` plays 28 scripted scenarios through `src/sim` and writes `tests/golden/*.json`: free flight, curl, dip, lift, knuckle, driven, low drive, three winds, run-up extremes, post, bar, wide, over, wall block, wall dip, wall jump, dummy run, lying man, encroachment retake and goal-stands, keeper save, beaten corner, hold, parry + rebound, and a challenge shot.
- `npm run contract` writes `docs/porting-contract.md`, generated from the TypeScript: every state/input structure, the exact `step` order, the RNG, the driver procedure and the tolerances. A test fails if it is stale.
- Still to do: set up Git LFS for `assets/` when the first binary assets arrive.

**B1. Platform shell**
- CMake project with an SDL3 window, a GL 4.1 core context and ImGui.
- Fixed 240 Hz sim loop with render interpolation.
- Input mapping identical to the web build: keyboard, and gamepads via SDL's mapping database.
- CI builds on macOS, Windows and Linux.
- Exit: an empty scene runs at the display refresh rate, and input events show in the ImGui overlay.

**B2. Sim port and parity**
- Port every `src/sim` module and load `data/*.json`.
- The golden runner replays each fixture through `sim::step` and compares with the recorded states.
- Port the pure Vitest unit tests to doctest.
- Exit: all goldens pass in CI (ball position within 1 cm for the whole flight, identical results and RNG state), and the ported unit tests pass.

**B3. Graybox and feature parity**
- Pitch, markings, goal frame, capsule players, ball, reticle, aim arrow, run-up marker and path aid.
- HUD: distance, run-up and wind readouts, power meter, contact widget, result toasts.
- Controls and Settings screens with every web setting, saved to a prefs file.
- Exit: the native build plays exactly like the web build. A side-by-side playtest from the same seed gives the same outcomes.

**B4. Lighting and materials**
- PBR shader, IBL (pre-filtered environment and BRDF lookup baked at load), 3-cascade shadow maps.
- HDR targets with ACES tone mapping, 4x MSAA, and day/dusk/night presets.
- Exit: the graybox scene in PBR stays at 60 fps on an M1 MacBook Air at 1080p "high".

**B5. Characters and animation**
- glTF skinning (GPU, ≤ 64 bones), clip playback and blending, run-up / strike / follow-through.
- Strike IK aligned to the launch tick, and plant-foot IK.
- Keeper clips mapped onto the sim capsule (blend chosen by dive height and side; get-up after the sink).
- Wall jump, lying man and encroacher run.
- Exit: every sim outcome has a believable animation, and the visible keeper's hands stay within 10 cm of the sim's hand point during saves.

**B6. Net cloth, grass and ball**
- Net as a CPU mass-spring cloth (Verlet, about 2.5k particles, pinned at the frame, collides with the ball sphere). It reacts to the sim's net events, while the ball itself stays controlled by the sim's net box.
- Grass shells or blades with wind sway, and skid marks where the ball rolls.
- Seamed ball rotating from the sim's spin, squashed at the strike.
- Exit: a goal visibly bulges the net. Cloth costs ≤ 1 ms per frame.

**B7. Stadium, crowd and weather**
- Stadium model with LODs and floodlight towers (night mode with shadowed floodlights).
- Instanced crowd with vertex-animation textures and a reaction state driven by sim events (near miss, goal, save).
- Rain particles, wet-pitch shading and wind-blown corner flags.
- Exit: a full stadium at night keeps the frame-rate target on the reference machines.

**B8. Post-processing, cameras and replays**
- Bloom, per-object motion blur, DOF and SSAO/contact shadows.
- Broadcast and dynamic cameras.
- Snapshot ring buffer with replay from 4 cameras, slow-mo and photo mode.
- Exit: every goal offers an instant replay. Scrubbing back and forth reproduces the exact flight.

**B9. Audio**
- miniaudio engine with 3D sources mapped to sim events (impact speed → volume and pitch).
- Crowd mixer reacting to ball proximity and the result.
- Volume settings.

**B10. Feel**
- Rumble profiles, DualSense adaptive trigger for the power meter, and a latency pass (late input sampling, optional "render ahead 0").
- Frame-rate options (vsync, capped, uncapped) and Steam Deck input and graphics profiles.
- Exit: measured input-to-photon latency ≤ 2 frames at 120 Hz.

**B11. New gameplay (shared sim)**
- 2P keeper mode: a keeper input struct in the sim; player 2's dive replaces the AI read when present.
- Wet pitch (friction and restitution from the weather setting) and seeded gusts.
- Implemented in TS and C++ together, with new goldens.
- Exit: goldens pass in both builds, and 2P works locally with two controllers.

**B12. Menus, packaging and release**
- Front-end menus (custom UI, controller-navigable) and a graphics quality settings page.
- Packaging: signed and notarized macOS .app, Windows zip/installer, Linux AppImage.
- Crash logging.
- Exit: release builds on all three platforms from CI.

## Performance targets
| Machine | Resolution / preset | Target |
|---|---|---|
| M1 MacBook Air (GL 4.1) | 1080p high | 60 fps |
| Mid-range discrete GPU (e.g. RTX 3060) | 1440p ultra | 144 fps |
| Steam Deck | 800p medium | 60 fps |

**Frame budget at 60 fps (16.6 ms):**
- Sim: 4 steps, < 0.2 ms
- Cloth: ≤ 1 ms
- Animation: ≤ 1 ms
- Shadows: ≤ 3 ms
- Main pass: ≤ 6 ms
- Post-processing: ≤ 3 ms
- Remainder: crowd and UI

## Risks and mitigations
- **macOS OpenGL is deprecated at 4.1.** Keep `gfx/` as the only place that calls GL, so a Metal backend (or sokol_gfx) can replace it later. No compute shaders: grass, cloth and crowd use the CPU, instancing or transform feedback.
- **Floating-point drift between V8 and the C++ math library.** Use `double`, keep the operation order identical, compare goldens with tolerances, and compare RNG state exactly. If `sin`/`exp` differ, use the same polynomial approximations on both sides.
- **Scope creep.** Each milestone has an exit check. B3 (parity) gates all visual work.
- **Asset licensing.** Track sources and licences in `assets/LICENSES.md`.

## Verification
- **CI matrix (macOS, Windows, Linux):** build, doctest unit tests, golden parity. The web job runs Vitest and regenerates the goldens, then checks that nothing changed.
- **Per-milestone exit checks** as listed above, recorded in the milestone table in this file.
- **Performance:** Tracy captures on the reference machines for B4, B7 and B8, plus an in-game frame-time overlay.
- **Manual:** side-by-side playtests against the web build from the same seed (B3 onward).
