#include "engine/resource/asset_registry.h"

#include <bx/bounds.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/shape.h"
#include "engine/platform/asset_root.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/gpu_mesh.h"
#include "engine/resource/mesh_handle.h"

namespace engine {

namespace {

/** @brief Reads the glTF at @p path, relative to the asset root. */
auto ReadAsset(const std::filesystem::path& path) -> CpuMesh {
  return ReadGltf(std::filesystem::path(AssetRoot()) / path);
}

}  // namespace

auto AssetRegistry::LoadMesh(const std::filesystem::path& path) -> MeshHandle {
  // Mesh previously loaded: return handle from map
  if (auto it = mesh_by_path_.find(path); it != mesh_by_path_.end()) {
    return it->second;
  }

  const auto cpu_mesh = ReadAsset(path);
  const MeshHandle handle(static_cast<std::uint16_t>(meshes_.size()));
  meshes_.push_back(UploadMesh(cpu_mesh));
  mesh_by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] auto AssetRegistry::GetMesh(MeshHandle handle) const
    -> const GpuMesh& {
  assert(handle.IsValid() && "Trying to get invalid mesh handle.");
  assert(handle.Value() < meshes_.size() && "Mesh handle out of range.");

  return meshes_.at(handle.Value());
}

auto AssetRegistry::GetMeshBounds(MeshHandle handle) const -> bx::Aabb {
  return GetMesh(handle).bounds;
}

auto AssetRegistry::LoadCollisionMesh(const std::filesystem::path& path,
                                      physics::PhysicsWorld& physics)
    -> physics::CollisionMeshHandle {
  if (auto it = collision_mesh_by_path_.find(path);
      it != collision_mesh_by_path_.end()) {
    return it->second;
  }

  // Positions and indices are already a triangle mesh
  // TODO: A registry to prevent loading assets from files twice.
  CpuMesh data = ReadAsset(path);
  const physics::TriangleMesh triangles{.vertices = std::move(data.positions),
                                        .indices = std::move(data.indices)};
  const physics::CollisionMeshHandle handle =
      physics.CreateCollisionMesh(triangles);
  if (!handle.IsValid()) {
    throw std::runtime_error("cannot build collision mesh: " + path.string());
  }

  collision_mesh_by_path_.emplace(path, handle);
  return handle;
}

}  // namespace engine
