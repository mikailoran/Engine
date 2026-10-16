#include "engine/animation/mask.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "engine/resource/skeleton.h"

namespace engine::animation {

auto JointMask(const Skeleton& skeleton, std::string_view root)
    -> std::vector<float> {
  std::vector<float> weights(skeleton.joints.size(), 0.0F);
  for (std::size_t joint = 0; joint < skeleton.joints.size(); ++joint) {
    // Covered if it or an ancestor is the root
    for (std::optional<std::uint32_t> at = static_cast<std::uint32_t>(joint);
         at; at = skeleton.joints.at(*at).parent) {
      if (skeleton.joints.at(*at).name == root) {
        weights.at(joint) = 1.0F;
        break;
      }
    }
  }
  return weights;
}

}  // namespace engine::animation
