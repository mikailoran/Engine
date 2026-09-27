#pragma once

#include "mesh_handle.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

struct Mesh;

/**
 * @brief Single owner of every loaded mesh, keyed by file path.
 *
 * Loads each path at most once and hands out non-owning MeshHandles, so any
 * number of entities can share one mesh without sharing responsibility for
 * freeing it. Asset lifetime is therefore independent of entity lifetime.
 *
 * Meshes live from their first load until UnloadAll; there is no refcounting.
 */
class AssetRegistry {
public:
  AssetRegistry() noexcept = default;
  // Delete copy&assignment: registry owns Mesh pointers
  AssetRegistry(const AssetRegistry &) = delete;
  AssetRegistry &operator=(const AssetRegistry &) = delete;

  /**
   * @brief Loads a mesh, or returns the handle of one already loaded.
   *
   * Idempotent per path: repeated calls with the same path perform a single
   * load and return equal handles. Requires bgfx::init to have completed and
   * the asset root to be set via entry::setCurrentDir, since the path resolves
   * against it.
   *
   * @param path Compiled mesh file, e.g. "assets/meshes/compiled/bunny.bin".
   * @return Handle to the mesh.
   */
  MeshHandle LoadMesh(const std::filesystem::path &path);

  /**
   * @brief Resolves a handle to the mesh it refers to.
   *
   * @param handle Handle from LoadMesh; must be valid and not yet unloaded.
   * @return Mesh owned by this registry, valid until UnloadAll.
   */
  const Mesh *Get(MeshHandle handle) const;

  /**
   * @brief Frees every loaded mesh and empties the registry.
   *
   * Must run before bgfx::shutdown, since meshUnload destroys GPU buffers.
   * Every handle handed out so far dangles afterwards.
   */
  void UnloadAll();

private:
  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> by_path_;
  std::vector<Mesh *> meshes_;
};
