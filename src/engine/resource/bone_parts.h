#pragma once

#include <bx/math.h>

#include <cstdint>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine {

/**
 * @brief The vertices one bone moves most, in mesh space and bind pose.
 * Made by SplitByBone; physics builds a convex hull from each.
 */
struct BonePart {
  /// The glTF node index of the bone, as in CpuMesh::bones.
  std::uint32_t bone{0};
  std::vector<bx::Vec3> points;
};

/**
 * @brief Groups @p mesh's positions by the bone that moves each most.
 *
 * Requires one entry in CpuMesh::bones per position, as ReadGltf gives.
 * @return One part per bone with any vertex, ordered by bone; an unskinned
 *         mesh gives one part per node.
 */
auto SplitByBone(const CpuMesh& mesh) -> std::vector<BonePart>;

}  // namespace engine
