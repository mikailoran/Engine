#include "engine/resource/asset_registry.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"
#include "engine/resource/mesh_handle.h"
#include "engine/resource/texture_handle.h"
#include "engine/resource/unique_handle.h"

namespace engine {

namespace {

/** @brief Gathers every group's triangles. @pre Loaded with a RAM copy. */
auto CollisionTriangles(const Mesh& mesh) -> physics::TriangleMesh {
  physics::TriangleMesh triangles;
  for (const Group& group : mesh.m_groups) {
    assert(group.m_vertices != nullptr && group.m_indices != nullptr &&
           "mesh loaded without a RAM copy");
    // Groups index their own vertices, so offset into the shared list
    const auto base = static_cast<std::uint32_t>(triangles.vertices.size());
    for (std::uint32_t i = 0; i < group.m_numVertices; ++i) {
      std::array<float, 4> position{};
      bgfx::vertexUnpack(position.data(), bgfx::Attrib::Position, mesh.m_layout,
                         group.m_vertices, i);
      const auto [x, y, z, w] = position;
      triangles.vertices.emplace_back(x, y, z);
    }
    for (const std::uint16_t index :
         std::span(group.m_indices, group.m_numIndices)) {
      triangles.indices.push_back(base + index);
    }
  }
  return triangles;
}

}  // namespace

void MeshUnloader::operator()(Mesh* mesh) const noexcept { meshUnload(mesh); }

auto AssetRegistry::LoadMesh(const std::filesystem::path& path) -> MeshHandle {
  // Mesh previously loaded: return handle from map
  if (auto it = mesh_by_path_.find(path); it != mesh_by_path_.end()) {
    return it->second;
  }

  // Owned before anything else can throw
  std::unique_ptr<Mesh, MeshUnloader> mesh(meshLoad(path.c_str()));
  if (!mesh) {
    throw std::runtime_error("cannot load mesh: " + path.string());
  }

  const MeshHandle handle{static_cast<std::uint16_t>(meshes_.size())};
  meshes_.push_back(std::move(mesh));
  mesh_by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] auto AssetRegistry::GetMesh(MeshHandle handle) const
    -> const Mesh* {
  assert(IsValid(handle) && "Trying to get invalid mesh handle.");
  assert(handle.idx < meshes_.size() && "Mesh handle out of range.");

  return meshes_.at(handle.idx).get();
}

auto AssetRegistry::GetMeshBounds(MeshHandle handle) const -> bx::Aabb {
  const Mesh& mesh = *GetMesh(handle);
  assert(!mesh.m_groups.empty() && "mesh has no groups");
  // Inverted, so an empty mesh yields an inverted (empty) box
  constexpr float kMax = std::numeric_limits<float>::max();
  bx::Aabb bounds{.min = {kMax, kMax, kMax}, .max = {-kMax, -kMax, -kMax}};
  for (const Group& group : mesh.m_groups) {
    bounds.min = bx::min(bounds.min, group.m_aabb.min);
    bounds.max = bx::max(bounds.max, group.m_aabb.max);
  }
  return bounds;
}

auto AssetRegistry::LoadCollisionMesh(const std::filesystem::path& path,
                                      physics::PhysicsWorld& physics)
    -> physics::CollisionMeshHandle {
  if (auto it = collision_mesh_by_path_.find(path);
      it != collision_mesh_by_path_.end()) {
    return it->second;
  }

  // Loaded with a RAM copy for its triangles, then freed with its GPU buffers
  const std::unique_ptr<Mesh, MeshUnloader> mesh(meshLoad(path.c_str(), true));
  if (!mesh) {
    throw std::runtime_error("cannot load mesh: " + path.string());
  }
  const physics::CollisionMeshHandle handle =
      physics.CreateCollisionMesh(CollisionTriangles(*mesh));
  if (!handle.IsValid()) {
    throw std::runtime_error("cannot build collision mesh: " + path.string());
  }

  collision_mesh_by_path_.emplace(path, handle);
  return handle;
}

auto AssetRegistry::LoadTexture(const std::filesystem::path& path)
    -> TextureHandle {
  // Texture previously loaded: return handle from map
  if (auto it = texture_by_path_.find(path); it != texture_by_path_.end()) {
    return it->second;
  }

  // Default sampler addressing is repeat, which tiling relies on
  constexpr uint64_t kFlags = BGFX_TEXTURE_SRGB | BGFX_SAMPLER_MIN_ANISOTROPIC |
                              BGFX_SAMPLER_MAG_ANISOTROPIC;
  UniqueHandle texture(loadTexture(path.c_str(), kFlags));
  if (!texture) {
    throw std::runtime_error("cannot load texture: " + path.string());
  }

  const TextureHandle handle{static_cast<std::uint16_t>(textures_.size())};
  textures_.push_back(std::move(texture));
  texture_by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] auto AssetRegistry::GetTexture(TextureHandle handle) const
    -> bgfx::TextureHandle {
  assert(IsValid(handle) && "Trying to get invalid texture handle.");
  assert(handle.idx < textures_.size() && "Texture handle out of range.");

  return textures_.at(handle.idx).Get();
}

}  // namespace engine
