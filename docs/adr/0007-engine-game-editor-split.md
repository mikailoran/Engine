# 0007: Split the engine from its hosts, and the editor from the game

**Status:** accepted, not yet implemented

## Context

The project grew into an engine, a game and the start of an editor in one
executable, without written objectives (now in `docs/objectives.md`):

- `Game` in `main.cpp` owns the window, the loop, every system and the
  authoring tools.
- The ImGui inspector, click-selection and the free camera are authoring
  tools, yet they sit beside the engine's systems.
- `RenderSystem` checks a `Selected` component, so the renderer knows about
  editor selection.

The plan is a Qt editor next to the game. That needs an engine both can host.

## Decision

- **The engine is a library driven by hosts.** It is created from a native
  surface the host provides, and exposes `Update`, `Render`, `EndFrame`,
  `Resize`, scene loading and queries such as `Pick`. Hosts own the window,
  the event loop, the clock and input translation.
- **Two hosts:** the SDL `game` executable and a Qt `editor` executable.
  Gameplay code goes in a library that both link. The in-game ImGui tools
  become `devtools`, used only by the game host.
- **Mechanism in the engine, policy in its users.** The engine offers
  highlights, outlines, debug shapes and picking; selection, hover and gizmos
  belong to the editor and devtools, which request overlays every frame.
- **The editor is a physics lab, not a map creator.** It arranges and tunes
  existing objects and authors only physics data. Geometry and looks come
  from external tools through the content pipeline.
- **Directories match the layers:** `src/engine/`, `src/game/`,
  `src/devtools/`, later `src/editor/`. Includes stay `src/`-rooted.

## Alternatives considered

- **Keep the ImGui inspector as the editor.** Cheapest, and it already works
  in play mode. Rejected: Qt experience is a stated goal, and the ImGui tools
  can't grow into undo, a model/view hierarchy or docking panels without
  becoming a second UI framework.
- **Qt as the only host.** One executable with an edit/play switch. Rejected:
  Qt fights a game loop, and the standalone game is what proves the editor's
  saved data is complete.
- **The editor before any gameplay.** Rejected: without gameplay there is
  nothing to author beyond what the ImGui inspector already does, so the
  editor's features would be guesses.

## Consequences

- `Game` splits into `Engine` and a host; `Game`'s member order
  (bring-up and teardown) moves into `Engine`. Host-owned GPU users, such as
  the ImGui overlay, must be destroyed before the engine.
- Every new feature first asks which layer it belongs to. Most of the answer
  is in `docs/architecture.md`.
- The engine needs an API surface it didn't have: picking, highlight
  requests, and later scene saving and component registration for the
  inspector.

## Revisit when

- A third host appears (a headless server, tests that render): the host API
  may need to grow.
- The editor needs to edit something only the game understands at runtime:
  that is the point to reconsider how far game logic and editor are kept
  apart.
