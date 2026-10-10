# 0010: glTF as the runtime asset format

**Status:** accepted

## Context

Levels are built in Blender, often from CC0 kits, and must reach the engine
with their materials and textures. Meshes went through bgfx's `geometryc` into
its `.bin` format, loaded by bgfx's example helper `meshLoad`. That path can't
carry a textured, multi-material level:

- `geometryc`'s glTF path never records material names, and `meshLoad`
  discards the names the format does store, so one mesh can't have several
  materials;
- it compiles geometry only: materials and textures would need a second
  reader of the same glTF;
- it flattens the node hierarchy and mesh sharing, and splits groups at 65k
  vertices (16-bit indices);
- reading `.bin` ourselves means bgfx's private vertex layout serialization.

## Decision

Load glTF (`.glb`) directly at runtime. There is no import step and no mesh
format of our own.

- The vendored `cgltf` reads the file; its types stay inside the reader.
- The loader is split: glTF → `MeshData` on the CPU (no bgfx), then
  `MeshData` → GPU `Mesh`. Collision meshes come from the same parse, and
  `asset_tests` loads every asset so a bad one fails `ctest`, not the game.
- Shaders take glTF's attributes as authored (float normals, `TEXCOORD_0`),
  uploaded as separate vertex streams with 32-bit indices.
- Base color textures (PNG/JPEG) are decoded with bimg and mipmapped with
  `bimg::imageGenerateMips` at load.
- Render materials live only in the glTF: Blender owns visuals, and the
  editor never assigns them.
- Blender sits in front of every asset, so the loader assumes a clean export
  (metres, +Y up, triangles with normals, base color materials) and throws
  on anything else.

## Alternatives considered

- **Our own mesh format, written by an importer.** What mature engines do:
  loading is a copy to the GPU, and errors surface at build time. But it
  means a format to version, a writer, a reader and a build tool. glTF
  already holds GPU-ready buffers, and the asset test catches bad files
  before running.
- **Keep geometryc and `.bin`.** It needs a patch to vendored geometryc for
  material names, our own `.bin` loader on bgfx internals, and cgltf anyway
  for materials and textures: more code than reading glTF once.
- **Textures through `texturec` (DDS, mips at build time).** Faster loads and
  GPU compression, but an extraction step and generated files. Decoding a
  dozen 1K textures at load costs well under a second.
- **Assimp.** Reads 40+ formats, but it's heavy and its RTTI needs would have
  to be checked against ADR 0002. Blender converting to glTF covers the same
  need.

## Consequences

- Every launch parses glTF and decodes textures. That's fine at this scale.
- Textures sit in VRAM uncompressed (RGBA8), about 4× BC7. Mips are averaged
  in sRGB space, so they're slightly dark.
- geometryc, `.bin` meshes, the DDS texture list and the debug grid's
  triplanar mapping go. The `.obj` primitives become `.glb`.
- A level loads flattened at first, as one mesh with node transforms baked
  in. Per-node entities sharing meshes, and instancing, come later.
- `bgfx_utils` remains only for `loadProgram`.

## Revisit when

- Load time passes about a second, or VRAM gets tight: run `gltfpack`
  offline (quantized meshes, meshopt compression, KTX2/Basis textures) and
  teach the loader those standard extensions.
- Offline processing appears that glTF can't hold (LODs, pre-built Jolt
  shapes): only then consider serializing `MeshData`.
- Assets must come from formats Blender can't convert to glTF.
