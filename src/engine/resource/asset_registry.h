#pragma once

#include <bx/bounds.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/animation/rig.h"
#include "engine/physics/collision_mesh_handle.h"
#include "engine/physics/ragdoll.h"
#include "engine/resource/animation_handles.h"
#include "engine/resource/gpu_mesh.h"
#include "engine/resource/mesh_handle.h"

namespace engine {

namespace physics {
class PhysicsWorld;
}  // namespace physics

/**
 * @brief Single owner of every loaded mesh, collision mesh, rig, clip and
 * ragdoll, keyed by file path.
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
   *        one mesh with ReadGltf. A skinned file also gets a rig
   *        (GpuMesh::rig).
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

  /**
   * @brief Resolves a skinned mesh's rig.
   * @param handle GpuMesh::rig of a loaded mesh; must be valid.
   */
  [[nodiscard]] auto GetRig(RigHandle handle) const -> const animation::Rig&;

  /**
   * @brief Loads a glTF file's clips retargeted onto @p rig (RetargetClip),
   * or returns the set already loaded for that file and rig.
   *
   * Reads keys only, no geometry or images. Same preconditions as LoadMesh.
   * @param path glTF file, e.g. "assets/animations/UAL1.glb".
   * @param rig A loaded mesh's rig; must be valid.
   * @throws std::runtime_error If the file can't be read as clips, or a clip
   *         shares no joint name with the rig.
   */
  auto LoadClips(const std::filesystem::path& path, RigHandle rig)
      -> ClipSetHandle;

  /**
   * @brief The clip named @p name in @p set.
   * @return Invalid if the set has no clip of that name.
   */
  [[nodiscard]] auto FindClip(ClipSetHandle set, std::string_view name) const
      -> ClipHandle;

  /** @brief Resolves a clip handle; it must be valid. */
  [[nodiscard]] auto GetClip(ClipHandle handle) const -> const animation::Clip&;

  /**
   * @brief Builds ragdoll settings from a ragdoll file fitted to @p rig's
   * rest pose (ReadRagdollFile, FitRagdoll), or returns those already built
   * for that file and rig.
   *
   * Same preconditions as LoadMesh.
   * @param path Ragdoll file, e.g. "assets/ragdolls/ual_humanoid.json".
   * @param rig A loaded mesh's rig; must be valid.
   * @param physics World to build the settings in; every call must pass the
   *        same.
   * @throws std::runtime_error If the file can't be read, names joints the
   *         rig lacks, or Jolt rejects the parts.
   */
  auto LoadRagdoll(const std::filesystem::path& path, RigHandle rig,
                   physics::PhysicsWorld& physics)
      -> physics::RagdollSettingsHandle;

 private:
  /** @brief One clip file's clips built for one rig, by name. */
  struct ClipSet {
    std::unordered_map<std::string, ClipHandle> by_name;
  };

  // TODO: figure out optimized key and also cross platform compatibility
  std::unordered_map<std::filesystem::path, MeshHandle> mesh_by_path_;
  std::vector<GpuMesh> meshes_;

  std::unordered_map<std::filesystem::path, physics::CollisionMeshHandle>
      collision_mesh_by_path_;

  std::vector<animation::Rig> rigs_;
  std::vector<animation::Clip> clips_;
  std::vector<ClipSet> clip_sets_;
  /// Keyed by clip file and RigHandle::Value.
  std::map<std::pair<std::filesystem::path, std::uint16_t>, ClipSetHandle>
      clip_set_by_key_;
  /// Keyed by ragdoll file and RigHandle::Value.
  std::map<std::pair<std::filesystem::path, std::uint16_t>,
           physics::RagdollSettingsHandle>
      ragdoll_by_key_;
};

}  // namespace engine
