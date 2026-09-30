#include "asset_registry.h"

#include <bgfx_utils.h>
#include <cassert>

MeshHandle AssetRegistry::LoadMesh(const std::filesystem::path &path) {
  // Mesh previously loaded: return handle from map
  if (auto it = mesh_by_path_.find(path); it != mesh_by_path_.end()) {
    return it->second;
  }

  auto *mesh = meshLoad(path.c_str());
  assert(mesh && "Trying to load invalid mesh.");

  const MeshHandle handle{static_cast<std::uint16_t>(meshes_.size())};
  meshes_.push_back(mesh);
  mesh_by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] const Mesh *AssetRegistry::GetMesh(MeshHandle handle) const {
  assert(isValid(handle) && "Trying to get invalid mesh handle.");
  assert(handle.idx < meshes_.size() && "Mesh handle out of range.");

  return meshes_.at(handle.idx);
}

TextureHandle AssetRegistry::LoadTexture(const std::filesystem::path &path) {
  // Texture previously loaded: return handle from map
  if (auto it = texture_by_path_.find(path); it != texture_by_path_.end()) {
    return it->second;
  }

  // Default sampler addressing is repeat, which tiling relies on
  constexpr uint64_t kFlags = BGFX_TEXTURE_SRGB | BGFX_SAMPLER_MIN_ANISOTROPIC |
                              BGFX_SAMPLER_MAG_ANISOTROPIC;
  const auto texture = loadTexture(path.c_str(), kFlags);
  assert(bgfx::isValid(texture) && "Trying to load invalid texture.");

  const TextureHandle handle{static_cast<std::uint16_t>(textures_.size())};
  textures_.push_back(texture);
  texture_by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] bgfx::TextureHandle
AssetRegistry::GetTexture(TextureHandle handle) const {
  assert(isValid(handle) && "Trying to get invalid texture handle.");
  assert(handle.idx < textures_.size() && "Texture handle out of range.");

  return textures_.at(handle.idx);
}

void AssetRegistry::UnloadAll() {
  for (Mesh *mesh : meshes_) {
    meshUnload(mesh);
  }
  meshes_.clear();
  mesh_by_path_.clear();

  for (const auto texture : textures_) {
    bgfx::destroy(texture);
  }
  textures_.clear();
  texture_by_path_.clear();
}
