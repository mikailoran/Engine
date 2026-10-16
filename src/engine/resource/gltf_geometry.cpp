#include "engine/resource/gltf_geometry.h"

#include <bx/bounds.h>
#include <bx/math.h>
#include <cgltf/cgltf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_document.h"

namespace engine::gltf {

namespace {

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

}  // namespace

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

}  // namespace engine::gltf
