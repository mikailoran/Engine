#include "resource/asset_registry.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "resource/mesh_handle.h"
#include "resource/texture_handle.h"
#include "resource/unique_handle.h"

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
