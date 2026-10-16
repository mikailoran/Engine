#pragma once

#include <string_view>
#include <vector>

#include "engine/resource/skeleton.h"

namespace engine::animation {

/**
 * @brief Per-joint weights covering the joint named @p root and everything
 * under it: 1 there, 0 elsewhere, one per joint in skeleton order.
 *
 * For LayerSample::joint_weights, e.g. "spine_01" for an upper-body layer.
 * @return All zeros if no joint has that name.
 */
auto JointMask(const Skeleton& skeleton, std::string_view root)
    -> std::vector<float>;

}  // namespace engine::animation
