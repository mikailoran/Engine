#pragma once

#include <cgltf/cgltf.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::gltf {

// Internal to engine_assets: no public header includes this one.

/**
 * @brief The glTF node index that moves each of @p primitive's vertices
 * most: its heaviest joint in @p node's skin, or @p node itself when the node
 * has no skin or the primitive no JOINTS_0.
 *
 * @param vertex_count The primitive's POSITION count.
 * @throws std::runtime_error If JOINTS_0 comes without WEIGHTS_0, their
 *         counts differ from @p vertex_count, or a joint is out of range.
 */
auto VertexBones(const cgltf_data& data, const cgltf_node& node,
                 const cgltf_primitive& primitive, std::size_t vertex_count)
    -> std::vector<std::uint32_t>;

}  // namespace engine::gltf
