#pragma once

#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace engine {

/**
 * @brief A run of a mesh's indices that shares one material, drawn in one
 * call. Shared by CpuMesh and GpuMesh.
 */
struct Submesh {
  /// Offset into the mesh's indices where the run starts.
  std::uint32_t first_index{0};
  /// Number of indices in the run, a multiple of 3.
  std::uint32_t index_count{0};
  /// The glTF material's index; empty for primitives without one.
  std::optional<std::uint32_t> material;
  /// Box around the vertices this run uses, in mesh space.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
};

/**
 * @brief A mesh's geometry in RAM, one array per vertex attribute.
 *
 * Made by ReadGltf; needs no bgfx. UploadMesh turns it into a GpuMesh, and
 * AssetRegistry builds collision meshes from its positions and indices.
 */
struct CpuMesh {
  /// In mesh space, in metres: right-handed, +Y up.
  std::vector<bx::Vec3> positions;
  /// Unit length, one per position.
  std::vector<bx::Vec3> normals;
  /// One per position; zeros when the source has no UVs.
  std::vector<std::array<float, 2>> uvs;
  /// Three per triangle, counter-clockwise seen from the front.
  std::vector<std::uint32_t> indices;
  /// Cover indices exactly, in order, one per material.
  std::vector<Submesh> submeshes;
  /// Box around every position.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
};

}  // namespace engine
