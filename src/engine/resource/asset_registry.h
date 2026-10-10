#pragma once

#include <bx/bounds.h>

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "engine/physics/collision_mesh_handle.h"
#include "engine/resource/gpu_mesh.h"
#include "engine/resource/mesh_handle.h"

namespace engine {

namespace physics {
class PhysicsWorld;
}  // namespace physics

/**
 * @brief Single owner of every loaded mesh and collision mesh, keyed by file
 * path.
 *
 * Loads each path at most once and hands out non-owning handles, so any
 * number of entities can share one asset without sharing responsibility for
 * freeing it. Asset lifetime is therefore independent of entity lifetime.
 *
 * Assets live from their first load until the registry is destroyed, which
 * must precede bgfx::shutdown; there is no refcounting. Move-only.
 */
class AssetRegistry {
 public:
  /**
   * @brief Loads a mesh, or returns the handle of one already loaded.
   *
   * Idempotent per path: repeated calls with the same path perform a single
   * load and return equal handles. Requires bgfx::init to have completed and
   * the asset root to be set via SetAssetRoot, since the path resolves
   * against it.
   *
   * @param path glTF file, e.g. "assets/meshes/bunny.glb", flattened into
   *        one mesh with ReadGltf.
   * @return Handle to the mesh.
   * @throws std::runtime_error If the file cannot be read as a mesh.
   */
  auto LoadMesh(const std::filesystem::path& path) -> MeshHandle;

  /**
   * @brief Resolves a handle to the mesh it refers to.
   *
   * @param handle Handle from LoadMesh; must be valid and not yet unloaded.
   * @return Mesh owned by this registry, valid for the registry's lifetime.
   */
  [[nodiscard]] auto GetMesh(MeshHandle handle) const -> const GpuMesh&;

  /**
   * @brief Returns the box around all of a mesh's submeshes, in mesh space.
   * @param handle Handle from LoadMesh; same preconditions as GetMesh.
   */
  [[nodiscard]] auto GetMeshBounds(MeshHandle handle) const -> bx::Aabb;

  /**
   * @brief Builds a collision mesh from a glTF file's triangles, or returns
   * the handle of the one already built from that path.
   *
   * Reads the file on the CPU only, so this does not load the render mesh.
   * Same preconditions as LoadMesh.
   *
   * @param path glTF file, e.g. "assets/levels/level01.glb".
   * @param physics World to build the mesh in; every call must pass the same.
   * @throws std::runtime_error If the file cannot be read as a mesh or Jolt
   *         rejects its triangles.
   */
  auto LoadCollisionMesh(const std::filesystem::path& path,
                         physics::PhysicsWorld& physics)
      -> physics::CollisionMeshHandle;

 private:
  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> mesh_by_path_;
  std::vector<GpuMesh> meshes_;

  std::unordered_map<std::filesystem::path, physics::CollisionMeshHandle>
      collision_mesh_by_path_;
};

}  // namespace engine
