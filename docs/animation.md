# Animation

Characters are skinned glTF meshes posed by clips from CC0 packs. This page
covers how to bring a pack in, how to make an entity play it, and how the
pieces fit together. The decisions behind it are ADRs
[0016](adr/0016-ozz-animation-runtime.md) (the runtime) and
[0017](adr/0017-ragdoll-hitboxes.md) (ragdolls).

---

## Bringing assets in

Packs come as `.gltf` + `.bin` + images, `.glb`, or `.blend`. Only `.glb`
files go into `assets/`; raw downloads stay in the ignored
`assets/downloads/`. Two scripts in `tools/assets/` do the conversion:

```bash
# A rigged character: embed its files, downscale textures for Debug loads
tools/assets/pack_gltf.py Char.gltf assets/meshes/characters/Char.glb --max-texture 1024

# A clip pack: keep only the clips the game plays
tools/assets/pack_gltf.py UAL1_Standard.glb assets/animations/UAL1.glb \
    --keep-animations Idle_Loop Walk_Loop Jog_Fwd_Loop

# A prop modelled in Blender at any scale, front along +X
blender -b Gun.blend --python tools/assets/export_glb.py -- \
    assets/meshes/weapons/Gun.glb --length 0.21 --x-forward
```

- **`pack_gltf.py` never changes a rig.** Nodes, skins and animations are
  copied byte for byte, which matters because clips are matched to meshes by
  joint name and rest pose. Round-tripping a character through Blender can
  change both.
- **`export_glb.py` prints the front-face centre** of what it exported: for a
  gun, its muzzle, which the prefab needs.
- New files must be listed in `CMakeLists.txt` (`ENGINE_GLTF_ASSETS`,
  `ENGINE_RAGDOLLS`) or the build won't copy them beside the executable.

### What a file must look like

- One skin at most, of up to 128 joints (`kMaxJoints`), with every joint in
  TRS form. A skinned file can't also hold static meshes: give a static part
  a joint, or attach it (see [Attached meshes](#attached-meshes)).
- Clips use LINEAR or STEP keys; CUBICSPLINE throws.
- The model faces +Z, glTF's front (what Blender's -Y front exports to).
- Skinned vertices are in bind pose, which must equal the rest pose
  (`asset_tests` checks this for every rig in the repository).

### Using a clip pack on another character

Clips are **retargeted by joint name** when they load (`RetargetClip`).
Rotations carry over; the target keeps its own bone lengths, except the hips,
whose movement is scaled by the ratio of the two hip heights. That's enough
for packs built on the same rig, like Quaternius' Universal Animation
Library and Universal Base Characters, even when proportions differ. It is
not a general retargeter: a rig with different joint names or bone axes
needs a pack made for it.

---

## Making an entity animate

```jsonc
"renderable": { "mesh": "assets/meshes/characters/Superhero_Male.glb" },
"animator": {
  "clips": "assets/animations/UAL1.glb",  // retargeted onto this mesh's rig
  "play": "Idle_Loop"                     // optional: loops on the base layer
}
```

An `Animator` needs a skinned `Renderable`; it gets a `SkinPose` the first
frame and the mesh draws posed from then on. Without an `Animator` a skinned
mesh draws in bind pose.

### Steering it from gameplay

An `Animator` stacks **four layers**. Layer 0 is the base; each later layer
overrides the ones below by its weight, optionally only over one joint's
subtree (its mask). Each layer plays one clip, or two blended at a shared
phase, and crossfades when it changes.

```cpp
const ClipHandle walk = assets.FindClip(animator.clips, "Walk_Loop");
const ClipHandle jog = assets.FindClip(animator.clips, "Jog_Fwd_Loop");
const ClipHandle aim = assets.FindClip(animator.clips, "Pistol_Aim_Neutral");

SetLayer(animator, 1, {.weight = 1.0F, .mask = "spine_01"});  // upper body
PlayBlend(animator, 0, walk, jog, 0.4F);  // legs: 40% of the way to a jog
Play(animator, 1, aim);                   // arms: aiming, over the legs
Play(animator, 2, shot, {.fade_s = 0.05F, .loop = false, .restart = true});
if (IsFinished(animator, 2)) { Stop(animator, 2, 0.15F); }
```

- **Calling `Play` every frame is fine:** it only restarts the clip when it
  changes (or `restart` is set). `PlayBlend` with the same pair just updates
  the blend.
- **Look clips up once** (at load, as the `enemy` loader does) and keep the
  handles; `FindClip` returns an invalid handle for an unknown name.
- **Blend two clips of the same gait at matching speeds.** Both are sampled
  at one phase, so a walk and a jog stay in step whatever their lengths.
- Clips play in place: the character controller moves the entity, and the
  game chooses speeds that match the clips (the enemy's `walk_speed` and
  `run_speed`).

### Attached meshes

Guns, hair and other rigid pieces ride on a joint without being skinned:

```jsonc
"attachments": [
  { "mesh": "assets/meshes/weapons/Pistol_1.glb", "joint": "hand_r",
    "position": [-0.03, 0.07, 0.0], "rotation_deg": [-97.46, -2.3, 0.0] },
  { "mesh": "assets/meshes/characters/hair/Hair_SimpleParted.glb",
    "joint": "Head", "bind_space": true }
]
```

- The offset is in the joint's space. With `bind_space`, the mesh is taken
  to be modelled on the character in its bind pose (Quaternius' "origin at 0"
  hair), and follows the joint by its skinning matrix instead.
- Without a `joint` it rides on the entity itself: the player's view gun is
  attached to the camera this way (`GameSession`).
- `AttachmentMatrix` gives gameplay an attachment's world matrix; the enemy
  fires from its pistol's muzzle with it.
- Attached meshes draw and cast shadows, but have no collider and aren't
  picked.

### Ragdolls

See [physics.md](physics.md#ragdolls): a `Ragdoll` gives the entity capsules
that follow the pose (its hitboxes) and fall limp when the game sets
`Ragdoll::simulate`.

---

## How it fits together

```
 glTF ──ReadGltf / ReadGltfClips──► Skeleton, AnimationClip, CpuMesh joints+weights   (engine_assets)
                                           │ RetargetClip
                                           ▼
          AssetRegistry: Rig per skinned mesh, Clip sets per (file, rig)              (engine_runtime)
                                           │
 Animator ──AnimationSystem──► animation::Evaluator (ozz) ──► SkinPose               (engine_animation)
                                                                │ model matrices, palette
                    RenderSystem / ShadowSystem: vs_*_skinned, u_bones[128]
                    PhysicsSystem::PoseRagdolls, attachments, debug draw
```

- **`engine_assets`** reads glTF into plain types: `Skeleton` (joints in the
  skin's order, so a vertex's joint indices index it directly),
  `AnimationClip` (key tracks per joint) and `CpuMesh::joints`/`weights`. No
  bgfx, no ozz.
- **`engine_animation`** (`src/engine/animation/`) is the facade over
  ozz-animation, linked PRIVATE like Jolt: `Rig` and `Clip` are the runtime
  forms, `Evaluator::Evaluate` samples and blends layers into model-space
  joint matrices, `Palette` turns them into skinning matrices. The pure
  playback rules (phase, loops, fades) are in `playback.h`.
- **`AnimationSystem`** steps every `Animator` and writes its `SkinPose`, in
  `Engine::Update` right after physics (so this frame's velocities chose the
  clips) and before ragdolls are posed and anything draws. A simulated
  ragdoll replaces the clips as the pose's source.
- **The renderer** draws an entity with a `SkinPose` on a skinned mesh with
  `vs_mesh_skinned` (and `vs_shadow_skinned` for the shadow map), uploading
  the palette as `u_bones`. Joint indices (four 8-bit integers) and weights
  share a fourth vertex stream: bgfx allows four.
- Debug draw shows each posed skeleton through its mesh.

---

## Testing

```bash
./build/Debug/tests/asset_tests      # skeletons, weights, clips, retargeting, prefab clip names
./build/Debug/tests/animation_tests  # rigs, evaluation, playback, masks, ragdoll fitting
```

`animation_tests` runs ozz on the real rigs and clips. The most important
test is `Evaluate.RestPaletteSkinsTheBindPose`: it skins every vertex of both
characters with the rest-pose palette and expects the bind pose back, which
breaks on any slip in joint order, matrix layout or the skin's root
transform. `PrefabClips.EveryNamedClipExists` checks the clip names prefabs
use, which would otherwise only fail when the game loads.

---

## Known issues

- Root motion is ignored; only in-place clips play correctly.
- One mesh per skeleton: there are no skinned attachments (hair skinned to a
  head would need the character's palette).
- The enemy's aim doesn't follow pitch (the `Pistol_Aim_Up/Down` clips are
  imported but unused), and clips don't adjust to the ground (no foot IK),
  so feet can float on slopes.
- The skinned shader needs a uniform scale on the entity, as the static one
  does.
