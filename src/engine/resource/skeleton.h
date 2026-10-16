#pragma once

#include <bx/bx.h>
#include <bx/math.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/util/mat4.h"  // IWYU pragma: export

namespace engine {

/// Most joints a skeleton may have; the skinning shaders hold this many.
inline constexpr std::size_t kMaxJoints = 128;

/**
 * @brief One bone of a Skeleton: its place in the hierarchy, its rest pose
 * and its inverse bind matrix.
 */
struct Joint {
  /// The glTF node's name; clips and attachments find joints by it.
  std::string name;
  /// Index into Skeleton::joints; empty for a root.
  std::optional<std::uint32_t> parent;
  /// Rest pose relative to the parent joint (or to Skeleton::root_transform
  /// for a root): translation in m, unit rotation, scale.
  bx::Vec3 translation{0.0F};
  bx::Quaternion rotation{bx::InitIdentity};
  bx::Vec3 scale{1.0F};
  /// Takes a vertex from mesh space to this joint's space in bind pose.
  Mat4 inverse_bind{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                    0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

/**
 * @brief A glTF skin's joint hierarchy, in the skin's own joint order, so a
 * vertex's joint indices (CpuMesh::joints) index it directly.
 *
 * Made by ReadGltf and ReadGltfClips. A joint's model-space matrix is
 * root_transform × the rest (or animated) locals from its root down; times
 * its inverse_bind, that is the matrix skinning applies.
 */
struct Skeleton {
  /// At most kMaxJoints; parents may come after their children.
  std::vector<Joint> joints;
  /// Mesh-space transform of the node above the root joints (e.g. Blender's
  /// armature object); identity when the roots are scene roots.
  Mat4 root_transform{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                      0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

}  // namespace engine
