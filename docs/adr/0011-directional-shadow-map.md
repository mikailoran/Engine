# 0011: One stable shadow map for the sun

**Status:** accepted

## Context

The sun lit every surface it faced, with nothing in the way, so props seemed
to float and the levels read flat. Levels are a few tens of metres across
and lit by one `DirectionalLight`. Shadows had to work for every `Renderable`
without per-scene setup, and leave room for cascades and softer filtering
later without a rewrite.

## Decision

A single 2048² depth map for the `DirectionalLight`, fitted to the camera
each frame.

- `ShadowSystem` draws every `Renderable`'s positions, one draw per mesh,
  into `kShadowView`. View ids live in `render_views.h`, because bgfx renders
  views in id order and the map must be drawn before the scene samples it.
- `FitDirectionalShadow` (`engine_math`, tested without bgfx) bounds the
  camera's first 40 m with a sphere, so the light volume's size depends only
  on the lens, and snaps its position to whole texels in a light frame
  without translation. Shadows neither resize when the camera turns nor
  shimmer when it moves. The volume reaches 50 m further toward the sun, so
  casters outside the view still land.
- Casters draw without culling: level meshes are one-sided, and culling
  either face would lose some of their shadows. A normal offset and a small
  depth bias keep surfaces from shadowing themselves.
- Receivers sample with hardware depth compare (`SAMPLER2DSHADOW`), 3×3 taps
  of 2×2-filtered PCF. All of it sits in `shadowVisibility` in `shadow.sh`,
  computed in the fragment shader from world position and normal; a new
  filter replaces that one function. Shadows dim the sun's diffuse and
  specular, not the ambient.
- A backend without compare samplers is a fatal error, like a shader that
  fails to link.

## Alternatives considered

- **Cascaded shadow maps now.** The standard for open outdoor scenes, but
  three or four maps, cascade selection and blending, for levels that fit in
  40 m. The fitting already takes a slice, so cascades are this fit run per
  slice.
- **A fixed box around the player or the world origin.** Simpler still, but
  it either wastes resolution on what's behind the camera or crawls as the
  box follows the player.
- **Fitting the camera's frustum corners directly.** Tighter, so sharper,
  but the box changes size as the camera turns, which reads as swimming
  shadows. Stable fitting wins at this resolution.
- **Shadow volumes, or baked lightmaps.** Volumes need closed meshes and
  silhouette extraction; baking can't follow moving props or the player.
- **Packing depth into a color target** for backends without compare
  samplers. Vulkan and OpenGL, the targets, have them.

## Consequences

- Nothing past 40 m is shadowed, and shadows stop abruptly at that edge.
- One texel is about 4.6 cm at 16:9 and 60°, so shadow edges are soft and
  thin objects lose detail.
- Every mesh is drawn twice per frame, with no culling of either pass.
- Only shaders that include `shadow.sh` receive shadows; a custom
  `Renderable::program` still casts.
- `bgfx_compile_shaders` doesn't track includes, so CMake lists `shadow.sh`
  as a dependency of `fs_mesh` by hand.

## Revisit when

- Levels outgrow 40 m, or near shadows look too blurry: cascades, from the
  same fit per slice.
- Scenes need different shadow settings: move distance, bias and an on/off
  switch into `DirectionalLight`.
- Some props shouldn't cast (foliage, decals): a flag on `Renderable`.
- Draw counts start to matter: cull casters against the light volume
  (`bx::buildFrustumPlanes` takes the light's view-projection as is).
- Point or spot lights arrive: they need their own maps (cube or
  perspective), which this design doesn't cover.
