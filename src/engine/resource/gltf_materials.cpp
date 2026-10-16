#include "engine/resource/gltf_materials.h"

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bimg/encode.h>
#include <bx/allocator.h>
#include <cgltf/cgltf.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine::gltf {

namespace {

/** @brief unique_ptr deleter for images bimg allocated. */
struct ImageFree {
  /** @brief Frees @p image through the allocator it was made with. */
  void operator()(bimg::ImageContainer* image) const noexcept {
    bimg::imageFree(image);
  }
};

using ImagePtr = std::unique_ptr<bimg::ImageContainer, ImageFree>;

/**
 * @brief Reads @p image's encoded bytes, embedded in the file or from a file
 * next to @p gltf_path.
 */
auto ImageBytes(const cgltf_image& image,
                const std::filesystem::path& gltf_path)
    -> std::vector<std::uint8_t> {
  if (image.buffer_view != nullptr) {
    const std::uint8_t* data = cgltf_buffer_view_data(image.buffer_view);
    if (data == nullptr) {
      throw std::runtime_error("image data is unavailable");
    }
    const std::span bytes(data, image.buffer_view->size);
    return {bytes.begin(), bytes.end()};
  }
  if (image.uri == nullptr) {
    throw std::runtime_error("image without data");
  }

  std::string uri(image.uri);
  if (uri.starts_with("data:")) {
    throw std::runtime_error("images in data URIs are unsupported");
  }
  // URIs are percent-encoded
  uri.resize(cgltf_decode_uri(uri.data()));
  const auto file = gltf_path.parent_path() / uri;
  std::ifstream stream(file, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("cannot open image " + file.string());
  }
  return {std::istreambuf_iterator<char>(stream), {}};
}

/** @brief Decodes PNG or JPEG @p bytes to RGBA8 and generates every mip. */
auto DecodeImage(std::span<const std::uint8_t> bytes) -> CpuImage {
  // Declared first, so it outlives the images allocated from it
  bx::DefaultAllocator allocator;
  const ImagePtr parsed(bimg::imageParse(
      &allocator, bytes.data(), static_cast<std::uint32_t>(bytes.size()),
      bimg::TextureFormat::RGBA8));
  if (!parsed) {
    throw std::runtime_error("cannot decode image");
  }
  const ImagePtr mips(bimg::imageGenerateMips(&allocator, *parsed));
  if (!mips) {
    throw std::runtime_error("cannot generate mips");
  }
  if (mips->m_width > std::numeric_limits<std::uint16_t>::max() ||
      mips->m_height > std::numeric_limits<std::uint16_t>::max()) {
    throw std::runtime_error("image too large");
  }

  const std::span pixels(static_cast<const std::uint8_t*>(mips->m_data),
                         mips->m_size);
  return {.width = static_cast<std::uint16_t>(mips->m_width),
          .height = static_cast<std::uint16_t>(mips->m_height),
          .mip_count = mips->m_numMips,
          .rgba8 = {pixels.begin(), pixels.end()}};
}

/**
 * @brief Reads one glTF material's base color and metallic-roughness factors
 * into @p mesh, decoding its image unless an earlier material already did.
 *
 * @param image_slots Index in mesh.images per glTF image, filled as decoded.
 */
auto ReadMaterial(const cgltf_data& data, const cgltf_material& source,
                  const std::filesystem::path& gltf_path,
                  std::vector<std::optional<std::uint32_t>>& image_slots,
                  CpuMesh& mesh) -> MeshMaterial {
  MeshMaterial material;
  if (!source.has_pbr_metallic_roughness) {
    return material;
  }
  const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
  material.base_color = std::to_array(pbr.base_color_factor);
  material.metallic = std::clamp(pbr.metallic_factor, 0.0F, 1.0F);
  material.roughness = std::clamp(pbr.roughness_factor, 0.0F, 1.0F);

  const cgltf_texture_view& view = pbr.base_color_texture;
  if (view.texture == nullptr || view.texture->image == nullptr) {
    return material;
  }
  if (view.texcoord != 0) {
    throw std::runtime_error("base color texture must use TEXCOORD_0");
  }
  if (view.has_transform) {
    throw std::runtime_error("texture transforms are unsupported");
  }

  const cgltf_image& image = *view.texture->image;
  auto& slot = image_slots.at(cgltf_image_index(&data, &image));
  if (!slot) {
    slot = static_cast<std::uint32_t>(mesh.images.size());
    mesh.images.push_back(DecodeImage(ImageBytes(image, gltf_path)));
  }
  material.base_color_image = slot;
  return material;
}

}  // namespace

void ReadMaterials(const cgltf_data& data,
                   const std::filesystem::path& gltf_path, CpuMesh& mesh) {
  std::vector<std::optional<std::uint32_t>> image_slots(data.images_count);
  for (const cgltf_material& source :
       std::span(data.materials, data.materials_count)) {
    // Name the material in errors; glTF names may be missing
    const std::string name = source.name != nullptr ? source.name : "";
    try {
      mesh.materials.push_back(
          ReadMaterial(data, source, gltf_path, image_slots, mesh));
    } catch (const std::runtime_error& e) {
      throw std::runtime_error("material '" + name + "': " + e.what());
    }
  }
}

}  // namespace engine::gltf
