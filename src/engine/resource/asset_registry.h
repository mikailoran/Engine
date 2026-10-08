#pragma once

#include <bgfx/bgfx.h>
#include <bx/bounds.h>

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include "engine/physics/collision_mesh_handle.h"
#include "engine/resource/mesh_handle.h"
#include "engine/resource/texture_handle.h"
#include "engine/resource/unique_handle.h"

struct Mesh;

namespace engine {

namespace physics {
class PhysicsWorld;
}  // namespace physics

/** @brief unique_ptr deleter that frees a mesh with meshUnload. */
struct MeshUnloader {
  /** @brief Unloads @p mesh's GPU buffers and frees it. */
  void operator()(Mesh* mesh) const noexcept;
};

/**
 * @brief Single owner of every loaded mesh and texture, keyed by file path.
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
   * @param path Compiled mesh file, e.g. "assets/meshes/compiled/bunny.bin".
   * @return Handle to the mesh.
   * @throws std::runtime_error If the file cannot be opened.
   */
  auto LoadMesh(const std::filesystem::path& path) -> MeshHandle;

  /**
   * @brief Resolves a handle to the mesh it refers to.
   *
   * @param handle Handle from LoadMesh; must be valid and not yet unloaded.
   * @return Mesh owned by this registry, valid for the registry's lifetime.
   */
  auto GetMesh(MeshHandle handle) const -> const Mesh*;

  /**
   * @brief Returns the box around all of a mesh's groups, in mesh space.
   * @param handle Handle from LoadMesh; same preconditions as GetMesh.
   */
  [[nodiscard]] auto GetMeshBounds(MeshHandle handle) const -> bx::Aabb;

  /**
   * @brief Builds a collision mesh from a compiled mesh file's triangles, or
   * returns the handle of the one already built from that path.
   *
   * The triangles are read through a temporary load, so this does not load
   * the render mesh. Same preconditions as LoadMesh.
   *
   * @param path Compiled mesh file, e.g. "assets/meshes/level01.bin".
   * @param physics World to build the mesh in; every call must pass the same.
   * @throws std::runtime_error If the file cannot be opened or Jolt rejects
   *         its triangles.
   */
  auto LoadCollisionMesh(const std::filesystem::path& path,
                         physics::PhysicsWorld& physics)
      -> physics::CollisionMeshHandle;

  /**
   * @brief Loads a texture, or returns the handle of one already loaded.
   *
   * Same idempotence and preconditions as LoadMesh. Loaded as sRGB with
   * anisotropic filtering and repeat addressing.
   *
   * @param path Compiled texture file, e.g. "assets/textures/debug_grid.dds".
   * @return Handle to the texture.
   * @throws std::runtime_error If the texture cannot be loaded.
   */
  auto LoadTexture(const std::filesystem::path& path) -> TextureHandle;

  /**
   * @brief Resolves a handle to the bgfx texture it refers to.
   *
   * @param handle Handle from LoadTexture; must be valid and not yet unloaded.
   * @return bgfx texture owned by this registry, valid for its lifetime.
   */
  auto GetTexture(TextureHandle handle) const -> bgfx::TextureHandle;

 private:
  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> mesh_by_path_;
  std::vector<std::unique_ptr<Mesh, MeshUnloader>> meshes_;

  std::unordered_map<std::filesystem::path, physics::CollisionMeshHandle>
      collision_mesh_by_path_;

  std::unordered_map<std::filesystem::path, TextureHandle> texture_by_path_;
  std::vector<UniqueHandle<bgfx::TextureHandle>> textures_;
};

}  // namespace engine
