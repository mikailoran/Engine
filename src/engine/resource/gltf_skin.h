#pragma once

#include <cgltf/cgltf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/resource/skeleton.h"

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/** @brief A skinned primitive's four joint influences per vertex. */
struct Influences {
  /// Indices into the skin's joints, one entry per vertex.
  std::vector<std::array<std::uint16_t, 4>> joints;
  /// Matching weights, summing to 1 per vertex.
  std::vector<std::array<float, 4>> weights;
};

/** @brief Whether @p node has a skin and @p primitive joint indices. */
auto IsSkinned(const cgltf_node& node, const cgltf_primitive& primitive)
    -> bool;

/**
 * @brief Reads @p primitive's JOINTS_0 and WEIGHTS_0, weights renormalized.
 *
 * @pre IsSkinned(node, primitive).
 * @param vertex_count The primitive's POSITION count.
 * @throws std::runtime_error If WEIGHTS_0 is missing, the counts differ from
 *         @p vertex_count, a joint is out of range, or a vertex has no weight.
 */
auto VertexInfluences(const cgltf_node& node, const cgltf_primitive& primitive,
                      std::size_t vertex_count) -> Influences;

/**
 * @brief Reads @p skin's joints, in the skin's order, with their parents,
 * rest poses and inverse bind matrices.
 *
 * A joint's parent is its nearest ancestor node that is also a joint.
 * @throws std::runtime_error If the skin has no joints or more than
 *         kMaxJoints, a joint has a matrix instead of TRS, or the root joints
 *         hang under different nodes.
 */
auto ReadSkeleton(const cgltf_skin& skin) -> Skeleton;

}  // namespace engine::gltf
