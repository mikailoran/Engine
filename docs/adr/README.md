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

## Adding one

Copy the shape of an existing record: **Context**, **Decision**,
**Alternatives considered**, **Consequences**, **Revisit when**. Keep it under a
page. Records are not edited once a decision changes; write a new one that
replaces the old, and mark the old one as superseded.
