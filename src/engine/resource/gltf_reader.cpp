#include "engine/resource/gltf_reader.h"

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bimg/encode.h>
#include <bx/allocator.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <cgltf/cgltf.h>

#include <array>
#include <cstddef>
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
#include <utility>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine {

namespace {

/** @brief unique_ptr deleter for parsed glTF data. */
struct CgltfFree {
  /** @brief Frees @p data and the buffers loaded into it. */
  void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};

using CgltfData = std::unique_ptr<cgltf_data, CgltfFree>;

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

/** @brief Parses @p path and loads its buffers. */
auto Parse(const std::filesystem::path& path) -> CgltfData {
  const cgltf_options options{};
  cgltf_data* raw = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &raw) != cgltf_result_success) {
    throw std::runtime_error("cannot parse glTF");
  }
  CgltfData data(raw);

  if (cgltf_load_buffers(&options, data.get(), path.c_str()) !=
      cgltf_result_success) {
    throw std::runtime_error("cannot load glTF buffers");
  }
  if (cgltf_validate(data.get()) != cgltf_result_success) {
    throw std::runtime_error("invalid glTF");
  }
  // Compressed or quantized data would read back wrong
  const std::span required(data->extensions_required,
                           data->extensions_required_count);
  if (!required.empty()) {
    throw std::runtime_error("unsupported required extension '" +
                             std::string(required.front()) + "'");
  }
  return data;
}

/** @brief Reads an accessor as flat floats, @p components per element. */
auto UnpackFloats(const cgltf_accessor& accessor, std::size_t components,
                  const char* what) -> std::vector<float> {
  if (cgltf_num_components(accessor.type) != components) {
    throw std::runtime_error(std::string(what) + " has the wrong element type");
  }
  std::vector<float> floats(accessor.count * components);
  cgltf_accessor_unpack_floats(&accessor, floats.data(), floats.size());
  return floats;
}

/**
 * @brief Whether a column-major 4x4 mirrors space, i.e. its upper 3x3 has a
 * negative determinant.
 */
auto IsMirror(const std::array<float, 16>& m) -> bool {
  const auto x = bx::load<bx::Vec3>(&m.at(0));
  const auto y = bx::load<bx::Vec3>(&m.at(4));
  const auto z = bx::load<bx::Vec3>(&m.at(8));
  return bx::dot(bx::cross(x, y), z) < 0.0F;
}

/**
 * @brief Appends one primitive's vertices, baked by @p world, and its
 * indices to its material's bucket.
 */
void AppendPrimitive(const cgltf_data& data, const cgltf_primitive& primitive,
                     const std::array<float, 16>& world, MeshBuilder& builder) {
  if (primitive.type != cgltf_primitive_type_triangles) {
    throw std::runtime_error("only triangle primitives are supported");
  }
  const cgltf_accessor* position =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  const cgltf_accessor* normal =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* uv =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
  if (position == nullptr) {
    throw std::runtime_error("primitive without positions");
  }
  if (normal == nullptr) {
    throw std::runtime_error("primitive without normals");
  }
  if (normal->count != position->count ||
      (uv != nullptr && uv->count != position->count)) {
    throw std::runtime_error("attribute counts differ within a primitive");
  }

  // Normals transform by the inverse transpose
  std::array<float, 16> inverse{};
  std::array<float, 16> normal_mtx{};
  bx::mtxInverse(inverse.data(), world.data());
  bx::mtxTranspose(normal_mtx.data(), inverse.data());

  CpuMesh& mesh = builder.mesh;
  const auto base = static_cast<std::uint32_t>(mesh.positions.size());
  const auto positions = UnpackFloats(*position, 3, "POSITION");
  const auto normals = UnpackFloats(*normal, 3, "NORMAL");
  for (std::size_t i = 0; i < position->count; ++i) {
    const auto p = bx::load<bx::Vec3>(&positions.at(i * 3));
    const auto n = bx::load<bx::Vec3>(&normals.at(i * 3));
    mesh.positions.push_back(bx::mul(p, world.data()));
    mesh.normals.push_back(bx::normalize(bx::mulXyz0(n, normal_mtx.data())));
  }
  if (uv != nullptr) {
    const auto uvs = UnpackFloats(*uv, 2, "TEXCOORD_0");
    for (std::size_t i = 0; i < uv->count; ++i) {
      mesh.uvs.push_back({uvs.at(i * 2), uvs.at((i * 2) + 1)});
    }
  } else {
    mesh.uvs.resize(mesh.positions.size(), {0.0F, 0.0F});
  }

  // Unindexed primitives list their vertices in order
  std::vector<std::uint32_t> indices;
  if (primitive.indices != nullptr) {
    for (std::size_t i = 0; i < primitive.indices->count; ++i) {
      indices.push_back(base +
                        static_cast<std::uint32_t>(
                            cgltf_accessor_read_index(primitive.indices, i)));
    }
  } else {
    for (std::size_t i = 0; i < position->count; ++i) {
      indices.push_back(base + static_cast<std::uint32_t>(i));
    }
  }
  if (indices.size() % 3 != 0) {
    throw std::runtime_error("index count is not a multiple of 3");
  }
  // A mirror turns front faces clockwise; restore counter-clockwise
  if (IsMirror(world)) {
    for (std::size_t i = 0; i < indices.size(); i += 3) {
      std::swap(indices.at(i + 1), indices.at(i + 2));
    }
  }

  const std::size_t bucket =
      primitive.material == nullptr
          ? 0
          : cgltf_material_index(&data, primitive.material) + 1;
  auto& bucket_indices = builder.buckets.at(bucket).indices;
  bucket_indices.insert(bucket_indices.end(), indices.begin(), indices.end());
}

/** @brief Appends the meshes of @p scene's nodes and all their descendants. */
void AppendScene(const cgltf_data& data, const cgltf_scene& scene,
                 MeshBuilder& builder) {
  // Depth-first with an explicit stack instead of recursion
  const std::span roots(scene.nodes, scene.nodes_count);
  std::vector<const cgltf_node*> pending(roots.begin(), roots.end());
  while (!pending.empty()) {
    const cgltf_node& node = *pending.back();
    pending.pop_back();

    if (node.mesh != nullptr) {
      std::array<float, 16> world{};
      cgltf_node_transform_world(&node, world.data());
      for (const cgltf_primitive& primitive :
           std::span(node.mesh->primitives, node.mesh->primitives_count)) {
        AppendPrimitive(data, primitive, world, builder);
      }
    }
    for (const cgltf_node* child :
         std::span(node.children, node.children_count)) {
      pending.push_back(child);
    }
  }
}

/** @brief Box around the vertices @p indices use. @pre Not empty. */
auto BoundsOf(const std::vector<bx::Vec3>& positions,
              std::span<const std::uint32_t> indices) -> bx::Aabb {
  const bx::Vec3& first = positions.at(indices.front());
  bx::Aabb bounds{.min = first, .max = first};
  for (const std::uint32_t index : indices) {
    bounds.min = bx::min(bounds.min, positions.at(index));
    bounds.max = bx::max(bounds.max, positions.at(index));
  }
  return bounds;
}

/** @brief Concatenates the buckets into indices and one submesh each. */
void CutSubmeshes(MeshBuilder& builder) {
  CpuMesh& mesh = builder.mesh;
  for (const MaterialBucket& bucket : builder.buckets) {
    if (bucket.indices.empty()) {
      continue;
    }
    const Submesh submesh{
        .first_index = static_cast<std::uint32_t>(mesh.indices.size()),
        .index_count = static_cast<std::uint32_t>(bucket.indices.size()),
        .material = bucket.material,
        .bounds = BoundsOf(mesh.positions, bucket.indices)};
    mesh.indices.insert(mesh.indices.end(), bucket.indices.begin(),
                        bucket.indices.end());
    mesh.submeshes.push_back(submesh);
  }
  mesh.bounds = BoundsOf(mesh.positions, mesh.indices);
}

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
 * @brief Reads one glTF material's base color into @p mesh, decoding its
 * image unless an earlier material already did.
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

/** @brief Reads every glTF material, in order, into @p mesh's materials. */
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

/**
 * @brief Builds the mesh from the default scene, else the first one, with
 * every material and the images they use.
 */
auto BuildMesh(const cgltf_data& data, const std::filesystem::path& gltf_path)
    -> CpuMesh {
  const cgltf_scene* scene = data.scene;
  if (scene == nullptr && data.scenes_count > 0) {
    scene = data.scenes;
  }
  if (scene == nullptr) {
    throw std::runtime_error("no scene");
  }

  MeshBuilder builder;
  builder.buckets.resize(data.materials_count + 1);
  for (std::uint32_t i = 0; i < data.materials_count; ++i) {
    builder.buckets.at(i + 1).material = i;
  }
  AppendScene(data, *scene, builder);

  if (builder.mesh.positions.size() >
      std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("too many vertices");
  }
  bool has_triangles = false;
  for (const MaterialBucket& bucket : builder.buckets) {
    has_triangles = has_triangles || !bucket.indices.empty();
  }
  if (!has_triangles) {
    throw std::runtime_error("no triangles");
  }
  CutSubmeshes(builder);
  ReadMaterials(data, gltf_path, builder.mesh);
  return std::move(builder.mesh);
}

}  // namespace

auto ReadGltf(const std::filesystem::path& path) -> CpuMesh {
  // Tag every error with the file
  try {
    const CgltfData data = Parse(path);
    return BuildMesh(*data, path);
  } catch (const std::runtime_error& e) {
    throw std::runtime_error(path.string() + ": " + e.what());
  }
}

}  // namespace engine
