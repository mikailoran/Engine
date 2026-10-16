#include "engine/resource/gltf_skin.h"

#include <cgltf/cgltf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "engine/resource/gltf_document.h"

namespace engine::gltf {

auto VertexBones(const cgltf_data& data, const cgltf_node& node,
                 const cgltf_primitive& primitive, std::size_t vertex_count)
    -> std::vector<std::uint32_t> {
  const cgltf_accessor* joints =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
  const cgltf_accessor* weights =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);

  // Unskinned vertices move with the node holding the mesh
  if (node.skin == nullptr || joints == nullptr) {
    return std::vector<std::uint32_t>(
        vertex_count,
        static_cast<std::uint32_t>(cgltf_node_index(&data, &node)));
  }
  if (weights == nullptr) {
    throw std::runtime_error("JOINTS_0 without WEIGHTS_0");
  }
  if (joints->count != vertex_count || weights->count != vertex_count) {
    throw std::runtime_error("attribute counts differ within a primitive");
  }
  if (cgltf_num_components(joints->type) != 4) {
    throw std::runtime_error("JOINTS_0 has the wrong element type");
  }

  // The node index of each of the skin's joints
  std::vector<std::uint32_t> joint_nodes;
  for (const cgltf_node* joint :
       std::span(node.skin->joints, node.skin->joints_count)) {
    joint_nodes.push_back(
        static_cast<std::uint32_t>(cgltf_node_index(&data, joint)));
  }

  const auto weight_values = UnpackFloats(*weights, 4, "WEIGHTS_0");
  std::vector<std::uint32_t> bones;
  bones.reserve(vertex_count);
  for (std::size_t i = 0; i < vertex_count; ++i) {
    std::array<cgltf_uint, 4> vertex_joints{};
    cgltf_accessor_read_uint(joints, i, vertex_joints.data(), 4);

    // The heaviest of the vertex's four influences
    std::size_t heaviest = 0;
    for (std::size_t k = 1; k < 4; ++k) {
      if (weight_values.at((i * 4) + k) >
          weight_values.at((i * 4) + heaviest)) {
        heaviest = k;
      }
    }
    const cgltf_uint joint = vertex_joints.at(heaviest);
    if (joint >= joint_nodes.size()) {
      throw std::runtime_error("joint index out of range");
    }
    bones.push_back(joint_nodes.at(joint));
  }
  return bones;
}

}  // namespace engine::gltf
