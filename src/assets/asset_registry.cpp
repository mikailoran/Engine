#include "asset_registry.h"

#include <bgfx_utils.h>
#include <cassert>

MeshHandle AssetRegistry::LoadMesh(const std::filesystem::path &path) {
  // Mesh previously loaded: return handle from map
  if (auto it = by_path_.find(path); it != by_path_.end()) {
    return it->second;
  }

  auto *mesh = meshLoad(path.c_str());
  // TODO: Handle mesh loading errors better: not silent
  if (!mesh) {
    return kInvalidMesh;
  }

  const MeshHandle handle{static_cast<std::uint16_t>(meshes_.size())};
  meshes_.push_back(mesh);
  by_path_.emplace(path, handle);
  return handle;
}

[[nodiscard]] const Mesh *AssetRegistry::Get(MeshHandle handle) const {
  assert(isValid(handle) && "Trying to get invalid mesh handle.");
  assert(handle.idx < meshes_.size() && "Mesh handle out of range.");

  return meshes_.at(handle.idx);
}

void AssetRegistry::UnloadAll() {
  for (Mesh *mesh : meshes_) {
    meshUnload(mesh);
  }
  meshes_.clear();
  by_path_.clear();
}