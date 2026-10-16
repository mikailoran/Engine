#include "engine/resource/gltf_skin.h"

#include <bx/math.h>
#include <cgltf/cgltf.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/resource/gltf_document.h"
#include "engine/resource/skeleton.h"

namespace engine::gltf {

namespace {

/** @brief @p node's name, or "" when it has none. */
auto NodeName(const cgltf_node& node) -> std::string {
  return node.name != nullptr ? node.name : "";
}

/**
 * @brief A joint named after @p node with its rest TRS; no parent yet.
 * @throws std::runtime_error If the node has a matrix instead of TRS.
 */
auto RestJoint(const cgltf_node& node) -> Joint {
  if (node.has_matrix) {
    throw std::runtime_error("joint '" + NodeName(node) +
                             "' has a matrix instead of TRS");
  }
  Joint joint{.name = NodeName(node), .parent = std::nullopt};
  // glTF quaternions are xyzw, as bx's
  if (node.has_translation) {
    joint.translation = bx::load<bx::Vec3>(std::span(node.translation).data());
  }
  if (node.has_rotation) {
    joint.rotation = bx::load<bx::Quaternion>(std::span(node.rotation).data());
  }
  if (node.has_scale) {
    joint.scale = bx::load<bx::Vec3>(std::span(node.scale).data());
  }
  return joint;
}

/**
 * @brief The joint index of @p node's nearest ancestor that is a joint, or
 * empty for a root.
 */
auto ParentJoint(
    const cgltf_node& node,
    const std::unordered_map<const cgltf_node*, std::uint32_t>& joint_of)
    -> std::optional<std::uint32_t> {
  for (const cgltf_node* ancestor = node.parent; ancestor != nullptr;
       ancestor = ancestor->parent) {
    if (const auto it = joint_of.find(ancestor); it != joint_of.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

/** @brief A skinned primitive's JOINTS_0 and WEIGHTS_0 accessors. */
struct SkinAttributes {
  /// Never null.
  const cgltf_accessor* joints{nullptr};
  /// Never null.
  const cgltf_accessor* weights{nullptr};
};

/**
 * @brief @p primitive's JOINTS_0 and WEIGHTS_0, checked against each other
 * and @p vertex_count.
 * @pre @p primitive has JOINTS_0.
 * @throws std::runtime_error If WEIGHTS_0 is missing, a count differs or
 *         JOINTS_0 isn't four components.
 */
auto CheckedSkinAttributes(const cgltf_primitive& primitive,
                           std::size_t vertex_count) -> SkinAttributes {
  const cgltf_accessor* joints =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
  const cgltf_accessor* weights =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
  if (weights == nullptr) {
    throw std::runtime_error("JOINTS_0 without WEIGHTS_0");
  }
  if (joints->count != vertex_count || weights->count != vertex_count) {
    throw std::runtime_error("attribute counts differ within a primitive");
  }
  if (cgltf_num_components(joints->type) != 4) {
    throw std::runtime_error("JOINTS_0 has the wrong element type");
  }
  return {.joints = joints, .weights = weights};
}

}  // namespace

auto IsSkinned(const cgltf_node& node, const cgltf_primitive& primitive)
    -> bool {
  return node.skin != nullptr &&
         cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0) !=
             nullptr;
}

auto VertexInfluences(const cgltf_node& node, const cgltf_primitive& primitive,
                      std::size_t vertex_count) -> Influences {
  const auto [joints, weights] = CheckedSkinAttributes(primitive, vertex_count);
  const auto weight_values = UnpackFloats(*weights, 4, "WEIGHTS_0");
  const std::size_t joint_count = node.skin->joints_count;

  Influences influences;
  influences.joints.reserve(vertex_count);
  influences.weights.reserve(vertex_count);
  for (std::size_t i = 0; i < vertex_count; ++i) {
    std::array<cgltf_uint, 4> vertex_joints{};
    cgltf_accessor_read_uint(joints, i, vertex_joints.data(), 4);
    std::array<std::uint16_t, 4> indices{};
    std::array<float, 4> vertex_weights{};
    float sum = 0.0F;
    for (std::size_t k = 0; k < 4; ++k) {
      if (vertex_joints.at(k) >= joint_count) {
        throw std::runtime_error("joint index out of range");
      }
      indices.at(k) = static_cast<std::uint16_t>(vertex_joints.at(k));
      vertex_weights.at(k) = weight_values.at((i * 4) + k);
      sum += vertex_weights.at(k);
    }
    if (sum <= 0.0F) {
      throw std::runtime_error("vertex with no joint weight");
    }
    // Exporters round weights; skinning wants them to sum to 1
    for (float& weight : vertex_weights) {
      weight /= sum;
    }
    influences.joints.push_back(indices);
    influences.weights.push_back(vertex_weights);
  }
  return influences;
}

auto VertexBones(const cgltf_data& data, const cgltf_node& node,
                 const cgltf_primitive& primitive, std::size_t vertex_count)
    -> std::vector<std::uint32_t> {
  // Unskinned vertices move with the node holding the mesh
  if (!IsSkinned(node, primitive)) {
    return std::vector<std::uint32_t>(
        vertex_count,
        static_cast<std::uint32_t>(cgltf_node_index(&data, &node)));
  }
  const auto [joints, weights] = CheckedSkinAttributes(primitive, vertex_count);

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

auto ReadSkeleton(const cgltf_skin& skin) -> Skeleton {
  const std::span joint_nodes(skin.joints, skin.joints_count);
  if (joint_nodes.empty()) {
    throw std::runtime_error("skin without joints");
  }
  if (joint_nodes.size() > kMaxJoints) {
    throw std::runtime_error("skin has more than " +
                             std::to_string(kMaxJoints) + " joints");
  }

  // Joint index of each node that is a joint
  std::unordered_map<const cgltf_node*, std::uint32_t> joint_of;
  for (const cgltf_node* node : joint_nodes) {
    joint_of.emplace(node, static_cast<std::uint32_t>(joint_of.size()));
  }

  Skeleton skeleton;
  std::optional<const cgltf_node*> root_parent;
  for (const cgltf_node* node : joint_nodes) {
    Joint joint = RestJoint(*node);
    joint.parent = ParentJoint(*node, joint_of);
    if (!joint.parent) {
      if (root_parent && *root_parent != node->parent) {
        throw std::runtime_error("root joints hang under different nodes");
      }
      root_parent = node->parent;
    }
    skeleton.joints.push_back(std::move(joint));
  }

  if (skin.inverse_bind_matrices != nullptr) {
    const auto matrices =
        UnpackFloats(*skin.inverse_bind_matrices, 16, "inverseBindMatrices");
    if (matrices.size() != joint_nodes.size() * 16) {
      throw std::runtime_error("inverseBindMatrices count differs from joints");
    }
    for (std::size_t i = 0; i < joint_nodes.size(); ++i) {
      std::copy_n(matrices.begin() + static_cast<std::ptrdiff_t>(i * 16), 16,
                  skeleton.joints.at(i).inverse_bind.begin());
    }
  }
  // Roots stand under their parent node, placed as in the scene
  if (root_parent && *root_parent != nullptr) {
    cgltf_node_transform_world(*root_parent, skeleton.root_transform.data());
  }
  return skeleton;
}

}  // namespace engine::gltf
