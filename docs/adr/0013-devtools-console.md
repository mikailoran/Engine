# 0013: The editor takes over the devtools' tools; the devtools become a console

**Status:** accepted 2026-10-12, not implemented

## Context

The devtools (`src/devtools/`) are the ImGui overlay in the game, behind
F1's debug mode: a component inspector, a fly camera, click-select with
highlights, and a debug-draw toggle. Most of these are level-editor
features, and the Qt editor's M4 builds the same things: a navigable camera
(M4a), selection and a `Transform` inspector (M4b).

If both are kept, every component needs two inspectors and the game has two
selection models. `objectives.md` left this open: keep the overlay as a
runtime debugger, or retire the parts the editor replaces.

## Decision

- **The editor takes over the devtools' authoring and inspection features:**
  the inspector, the free camera, click-select and the selection highlight,
  and the debug-draw toggle. Each one leaves the devtools once the editor has
  its replacement.
- **The devtools become an in-game developer console,** like the one in
  Bethesda's games (the `` ` `` key in Fallout): a prompt where you type
  commands that act on the running game, such as spawning an item.
- **The editor is more than the devtools redone in Qt.** Porting them is
  where it starts. It earns its place with what the overlay never had: undo,
  a model/view hierarchy, a generic inspector, constraints, a surface
  library, play/stop with restore and telemetry (M5).

## Alternatives considered

- **Keep the devtools as they are, next to the editor.** Rejected: two
  inspectors and two selection paths to maintain for every component, and
  the overlay's tools are level editing, which belongs in the editor.
- **Retire the devtools entirely.** Rejected: the standalone game still
  needs a way to act on itself while playing, where the editor isn't
  running.
- **An editor that only ports the devtools to Qt.** Rejected: the same tools
  in a heavier framework don't justify a second UI framework
  ([0007](0007-engine-game-editor-split.md)).

## Consequences

- F1's debug mode, `FlyCameraSystem`, `UpdateSelection` and `Selected`
  leave the launcher as the editor replaces them. The launcher gets a
  console toggle instead.
- Authoring stays in the editor: the console acts on the running game and
  never writes the scene file.
- The devtools keep their layer rule (Engine and ImGui, never Qt or SDL).
  The console's window and text input are ImGui's.
- Spawning needs an engine mechanism to create a described object at
  runtime; today only whole scenes load. Commands are policy and live in the
  devtools or the game.
- Which commands exist beyond spawning is decided one at a time. A free
  camera, for example, could come back as a command.
- Not scheduled in the milestones yet.

## Revisit when

- A command wants to save to the scene file: that work belongs in the
  editor.
- Playtesting the standalone game needs live inspection the console can't
  give: add a read-only overlay rather than bring the inspector back.
