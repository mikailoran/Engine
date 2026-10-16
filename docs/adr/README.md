# Architecture decision records

Short notes on decisions that shaped the engine: what we chose, what we turned
down, and **what would make us reconsider**. The code shows what was built;
these explain why it isn't built some other way.

| # | Decision |
|---|---|
| [0001](0001-jolt-physics.md) | Use Jolt Physics |
| [0002](0002-no-rtti.md) | Build the whole project without RTTI |
| [0003](0003-physics-behind-a-facade.md) | Hide Jolt behind a `PhysicsWorld` facade |
| [0004](0004-sync-by-comparing.md) | Sync the ECS and physics by comparing, not with ECS hooks |
| [0005](0005-quaternion-rotations.md) | Store rotations as quaternions |
| [0006](0006-sdl3-windowing.md) | Own windowing and input with SDL3 |
| [0007](0007-engine-game-editor-split.md) | Split the engine from its hosts, and the editor from the game |
| [0008](0008-right-handed-frame.md) | Use a right-handed, +Y up coordinate frame |
| [0009](0009-kinematic-character.md) | A kinematic character on Jolt's `CharacterVirtual` |
| [0010](0010-gltf-runtime-format.md) | Load glTF directly at runtime, with no format of our own |
| [0011](0011-directional-shadow-map.md) | Shadow the sun with one stable, camera-fitted shadow map |
| [0012](0012-qt-widgets-editor.md) | Build the editor with Qt Widgets |
| [0013](0013-devtools-console.md) | The editor takes over the devtools' tools; the devtools become a console |
| [0014](0014-editor-scene-composer.md) | The editor composes scenes; Blender makes assets |
| [0015](0015-per-bone-hitboxes.md) | Per-bone convex hitboxes for characters (superseded by 0017) |
| [0016](0016-ozz-animation-runtime.md) | Sample and blend skeletal animation with ozz-animation |
| [0017](0017-ragdoll-hitboxes.md) | One per-bone ragdoll for hitboxes and death |

## Adding one

Copy the shape of an existing record: **Context**, **Decision**,
**Alternatives considered**, **Consequences**, **Revisit when**. Keep it under a
page. Records are not edited once a decision changes; write a new one that
replaces the old, and mark the old one as superseded.
