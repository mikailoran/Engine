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
3. **A Qt editor** for setting up the game's physical world.

The editor is also where the project builds real Qt experience: model/view,
widgets, undo, embedding a native viewport. When choosing between editor
features, prefer the ones that exercise Qt properly over plumbing.

## The products

| Product | What it is | Owns |
|---|---|---|
| **Engine** | A library. Knows nothing about Qt, SDL, gameplay or the editor | Rendering, ECS, physics, assets, scene loading and saving, picking |
| **Game** | A kinematic first-person character exploring a map full of physics objects, hosted in an SDL3 window | The main loop, input mapping, character and camera behaviour |
| **Editor** | A Qt application: a **physics lab**, not a map creator | Arranging and tuning physical objects, constraints and surfaces |
| **Devtools** | The ImGui overlay inside the game (inspector, free camera, click-select) | Runtime debugging; authoring moves to the editor once it exists |

The gameplay direction stops at "first-person exploration of a physical
world" on purpose. It is generic enough to grow into a specific game later,
and specific enough to drive the engine work.

## Scope

### Editor: a physics lab, not a map creator

The editor **arranges and tunes things that already exist. It never creates
geometry.**

| In scope | Out of scope |
|---|---|
| Move, rotate, scale and duplicate entities | Modelling geometry: brushes, CSG, extrusion |
| Mass, friction, restitution, damping, gravity, motion type | Terrain sculpting, foliage painting |
| Constraints (hinge, slider, distance, fixed) with limits and motors | Texture painting, UVs, material graphs |
| Sensors and trigger volumes | Light baking, navmesh generation |
| Character tuning: speed, step height, max slope, push strength | Asset libraries with import pipelines |
| Simulate, pause, step and inspect live values | Shipping the editor as a modding tool |
| Choosing the physics surface of a placed object or the level | Anything visual: creating, choosing or assigning render materials |
| Creating and tuning physics surfaces ("ice", "rubber") | Editing the level's static shell |

The rule: **the editor may reference any asset, but it only authors physics
data.** Blender owns everything visual.

### Assets

Levels and models come from CC0 packs or our own work, always through
Blender, and reach the engine as glTF (`.glb`). The engine loads glTF
directly at runtime: there is no mesh format of our own and no import step
([ADR 0010](adr/0010-gltf-runtime-format.md)).

### Content ownership

| Content | Owned by | Changed by |
|---|---|---|
| The level's static shell (geometry, its looks) | Blender, built from CC0 packs or our own models | Re-export, then rebuild |
| Render materials, textures | Blender, inside each glTF | Editing in Blender, then re-export |
| Placed objects, constraints, physics surfaces | The editor | The editor, saved to the scene file |

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
| M3 | **Scene authoring foundation.** glTF loaded at runtime with its materials and textures, a level as its glTF plus a scene file, a `Name` component, physics-surface assets | You walk around a textured level built in Blender from a CC0 kit; a level is its glTF plus a scene file | |
| M3a | **glTF meshes.** A runtime glTF loader replacing geometryc and bgfx's `.bin`, multi-material meshes, a gather-then-submit renderer, an asset test | `game` plays as before from `.glb` files, and `asset_tests` loads every asset | Done |
| M3b | **glTF materials and textures.** Base color factor and texture, decoded and mipmapped at load, UV-mapped shading | M3's own goal: walk around a textured level built from Kenney's Prototype Kit | |
| M3c | **Names.** A `Name` component from the scene file, shown in the devtools | Selecting an entity shows its scene name | |
| M3d | **Physics surfaces.** Surface assets referenced by colliders, with inline overrides | A crate on an "ice" surface slides visibly further | |
| M4 | **Qt editor MVP.** Viewport, entity hierarchy, `Transform` inspector, picking, a simulate/pause toggle, scene saving | Move a crate in the editor, save, and `game` shows it there | |
| M4a | **Qt viewport.** The `editor` executable, the engine rendering into an embedded Qt viewport, a navigable camera | The editor opens a level and you can look around it | |
| M4b | **Select and edit.** An entity hierarchy over the ECS (by `Name`), viewport picking kept in sync with it, a `Transform` inspector | Select a crate in the viewport or the hierarchy, and move it with the inspector | |
| M4c | **Simulate.** A simulate/pause toggle that steps physics in the editor | Props fall and settle while simulating, and freeze on pause | |
| M4d | **Scene saving.** Writing the editor-owned scene file, a saver beside each component loader | M4's own goal; and load, save and load again gives an identical scene | |
| M5 | **Physics lab.** Undo/redo, a generic inspector driven by component registration, constraints, a physics-surface library, play/stop with restore, physics debug overlays, telemetry plots, a "launch game" button | You can build a hinged door or a pendulum and tune it without touching JSON | |

M2, M3 and M4 are split into lettered parts, each on its own branch, so each
part is reviewed and merged on its own. The editor deliberately comes after M2 and M3. Its features should come from
real authoring needs, and it needs the engine API those milestones create.
Scene saving waits for M4d for the same reason: the editor is its first
user. Saving after simulating writes the simulated poses; restoring the
pre-simulation state is M5's play/stop.

## Open questions

- **Qt Widgets or QML?** Widgets is the usual choice for editors and tools;
  QML is the usual choice for embedded and HMI work. Decide before M4a, based
  on which skills matter more.
- **A specific game.** Deferred until exploration (M2) works.
- **Physics surfaces on parts of the static shell.** The editor picks
  surfaces, and Blender owns only visuals. One surface for the whole shell is
  easy. An ice patch on one floor needs a way to address part of the shell:
  per-node entities, or glTF node names. Decide when per-node entities
  arrive.
- **The devtools after M4.** Keep the ImGui overlay as a runtime debugger, or
  retire the parts the editor replaces.
