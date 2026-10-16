#pragma once

#include <bgfx/bgfx.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <vector>

#include "engine/resource/animation_handles.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/unique_handle.h"

namespace engine {

/**
 * @brief A mesh's GPU buffers and textures, shared by every entity that
 * draws it. Made by UploadMesh from a CpuMesh.
 *
 * One vertex stream per attribute, bound as streams 0 (position), 1 (normal)
 * and 2 (UV), plus 3 (joint indices) and 4 (weights) when skinned, and
 * 32-bit indices. Must be destroyed before bgfx::shutdown.
 */
struct GpuMesh {
  UniqueHandle<bgfx::VertexBufferHandle> positions;
  UniqueHandle<bgfx::VertexBufferHandle> normals;
  UniqueHandle<bgfx::VertexBufferHandle> uvs;
  /// Four 8-bit joint indices per vertex; invalid for a static mesh.
  UniqueHandle<bgfx::VertexBufferHandle> joints;
  /// Four float weights per vertex; invalid for a static mesh.
  UniqueHandle<bgfx::VertexBufferHandle> weights;
  UniqueHandle<bgfx::IndexBufferHandle> indices;
  std::vector<Submesh> submeshes;
  /// Box around every submesh, in mesh space.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
  /// Indexed by Submesh::material.
  std::vector<MeshMaterial> materials;
  /// Indexed by MeshMaterial's *_image fields: sRGB, mipmapped, repeating.
  std::vector<UniqueHandle<bgfx::TextureHandle>> textures;
  /// The skeleton it is skinned to, set by AssetRegistry; invalid if static.
  RigHandle rig;
};

/**
 * @brief Uploads a CPU mesh's vertex streams, indices and images to the GPU.
 *
 * Requires bgfx::init. The returned mesh owns its buffers, so it must be
 * destroyed before bgfx::shutdown. Its rig is left invalid.
 * @pre Joint indices below 256 (kMaxJoints is less).
 */
auto UploadMesh(const CpuMesh& mesh) -> GpuMesh;

}  // namespace engine
