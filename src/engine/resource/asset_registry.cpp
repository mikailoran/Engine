#include "engine/resource/asset_registry.h"

#include <bx/bounds.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/animation/evaluate.h"
#include "engine/animation/rig.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/hitbox_handle.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/ragdoll.h"
#include "engine/physics/shape.h"
#include "engine/platform/asset_root.h"
#include "engine/resource/animation_clip.h"
#include "engine/resource/animation_handles.h"
#include "engine/resource/bone_parts.h"
#include "engine/resource/cpu_mesh.h"
#include "engine/resource/gltf_reader.h"
#include "engine/resource/gpu_mesh.h"
#include "engine/resource/mesh_handle.h"
#include "engine/resource/ragdoll_fit.h"
#include "engine/resource/retarget.h"
#include "engine/util/mat4.h"

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
  GpuMesh gpu = UploadMesh(cpu_mesh);
  if (cpu_mesh.skeleton) {
    gpu.rig = RigHandle(static_cast<std::uint16_t>(rigs_.size()));
    rigs_.emplace_back(*cpu_mesh.skeleton);
  }
  const MeshHandle handle(static_cast<std::uint16_t>(meshes_.size()));
  meshes_.push_back(std::move(gpu));
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

auto AssetRegistry::LoadHitbox(const std::filesystem::path& path,
                               physics::PhysicsWorld& physics)
    -> physics::HitboxHandle {
  if (auto it = hitbox_by_path_.find(path); it != hitbox_by_path_.end()) {
    return it->second;
  }

  // One hull per bone, in the mesh's bind pose
  std::vector<physics::ConvexPart> parts;
  for (BonePart& part : SplitByBone(ReadAsset(path))) {
    parts.push_back({.points = std::move(part.points)});
  }
  const physics::HitboxHandle handle = physics.CreateHitbox(parts);
  if (!handle.IsValid()) {
    throw std::runtime_error("cannot build hitbox: " + path.string());
  }

  hitbox_by_path_.emplace(path, handle);
  return handle;
}

auto AssetRegistry::GetRig(RigHandle handle) const -> const animation::Rig& {
  assert(handle.IsValid() && "invalid rig handle");
  return rigs_.at(handle.Value());
}

auto AssetRegistry::LoadClips(const std::filesystem::path& path, RigHandle rig)
    -> ClipSetHandle {
  const auto key = std::make_pair(path, rig.Value());
  if (auto it = clip_set_by_key_.find(key); it != clip_set_by_key_.end()) {
    return it->second;
  }

  const ClipFile file =
      ReadGltfClips(std::filesystem::path(AssetRoot()) / path);
  const animation::Rig& target = GetRig(rig);
  ClipSet set;
  for (const AnimationClip& clip : file.clips) {
    const AnimationClip mapped =
        RetargetClip(clip, file.skeleton, target.Source());
    const ClipHandle handle(static_cast<std::uint32_t>(clips_.size()));
    clips_.emplace_back(mapped, target);
    set.by_name.emplace(clip.name, handle);
  }

  const ClipSetHandle handle(static_cast<std::uint16_t>(clip_sets_.size()));
  clip_sets_.push_back(std::move(set));
  clip_set_by_key_.emplace(key, handle);
  return handle;
}

auto AssetRegistry::LoadRagdoll(const std::filesystem::path& path,
                                RigHandle rig, physics::PhysicsWorld& physics)
    -> physics::RagdollSettingsHandle {
  const auto key = std::make_pair(path, rig.Value());
  if (auto it = ragdoll_by_key_.find(key); it != ragdoll_by_key_.end()) {
    return it->second;
  }

  // Capsules and joint limits are set from the rig's rest pose
  const animation::Rig& target = GetRig(rig);
  std::vector<Mat4> rest;
  animation::RestPose(target, rest);
  const auto specs = ReadRagdollFile(std::filesystem::path(AssetRoot()) / path);
  const physics::RagdollSettingsHandle handle = physics.CreateRagdollSettings(
      FitRagdoll(specs, target.Source(), rest), PosesOf(rest));
  if (!handle.IsValid()) {
    throw std::runtime_error("cannot build ragdoll: " + path.string());
  }
  ragdoll_by_key_.emplace(key, handle);
  return handle;
}

auto AssetRegistry::FindClip(ClipSetHandle set, std::string_view name) const
    -> ClipHandle {
  assert(set.IsValid() && "invalid clip set handle");
  const auto& by_name = clip_sets_.at(set.Value()).by_name;
  const auto it = by_name.find(std::string(name));
  return it != by_name.end() ? it->second : ClipHandle{};
}

auto AssetRegistry::GetClip(ClipHandle handle) const -> const animation::Clip& {
  assert(handle.IsValid() && "invalid clip handle");
  return clips_.at(handle.Value());
}

}  // namespace engine
