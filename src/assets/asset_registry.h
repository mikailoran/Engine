#pragma once

#include "mesh_handle.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

struct Mesh;

class AssetRegistry {
public:
  AssetRegistry() noexcept = default;
  // Delete copy&assignment: owns Mesh pointers
  AssetRegistry(const AssetRegistry &) = delete;
  AssetRegistry &operator=(const AssetRegistry &) = delete;

  MeshHandle LoadMesh(const std::filesystem::path &path);
  const Mesh *Get(MeshHandle handle) const;
  void UnloadAll();

private:
  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> by_path_;
  std::vector<Mesh *> meshes_;
};