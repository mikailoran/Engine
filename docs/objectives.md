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
| **Choosing** existing render materials for placed objects | **Creating** render materials |
| Creating and tuning physics surfaces ("ice", "rubber") | Editing the level's static shell |

The rule: **the editor may reference any asset, but it only authors physics
data.**

### Content ownership

| Content | Owned by | Changed by |
|---|---|---|
| The level's static shell (geometry, its looks) | Blender or a vendored pack | Re-export, then rebuild |
| Render materials, textures | External tools and hand-written asset files | Editing those files |
| Placed objects, constraints, physics surfaces | The editor | The editor, saved to the scene file |

Each level is two files: an **environment** file generated from the export,
and a **scene** file the editor owns. Re-exporting the shell never touches the
editor's work. See [architecture.md](architecture.md#content-pipeline).

### Platform

Linux only, Wayland first with an X11 fallback, single-config CMake
generators. Nothing is planned beyond that.

## Milestones

Each milestone ends with something you can run and show.

| # | Milestone | Done when | Status |
|---|---|---|---|
| M0 | **Objectives and architecture written down** | This page, [architecture.md](architecture.md) and their ADRs exist | Done |
| M1 | **Off bgfx's example harness.** Engine `Input` type, a highlight mechanism instead of `Selected` in the renderer, an `engine`/`devtools`/`game` directory split, an `Engine` class driven by its host, an SDL3 host with a real `main()` | `game` runs natively on Wayland with no `entry` code linked, and behaves as it does today | Done |
| M2 | **First-person exploration.** A kinematic character controller (Jolt `CharacterVirtual` behind the facade), a first-person camera, a triangle-mesh collider, one level shell exported from Blender | You can walk around a Blender-made level and push physics props | In progress |
| M2a | **Level shell.** Rigid bodies in scene files, triangle-mesh colliders, glTF meshes through the build, mesh colliders loaded from files | A glTF shell built with the project renders, and props dropped on it rest on its mesh collider | Done |
| M2b | **Kinematic character.** `CharacterVirtual` behind the facade and a `CharacterBody` component synced like bodies | Tests show the character standing, sliding along walls, respecting a slope limit, jumping and pushing a box; in game, a capsule given a velocity pushes props | |
| M2c | **First-person play.** Relative mouse input, the game-logic library, play and debug modes, the Blender level | M2's own goal: walk, sprint and jump around the Blender level and push props | |
| M3 | **Scene authoring foundation.** A `Name` component, scene saving, separate environment and scene files, material and physics-surface assets, multi-material meshes | Load, save and load again gives an identical scene; a level is an environment file plus a scene file | |
| M4 | **Qt editor MVP.** Viewport, entity hierarchy, `Transform` inspector, picking, a simulate/pause toggle | Move a crate in the editor, save, and `game` shows it there | |
| M5 | **Physics lab.** Undo/redo, a generic inspector driven by component registration, constraints, a physics-surface library, play/stop with restore, physics debug overlays, telemetry plots, a "launch game" button | You can build a hinged door or a pendulum and tune it without touching JSON | |

M2 is split into M2a to M2c, each on its own branch, so each part is reviewed
and merged on its own. The editor deliberately comes after M2 and M3. Its features should come from
real authoring needs, and it needs the engine API those milestones create.

## Open questions

- **Qt Widgets or QML?** Widgets is the usual choice for editors and tools;
  QML is the usual choice for embedded and HMI work. Decide before M4, based
  on which skills matter more.
- **A specific game.** Deferred until exploration (M2) works.
- **Physics surfaces on the static shell.** Set them in Blender (custom
  properties read on import) or as an editor-owned override layer? Blender
  for now; revisit if it gets in the way during M5.
- **The devtools after M4.** Keep the ImGui overlay as a runtime debugger, or
  retire the parts the editor replaces.
