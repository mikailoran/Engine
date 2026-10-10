#include "engine/resource/gpu_mesh.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

#include <cstdint>
#include <vector>

#include "engine/resource/cpu_mesh.h"
#include "engine/resource/unique_handle.h"

namespace engine {

namespace {

/**
 * @brief Uploads @p elements as one vertex stream of @p attrib, each element
 * read as consecutive floats (3 for a bx::Vec3, 2 for a UV).
 */
template <class T>
auto CreateStream(const std::vector<T>& elements, bgfx::Attrib::Enum attrib)
    -> UniqueHandle<bgfx::VertexBufferHandle> {
  static_assert(sizeof(T) % sizeof(float) == 0, "elements must be floats");
  constexpr auto kCount = static_cast<std::uint8_t>(sizeof(T) / sizeof(float));

  bgfx::VertexLayout layout;
  layout.begin().add(attrib, kCount, bgfx::AttribType::Float).end();
  const auto size = static_cast<std::uint32_t>(elements.size() * sizeof(T));
  return UniqueHandle(
      bgfx::createVertexBuffer(bgfx::copy(elements.data(), size), layout));
}

/** @brief Uploads @p indices as a 32-bit index buffer. */
auto CreateIndices(const std::vector<std::uint32_t>& indices)
    -> UniqueHandle<bgfx::IndexBufferHandle> {
  const auto size =
      static_cast<std::uint32_t>(indices.size() * sizeof(std::uint32_t));
  return UniqueHandle(bgfx::createIndexBuffer(bgfx::copy(indices.data(), size),
                                              BGFX_BUFFER_INDEX32));
}

}  // namespace

auto UploadMesh(const CpuMesh& mesh) -> GpuMesh {
  return {.positions = CreateStream(mesh.positions, bgfx::Attrib::Position),
          .normals = CreateStream(mesh.normals, bgfx::Attrib::Normal),
          .uvs = CreateStream(mesh.uvs, bgfx::Attrib::TexCoord0),
          .indices = CreateIndices(mesh.indices),
          .submeshes = mesh.submeshes,
          .bounds = mesh.bounds};
}

}  // namespace engine
