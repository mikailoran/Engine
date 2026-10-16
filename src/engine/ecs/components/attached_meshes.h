#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/resource/mesh_handle.h"
#include "engine/util/mat4.h"

namespace engine {

/**
 * @brief A mesh drawn fixed to one of its entity's joints, or to the entity
 * itself: a held weapon, hair, a gun before a camera.
 *
 * Draws with the default material and casts shadows, but has no collider
 * and isn't picked.
 */
struct AttachedMesh {
  /// Owned by AssetRegistry, like Renderable's.
  MeshHandle mesh{MeshHandle::kInvalid};
  /// Index into the entity's mesh skeleton; empty: the entity's Transform.
  std::optional<std::uint32_t> joint;
  /// Placement relative to the joint (or entity), in its space, e.g.
  /// ModelMatrix of a Transform.
  Mat4 offset{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
              0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  /// The mesh is modelled in the character's bind pose (like hair placed on
  /// the head in Blender): it follows `joint` by its skinning matrix, and
  /// `offset` applies in mesh space before that.
  bool bind_space{false};
};

/**
 * @brief Meshes attached to this entity, drawn every frame wherever their
 * joint is. Joints follow the entity's SkinPose, or the rest pose without
 * one.
 */
struct AttachedMeshes {
  std::vector<AttachedMesh> meshes;
};

}  // namespace engine
