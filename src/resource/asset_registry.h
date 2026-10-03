#pragma once

#include <bgfx/bgfx.h>

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "mesh_handle.h"
#include "texture_handle.h"

struct Mesh;

/**
 * @brief Single owner of every loaded mesh and texture, keyed by file path.
 *
 * Loads each path at most once and hands out non-owning handles, so any
 * number of entities can share one asset without sharing responsibility for
 * freeing it. Asset lifetime is therefore independent of entity lifetime.
 *
 * Assets live from their first load until UnloadAll; there is no refcounting.
 */
class AssetRegistry {
 public:
  AssetRegistry() noexcept = default;
  // Delete copy&assignment: registry owns Mesh pointers
  AssetRegistry(const AssetRegistry&) = delete;
  AssetRegistry& operator=(const AssetRegistry&) = delete;

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
  MeshHandle LoadMesh(const std::filesystem::path& path);

  /**
   * @brief Resolves a handle to the mesh it refers to.
   *
   * @param handle Handle from LoadMesh; must be valid and not yet unloaded.
   * @return Mesh owned by this registry, valid until UnloadAll.
   */
  const Mesh* GetMesh(MeshHandle handle) const;

  /**
   * @brief Loads a texture, or returns the handle of one already loaded.
   *
   * Same idempotence and preconditions as LoadMesh. Loaded as sRGB with
   * anisotropic filtering and repeat addressing.
   *
   * @param path Compiled texture file, e.g. "assets/textures/debug_grid.dds".
   * @return Handle to the texture.
   */
  TextureHandle LoadTexture(const std::filesystem::path& path);

  /**
   * @brief Resolves a handle to the bgfx texture it refers to.
   *
   * @param handle Handle from LoadTexture; must be valid and not yet unloaded.
   * @return bgfx texture owned by this registry, valid until UnloadAll.
   */
  bgfx::TextureHandle GetTexture(TextureHandle handle) const;

  /**
   * @brief Frees every loaded asset and empties the registry.
   *
   * Must run before bgfx::shutdown, since it destroys GPU resources.
   * Every handle handed out so far dangles afterwards.
   */
  void UnloadAll();

 private:
  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> mesh_by_path_;
  std::vector<Mesh*> meshes_;

  std::unordered_map<std::filesystem::path, TextureHandle> texture_by_path_;
  std::vector<bgfx::TextureHandle> textures_;
};
