#pragma once

#include <cgltf/cgltf.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/** @brief Indices gathered for one material while walking the nodes. */
struct MaterialBucket {
  std::optional<std::uint32_t> material;
  std::vector<std::uint32_t> indices;
};

/** @brief Vertices shared by all materials, plus each material's indices. */
struct MeshBuilder {
  CpuMesh mesh;
  /// Index 0 holds primitives without a material, i + 1 material i.
  std::vector<MaterialBucket> buckets;
};

/**
 * @brief Appends the meshes of @p scene's nodes and all their descendants,
 * with node transforms baked in and mirrored nodes rewound.
 *
 * Requires @p builder's buckets sized to the file's materials + 1.
 * @throws std::runtime_error On a primitive that isn't triangles, lacks
 *         positions or normals, or whose attribute counts differ.
 */
void AppendScene(const cgltf_data& data, const cgltf_scene& scene,
                 MeshBuilder& builder);

/**
 * @brief Concatenates the buckets into the mesh's indices, one submesh per
 * non-empty bucket, and computes every bounds. Requires a triangle.
 */
void CutSubmeshes(MeshBuilder& builder);

}  // namespace engine::gltf
