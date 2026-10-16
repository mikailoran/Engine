#include "engine/resource/gpu_mesh.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
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

/**
 * @brief Uploads joint indices as 8-bit integers, four per vertex; an
 * invalid handle for none.
 */
auto CreateJointStream(const std::vector<std::array<std::uint16_t, 4>>& joints)
    -> UniqueHandle<bgfx::VertexBufferHandle> {
  if (joints.empty()) {
    return {};
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(joints.size() * 4);
  for (const auto& vertex : joints) {
    for (const std::uint16_t joint : vertex) {
      assert(joint <= std::numeric_limits<std::uint8_t>::max() &&
             "joint index too large for 8 bits");
      bytes.push_back(static_cast<std::uint8_t>(joint));
    }
  }
  bgfx::VertexLayout layout;
  // Integers, not normalized: the shader indexes its palette with them
  layout.begin()
      .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, false, true)
      .end();
  const auto size = static_cast<std::uint32_t>(bytes.size());
  return UniqueHandle(
      bgfx::createVertexBuffer(bgfx::copy(bytes.data(), size), layout));
}

/** @brief Uploads @p indices as a 32-bit index buffer. */
auto CreateIndices(const std::vector<std::uint32_t>& indices)
    -> UniqueHandle<bgfx::IndexBufferHandle> {
  const auto size =
      static_cast<std::uint32_t>(indices.size() * sizeof(std::uint32_t));
  return UniqueHandle(bgfx::createIndexBuffer(bgfx::copy(indices.data(), size),
                                              BGFX_BUFFER_INDEX32));
}

/** @brief Uploads @p image as an sRGB texture with its mips. */
auto CreateTexture(const CpuImage& image) -> UniqueHandle<bgfx::TextureHandle> {
  // Default sampler addressing is repeat, which tiled UVs rely on
  constexpr std::uint64_t kFlags = BGFX_TEXTURE_SRGB |
                                   BGFX_SAMPLER_MIN_ANISOTROPIC |
                                   BGFX_SAMPLER_MAG_ANISOTROPIC;
  const auto size = static_cast<std::uint32_t>(image.rgba8.size());
  return UniqueHandle(
      bgfx::createTexture2D(image.width, image.height, image.mip_count > 1, 1,
                            bgfx::TextureFormat::RGBA8, kFlags,
                            bgfx::copy(image.rgba8.data(), size)));
}

}  // namespace

auto UploadMesh(const CpuMesh& mesh) -> GpuMesh {
  GpuMesh gpu{.positions = CreateStream(mesh.positions, bgfx::Attrib::Position),
              .normals = CreateStream(mesh.normals, bgfx::Attrib::Normal),
              .uvs = CreateStream(mesh.uvs, bgfx::Attrib::TexCoord0),
              .joints = CreateJointStream(mesh.joints),
              .weights = mesh.weights.empty()
                             ? UniqueHandle<bgfx::VertexBufferHandle>()
                             : CreateStream(mesh.weights, bgfx::Attrib::Weight),
              .indices = CreateIndices(mesh.indices),
              .submeshes = mesh.submeshes,
              .bounds = mesh.bounds,
              .materials = mesh.materials,
              .textures = {},
              .rig = {}};
  gpu.textures.reserve(mesh.images.size());
  for (const CpuImage& image : mesh.images) {
    gpu.textures.push_back(CreateTexture(image));
  }
  return gpu;
}

}  // namespace engine
