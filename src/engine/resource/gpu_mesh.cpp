#include "engine/resource/gpu_mesh.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
 * @brief Uploads each vertex's four joint indices (8-bit integers) and four
 * weights (floats), interleaved in one stream; an invalid handle for a
 * static mesh.
 */
auto CreateSkinStream(const CpuMesh& mesh)
    -> UniqueHandle<bgfx::VertexBufferHandle> {
  if (mesh.joints.empty()) {
    return {};
  }
  // Integers, not normalized: the shader indexes its palette with them
  bgfx::VertexLayout layout;
  layout.begin()
      .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, false, true)
      .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
      .end();
  const std::uint16_t stride = layout.getStride();

  std::vector<std::uint8_t> bytes(mesh.joints.size() * stride);
  for (std::size_t v = 0; v < mesh.joints.size(); ++v) {
    const std::size_t base = v * stride;
    for (std::size_t k = 0; k < 4; ++k) {
      const std::uint16_t joint = mesh.joints.at(v).at(k);
      assert(joint <= std::numeric_limits<std::uint8_t>::max() &&
             "joint index too large for 8 bits");
      bytes.at(base + k) = static_cast<std::uint8_t>(joint);
    }
    // The weights follow the four index bytes
    const auto& weights = mesh.weights.at(v);
    std::memcpy(&bytes.at(base + 4), weights.data(), sizeof(weights));
  }
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
              .skin = CreateSkinStream(mesh),
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
