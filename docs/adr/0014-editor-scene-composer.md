# 0014: The editor composes scenes; Blender makes assets

**Status:** accepted 2026-10-14; supersedes the editor-scope bullet of
[0007](0007-engine-game-editor-split.md)

## Context

[0007](0007-engine-game-editor-split.md) made the editor "a physics lab, not
a map creator" that "authors only physics data". The rule was meant to keep
the editor from growing into a second map editor, since Blender already owns
geometry.

It draws the line by the kind of data, and that line cuts through the scene
file the editor owns. Scenes already hold the sun, ambient colours,
`Renderable` tints and the player, none of which the rule lets the editor
touch. It also rules out everyday level work that no other tool does:
spawn points, triggers that act on gameplay, lighting judged in the engine's
own renderer, and tuning gameplay values. The generic inspector planned for
M5 would show gameplay components anyway.

## Decision

- **The boundary is the file, not the kind of data.** Blender owns
  everything inside a `.glb`: geometry, UVs, render materials, textures and
  the static shell. The editor owns the scene file: which assets are placed
  where, and every component value on every entity, whether physics,
  gameplay, lighting or cameras.
- **A feature belongs in the editor only if it passes two tests:**
  1. Its output is editor-owned data: the scene file, or another file the
     editor owns (the surface library, later prefabs). Anything that would
     change a `.glb` or create vertices or pixels belongs to Blender.
  2. The engine or the game already reads it. The editor exposes components
     that exist; it never invents data no runtime code reads.
- **The physics lab stays the editor's main strength** (M5). It is no
  longer the editor's only job.
- **The level's static shell stays locked** in the editor: visible and
  pickable, never edited.

## Alternatives considered

- **Keep the physics-only rule.** Rejected: it forbids editing data already
  in the scene file, and pushes gameplay setup into hand-written JSON.
- **A full map editor (brushes, CSG, terrain).** Rejected: Blender already
  does this better, and it would make the editor a much larger product than
  the project needs.
- **Keep the rule and list exceptions.** Rejected: each new need would add
  one, and the rule would end up as nothing but its exceptions.

## Consequences

- `objectives.md`'s editor scope is rewritten around the two tests. Its
  in-scope list names kinds of work; each one arrives only once the engine
  or game has the component it edits (test 2), so none is scheduled by this
  record.
- Lighting is tuned in the editor's viewport, which renders through the same
  engine as the game, so what you see is what the game shows.
- Blockout with primitive props is composition and allowed. Merging or
  cutting them would be CSG, which stays out.
- The editor may own more files than the scene (surface library, prefabs),
  as long as each passes test 2.

## Revisit when

- A feature can only be built by writing into a `.glb`: that work belongs in
  Blender, or in an export step from it.
- A game wants player-made maps: a modding tool is a separate product
  decision, not a widening of this one.
- The second test starts blocking work, for example the editor needs data
  before the game reads it, to prototype a feature.
