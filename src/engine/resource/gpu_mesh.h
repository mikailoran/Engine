#pragma once

#include <bgfx/bgfx.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <vector>

#include "engine/resource/cpu_mesh.h"
#include "engine/resource/unique_handle.h"

namespace engine {

/**
 * @brief A mesh's GPU buffers, shared by every entity that draws it. Made by
 * UploadMesh from a CpuMesh.
 *
 * One vertex stream per attribute, bound as streams 0 (position), 1 (normal)
 * and 2 (UV), and 32-bit indices. Must be destroyed before bgfx::shutdown.
 */
struct GpuMesh {
  UniqueHandle<bgfx::VertexBufferHandle> positions;
  UniqueHandle<bgfx::VertexBufferHandle> normals;
  UniqueHandle<bgfx::VertexBufferHandle> uvs;
  UniqueHandle<bgfx::IndexBufferHandle> indices;
  std::vector<Submesh> submeshes;
  /// Box around every submesh, in mesh space.
  bx::Aabb bounds{.min = bx::InitZero, .max = bx::InitZero};
};

/**
 * @brief Uploads a CPU mesh's vertex streams and indices to the GPU.
 *
 * Requires bgfx::init. The returned mesh owns its buffers, so it must be
 * destroyed before bgfx::shutdown.
 */
auto UploadMesh(const CpuMesh& mesh) -> GpuMesh;

}  // namespace engine
