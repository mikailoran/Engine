# Architecture: engine, game and editor

How the engine, the game and the Qt editor divide the work. This page
describes **where the code is going**; `CLAUDE.md` and the code describe where
it is today. The goals and milestones behind it are in
[objectives.md](objectives.md).

**Built so far (M1, M2):** the engine is a library (`Engine`, in
`src/engine/engine.h`) driven by the SDL `game` host, with the ImGui tools in
`src/devtools/`. glTF meshes compile through the build, and a level shell
collides through a triangle-mesh collider (the first version of the static
shell below). The game-logic library (`src/game/logic/`) holds the first
gameplay: a first-person `Player` on a kinematic character (`CharacterBody`),
walking a level made in Blender. The game starts in play mode; F1 switches to
the devtools. The editor doesn't exist yet.

---

## Layers

```
editor (Qt exe) ──┐
                  ├──► game logic (lib) ──► engine (lib) ──► ecs_core
game (SDL exe) ───┘                           ▲
     └──► devtools (ImGui) ───────────────────┘
```

| Layer | Directory | May include | Must never include |
|---|---|---|---|
| **Engine** | `src/engine/` | bgfx, bx, Jolt (only inside `physics/`), nlohmann/json | Qt, SDL, ImGui, game logic, editor |
| **Game logic** | `src/game/logic/` (library `game_logic`) | Engine | Qt, SDL, ImGui, devtools, editor |
| **Game host** | `src/game/` | Engine, game logic, devtools, SDL3 | Qt |
| **Devtools** | `src/devtools/` | Engine, ImGui | Qt, SDL |
| **Editor** | `src/editor/` | Engine, game logic, Qt | bgfx or Jolt directly |

Includes stay `src/`-rooted: `"engine/ecs/core/types.h"`. Namespaces follow
the same split: `engine` (with `engine::physics` for the Jolt facade),
`devtools`, `game`, and later `editor`.

The editor links the game's logic rather than excluding it. Playing in the
editor runs the real gameplay systems, and the inspector has to show gameplay
components. The dependency only points one way: game logic never includes
anything from the editor.

## Hosts drive the engine

The engine is a library. It does not own the process, the OS window, the
event loop or the clock. A **host** does: the SDL `game` executable, or the Qt
editor. Each frame, the host:

1. turns its own events (SDL or Qt) into the engine's `Input`, in backbuffer
   pixels;
2. runs its own tools that must come before simulation (free camera, click
   selection);
3. **ticks the engine**: advance the world one frame, then render it;
4. runs its tools that draw after the frame (the ImGui overlay);
5. ends the frame (deferred entity destruction).

"Tick" is the unit of work: everything the engine does for one frame. The API
splits it so a host can run its tools in between:

```cpp
Engine engine(window.Surface());   // native handles and backbuffer size
engine.LoadScene("assets/scenes/debug.json");
engine.SetActiveCamera(camera);

// Host loop
engine.Resize(window.BackbufferSize());     // cheap when unchanged
engine.Update(ctx);                         // physics, lighting
engine.Render(ctx, {.debug_draw = true});   // debug draw, scene, bgfx::frame()
engine.EndFrame();                          // Ecs::Flush
```

The engine creates its rendering context from a `NativeSurface` (window and
display pointers, plus Wayland or X11) that the host fills. The SDL host gets
it from SDL's window properties; the editor would get it from an embedded
`QWindow`.

**Edit vs play.** The game always simulates. The editor renders without
simulating while you arrange things, and simulates while ▶ is pressed. Stop
restores the scene from before play: serialize the world to a string on Play,
reload it on Stop. That reuses the scene saver, and tests it on every play.

## Mechanism vs policy

The engine offers **mechanisms**: draw an outline, tint, draw a wire shape,
cast a pick ray. Its users decide **policy**: what is selected, hovered,
targeted, invalid. A quick test: a name that says **why** (`Selected`,
`Hovered`) does not belong in the engine; a name that says **what**
(`Highlight`, `Outline`, `WireBox`) does.

Highlights come from two places, and they travel through two channels:

| | Gameplay visuals | Editor and devtools overlays |
|---|---|---|
| Example | The prop you can grab glows | The selected crate gets an outline |
| Part of the game | Yes, visible in the standalone game | No; the game host never asks for it |
| Expressed as | Component data, set by gameplay systems | Requests made every frame (`Highlight(entity)`), forgotten after drawing |
| Saved in the scene | Authored values yes, runtime ones no | Never |

Overlays are requested every frame because a component written by two owners
collides: deselecting in the editor would clear the glow gameplay set. Editor
state written into the world would also leak into saved scenes.
`DebugDrawSystem` already follows the per-frame pattern.

Picking follows the same split. The engine answers "which entity is under
this point" (`Pick`). Whether a click selects it, grabs it or does nothing is
up to the host.

## Component categories

| Category | Examples | Saved in the scene? | Lives in |
|---|---|---|---|
| Engine | `Transform`, `Renderable`, `Collider`, `RigidBody`, `Camera`, `DirectionalLight`, `Name` | Yes | Engine |
| Gameplay | Character tuning, interactables, spawn points | Yes | Game logic |
| Runtime only | `PhysicsLink`, per-play counters | No | The system that owns it |
| Editor state | Selection, gizmo drags, editor camera, undo stack | No; preferably not components | Editor or devtools |

**Saving is the test.** If a component belongs in the level file, it is
engine or gameplay data. If saving it would be a bug, it is runtime or editor
state.

The editor shouldn't hard-code the game's components. Game logic registers
each component with the engine (its scene loader and saver, and a description
of its fields). Loaders are there today: `RegisterGameLogic` registers
`Player` and its loader through `Engine::RegisterSceneLoader`, and the editor
will call it too. A generic inspector then shows a gameplay component it has
never heard of.

## Playing in the editor vs the game

Both run the same engine and the same gameplay systems on the same scene, so
a body falls identically in either. What differs:

| | Play in the editor | `game` executable |
|---|---|---|
| Starting state | The editor's in-memory scene, **unsaved edits included** | The scene file on disk |
| Flow | Starts in the open level | Full flow: boot, level, and later menus |
| Input | Qt events; the viewport needs focus, editor shortcuts can steal keys | SDL events, mouse capture |
| Overlays | Gizmos, selection, trigger volumes; pause, step, inspect | None (devtools in dev builds) |
| On exit | Restores the pre-play scene | Process exits |

The standalone run catches what playing in the editor can't: fields that were
never saved, and host-specific input bugs. The editor's "launch game" button
saves, then starts `game --scene <path>`.

## Content pipeline

```
 Blender or a vendored pack (glTF, PNG)                   owns geometry and looks
        │ export
        ▼
 assets/…/level01.glb, textures/*.png                     (git)
        │ build: geometryc, texturec, later an importer
        ▼
 build/…/meshes/level01.bin, textures/*.dds,
         levels/level01.env.json   (generated, never hand-edited)
        │ referenced by
        ▼
 assets/scenes/level01.json                               owned by the editor
        │ copied beside the executable at build time
        ▼
 the engine loads both, in the game and in the editor
```

- **Two files per level.** The environment file is regenerated on every
  export; the scene file holds what the editor placed and tuned.
  Re-exporting never wipes editor work.
- **The editor saves into the source tree** (`assets/scenes/`). The build
  copies scenes beside the executable, so a save into the build directory
  would be overwritten and never reach git.
- **The static shell, first version:** one glTF exported from Blender,
  compiled by `geometryc` (which reads glTF through the vendored `cgltf` and
  flattens its nodes), and loaded as one static entity with a triangle-mesh
  collider.
- **The static shell, later:** an importer (on the vendored `cgltf`) writes
  one entity per glTF node into the environment file. Nodes named `*-col`
  become collision-only proxies, and glTF custom properties set physics
  surfaces.
- **In the editor,** the environment is visible and pickable (to click on it,
  or to drop objects onto it) but locked.

## Materials

Two different things share the name:

| | Render material | Physics surface |
|---|---|---|
| Holds | Shader, albedo texture, tint, tiling | Friction, restitution, later density and damping |
| Today | Inline on `Renderable` | Inline on `Collider` (`Material` in `physics/shape.h`) |
| Planned | An asset file referenced by path, loaded once by `AssetRegistry` | An asset file referenced by path; `Collider` can still override inline |
| Created by | Hand-written files, or the importer from glTF materials | The editor |
| Assigned by | The editor (choosing from a list) or the environment file | The editor |
