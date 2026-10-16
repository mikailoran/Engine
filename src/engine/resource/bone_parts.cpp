#include "engine/resource/bone_parts.h"

#include <bx/math.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "engine/resource/cpu_mesh.h"

namespace engine {

auto SplitByBone(const CpuMesh& mesh) -> std::vector<BonePart> {
  assert(mesh.bones.size() == mesh.positions.size() && "one bone per position");
  // Ordered by bone, so parts come out sorted
  std::map<std::uint32_t, std::vector<bx::Vec3>> points_by_bone;
  for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
    points_by_bone[mesh.bones.at(i)].push_back(mesh.positions.at(i));
  }

  std::vector<BonePart> parts;
  parts.reserve(points_by_bone.size());
  for (auto& [bone, points] : points_by_bone) {
    parts.push_back({.bone = bone, .points = std::move(points)});
  }
  return parts;
}

}  // namespace engine
