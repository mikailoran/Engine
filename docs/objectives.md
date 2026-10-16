# Objectives

What this project is for, what it will and won't become, and the order of the
work. The project grew into an engine, a game and the start of an editor
before any of that was written down. This page is the reference for judging
whether a feature belongs.

How the pieces fit together is in [architecture.md](architecture.md). The
decisions behind them are in [`adr/`](adr/README.md).

## Purpose

One codebase, three products, with clean boundaries between them:

1. **A hand-written 3D engine**: bgfx rendering, its own ECS, Jolt physics.
2. **A small first-person game** built on the engine.
3. **A Qt editor** that composes the game's scenes: placing, configuring and
   tuning what Blender made.

The editor is also where the project builds real Qt experience: model/view,
widgets, undo, embedding a native viewport. When choosing between editor
features, prefer the ones that exercise Qt properly over plumbing.

The editor is more than the in-game devtools redone in Qt. Porting their
tools is where it starts. Undo, constraints, surfaces and live simulation
are what make it worth building
([ADR 0013](adr/0013-devtools-console.md)).

## The products

| Product | What it is | Owns |
|---|---|---|
| **Engine** | A library. Knows nothing about Qt, SDL, gameplay or the editor | Rendering, ECS, physics, assets, scene loading and saving, picking |
| **Game** | A kinematic first-person character exploring a map full of physics objects, hosted in an SDL3 window | The main loop, input mapping, character and camera behaviour |
| **Editor** | A Qt Widgets application ([ADR 0012](adr/0012-qt-widgets-editor.md)): a **scene composer** with a physics lab at its core, not a map creator ([ADR 0014](adr/0014-editor-scene-composer.md)) | The scene file: placement, physics, gameplay setup, lighting and tuning |
| **Devtools** | An in-game developer console, like Fallout's `` ` `` menu: typed commands such as spawning items. Today an ImGui overlay (inspector, free camera, click-select) | Commands on the running game. Its inspector, free camera and selection move to the editor ([ADR 0013](adr/0013-devtools-console.md)) |

The gameplay direction stops at "first-person exploration of a physical
world" on purpose. It is generic enough to grow into a specific game later,
and specific enough to drive the engine work.

## Scope

### Editor: a scene composer, not a map creator

The rule: **the editor composes scenes; Blender makes assets**
([ADR 0014](adr/0014-editor-scene-composer.md)). Blender owns everything
inside a `.glb`. The editor owns the scene file: which assets are placed
where, and every component value on every entity.

A feature belongs in the editor only if it passes both tests:

1. **Its output is editor-owned data:** the scene file, or another file the
   editor owns (the surface library, later prefabs). Anything that would
   change a `.glb` or create vertices or pixels belongs to Blender.
2. **The engine or the game already reads it.** The editor exposes
   components that exist, and never invents data that no runtime code reads.

The in-scope column lists kinds of work, not a schedule: each item arrives
once the engine or game has the component it edits.

| In scope | Out of scope |
|---|---|
| Place, move, rotate, scale and duplicate entities, including new props from existing assets | Modelling geometry: brushes, CSG, extrusion |
| Blockout with primitive props, before the layout is built in Blender | Terrain sculpting, foliage painting |
| Mass, friction, restitution, damping, gravity, motion type | Texture painting, UVs, material graphs, shader editing |
| Constraints (hinge, slider, distance, fixed) with limits and motors | Light baking, navmesh generation |
| Sensors and trigger volumes, and what they trigger: kill zones, jump pads, teleporters | Asset libraries with import pipelines |
| Gameplay markers: spawn points, objectives, pickups, waypoints | Animation authoring, visual scripting |
| Character and gameplay tuning: movement, jumping, any component the game registers | Creating or assigning render materials |
| Choosing, creating and tuning physics surfaces ("ice", "rubber") | Editing the level's static shell |
| Lighting and atmosphere: sun, ambient, sky, shadows | Shipping the editor as a modding tool |
| Per-instance tint (`Renderable::color`) | |
| Authored cameras: spectator points, menu backdrops, intro shots | |
| Simulate, pause, step and inspect live values | |
| Diagnostics: jump-arc preview, spawn sight-lines, collider vs render mismatch | |
| Engine debug views: shadow map, normals, draw and body counts | |
| Prefabs: reusable entity templates, once objects repeat | |

Blockout is composition: placing scaled primitives. Merging or cutting them
would be CSG, which stays out.

### Assets

Levels and models come from CC0 packs or our own work, always through
Blender, and reach the engine as glTF (`.glb`). The engine loads glTF
directly at runtime: there is no mesh format of our own and no import step
([ADR 0010](adr/0010-gltf-runtime-format.md)).

### Content ownership

| Content | Owned by | Changed by |
|---|---|---|
| The level's static shell (geometry, its materials) | Blender, built from CC0 packs or our own models | Re-export, then rebuild |
| Render materials, textures | Blender, inside each glTF | Editing in Blender, then re-export |
| The scene: placed objects, physics, constraints, gameplay setup, lighting, cameras | The editor | The editor, saved to the scene file |
| Prefabs, later physics surfaces | The editor | The editor, saved to their own files; prefabs are hand-written JSON until then |

Each level is two files: its **glTF**, exported from Blender, and a **scene**
file the editor owns, which places the level and everything on it.
Re-exporting the shell never touches the editor's work. See
[architecture.md](architecture.md#content-pipeline).

### Platform

Linux only, Wayland first with an X11 fallback, single-config CMake
generators. Nothing is planned beyond that.

## Milestones

Each milestone ends with something you can run and show.

| # | Milestone | Done when | Status |
|---|---|---|---|
| M0 | **Objectives and architecture written down** | This page, [architecture.md](architecture.md) and their ADRs exist | Done |
| M1 | **Off bgfx's example harness.** Engine `Input` type, a highlight mechanism instead of `Selected` in the renderer, an `engine`/`devtools`/`game` directory split, an `Engine` class driven by its host, an SDL3 host with a real `main()` | `game` runs natively on Wayland with no `entry` code linked, and behaves as it does today | Done |
| M2 | **First-person exploration.** A kinematic character controller (Jolt `CharacterVirtual` behind the facade), a first-person camera, a triangle-mesh collider, one level shell exported from Blender | You can walk around a Blender-made level and push physics props | Done |
| M2a | **Level shell.** Rigid bodies in scene files, triangle-mesh colliders, glTF meshes through the build, mesh colliders loaded from files | A glTF shell built with the project renders, and props dropped on it rest on its mesh collider | Done |
| M2b | **Kinematic character.** `CharacterVirtual` behind the facade and a `CharacterBody` component synced like bodies | Tests show the character standing, sliding along walls, respecting a slope limit, jumping and pushing a ball; in game, a capsule given a velocity pushes props | Done |
| M2c | **First-person play.** Relative mouse input, the game-logic library, play and debug modes, the Blender level | M2's own goal: walk, sprint and jump around the Blender level and push props | Done |
| M3 | **Scene authoring foundation.** glTF loaded at runtime with its materials and textures, a level as its glTF plus a scene file | You walk around a textured level built in Blender from a CC0 kit; a level is its glTF plus a scene file | Done |
| M3a | **glTF meshes.** A runtime glTF loader replacing geometryc and bgfx's `.bin`, multi-material meshes, a gather-then-submit renderer, an asset test | `game` plays as before from `.glb` files, and `asset_tests` loads every asset | Done |
| M3b | **glTF materials and textures.** Base color factor and texture, decoded and mipmapped at load, UV-mapped shading | M3's own goal: walk around a textured level built from Kenney's Prototype Kit | Done |
| M3c | **Animated characters.** Skinned glTF and CC0 clip packs retargeted by joint name, sampled and blended with ozz-animation (ADR 0016); layered, masked playback; meshes attached to joints; a per-bone ragdoll as hitboxes and death ragdoll (ADR 0017) | An enemy on the Universal Animation Library chases the player, aims and fires a pistol, and falls as a ragdoll when shot | In review |
| M4 | **Qt editor MVP.** Viewport, entity hierarchy, `Transform` inspector, picking, a simulate/pause toggle, scene saving | Move a crate in the editor, save, and `game` shows it there | |
| M4a | **Qt viewport.** The `editor` executable, the engine rendering into an embedded Qt viewport, a navigable camera | The editor opens a level and you can look around it | |
| M4b | **Select and edit.** A `Name` component from the scene file, an entity hierarchy over the ECS (by `Name`), viewport picking kept in sync with it, a `Transform` inspector | Select a crate in the viewport or the hierarchy, and move it with the inspector | |
| M4c | **Simulate.** A simulate/pause toggle that steps physics in the editor | Props fall and settle while simulating, and freeze on pause | |
| M4d | **Scene saving.** Writing the editor-owned scene file, a saver beside each component loader, prefab instances written as their prefab and overrides | M4's own goal; and load, save and load again gives an identical scene | |
| M5 | **Physics lab.** Undo/redo, a generic inspector driven by component registration, constraints, a physics-surface library (surface assets referenced by colliders, with inline overrides), play/stop with restore, physics debug overlays, telemetry plots, a "launch game" button | You can build a hinged door or a pendulum and tune it without touching JSON | |

M2, M3 and M4 are split into lettered parts, each on its own branch, so each
part is reviewed and merged on its own. The editor deliberately comes after M2 and M3. Its features should come from
real authoring needs, and it needs the engine API those milestones create.
Scene saving waits for M4d for the same reason: the editor is its first
user. Saving after simulating writes the simulated poses; restoring the
pre-simulation state is M5's play/stop. `Name` and physics-surface assets
moved out of M3 the same way: the editor's hierarchy (M4b) and surface
library (M5) are their first users.

## Open questions

Settled questions move to an ADR: Qt Widgets over QML
([0012](adr/0012-qt-widgets-editor.md)), and the devtools after M4
([0013](adr/0013-devtools-console.md)).

- **A specific game.** Deferred until exploration (M2) works.
- **Physics surfaces on parts of the static shell.** The editor picks
  surfaces, and Blender owns only visuals. One surface for the whole shell is
  easy. An ice patch on one floor needs a way to address part of the shell:
  per-node entities, or glTF node names. Decide when per-node entities
  arrive.
