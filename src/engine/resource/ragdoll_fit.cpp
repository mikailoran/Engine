#include "engine/resource/ragdoll_fit.h"

#include <bx/math.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/math/rotation.h"
#include "engine/physics/pose.h"
#include "engine/physics/ragdoll.h"
#include "engine/resource/skeleton.h"
#include "engine/util/mat4.h"

namespace engine {

namespace {

/// Shortest capsule straight section, in m, so a short bone still has one.
constexpr float kMinHalfHeight = 0.01F;

/** @brief Index of the joint named @p name; throws if there is none. */
auto JointNamed(const Skeleton& skeleton, const std::string& name)
    -> std::uint32_t {
  const auto it = std::ranges::find(skeleton.joints, name, &Joint::name);
  if (it == skeleton.joints.end()) {
    throw std::runtime_error("ragdoll: no joint named '" + name + "'");
  }
  return static_cast<std::uint32_t>(it - skeleton.joints.begin());
}

/** @brief The shortest rotation taking +Y onto unit @p to. */
auto FromUp(const bx::Vec3& to) -> bx::Quaternion {
  const bx::Vec3 up{0.0F, 1.0F, 0.0F};
  const float cos_angle = bx::dot(up, to);
  // Straight down: any half turn about a level axis will do
  if (cos_angle < -0.9999F) {
    return {1.0F, 0.0F, 0.0F, 0.0F};
  }
  const bx::Vec3 axis = bx::cross(up, to);
  return bx::normalize(
      bx::Quaternion{axis.x, axis.y, axis.z, 1.0F + cos_angle});
}

/// Ragdoll file format version this reader reads.
constexpr int kRagdollVersion = 1;

/** @brief Throws unless every key of @p object is in @p allowed. */
void CheckKeys(const nlohmann::json& object,
               std::initializer_list<std::string_view> allowed) {
  for (const auto& [key, value] : object.items()) {
    if (std::ranges::find(allowed, key) == allowed.end()) {
      throw std::runtime_error("unknown key '" + key + "'");
    }
  }
}

/** @brief One part's spec from its JSON object. */
auto ReadPart(const nlohmann::json& data) -> RagdollPartSpec {
  CheckKeys(data, {"joint", "end", "length", "parent", "radius", "mass_kg",
                   "swing_deg", "twist_deg"});
  RagdollPartSpec spec;
  spec.joint = data.at("joint").get<std::string>();
  spec.end = data.value("end", spec.end);
  spec.length = data.value("length", spec.length);
  spec.parent = data.value("parent", spec.parent);
  spec.radius = data.value("radius", spec.radius);
  spec.mass = data.value("mass_kg", spec.mass);
  spec.swing_deg = data.value("swing_deg", spec.swing_deg);
  spec.twist_deg = data.value("twist_deg", spec.twist_deg);
  return spec;
}

}  // namespace

auto ReadRagdollFile(const std::filesystem::path& path)
    -> std::vector<RagdollPartSpec> {
  // Tag every error with the file; JSON errors become runtime_error
  try {
    std::ifstream file(path);
    if (!file) {
      throw std::runtime_error("cannot open file");
    }
    const auto data = nlohmann::json::parse(file);
    CheckKeys(data, {"version", "parts"});
    if (data.at("version").get<int>() != kRagdollVersion) {
      throw std::runtime_error("unsupported version");
    }
    std::vector<RagdollPartSpec> specs;
    for (const auto& part : data.at("parts")) {
      specs.push_back(ReadPart(part));
    }
    return specs;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(path.string() + ": " + e.what());
  } catch (const std::runtime_error& e) {
    throw std::runtime_error(path.string() + ": " + e.what());
  }
}

auto PosesOf(const std::vector<Mat4>& model) -> std::vector<physics::Pose> {
  std::vector<physics::Pose> poses;
  poses.reserve(model.size());
  for (const Mat4& m : model) {
    poses.push_back(
        {.position = bx::load<bx::Vec3>(&m.at(12)), .rotation = RotationOf(m)});
  }
  return poses;
}

auto FitRagdoll(const std::vector<RagdollPartSpec>& specs,
                const Skeleton& skeleton, const std::vector<Mat4>& rest_model)
    -> physics::RagdollDesc {
  const auto rest = PosesOf(rest_model);
  physics::RagdollDesc desc;
  for (const RagdollPartSpec& spec : specs) {
    physics::RagdollPart part;
    part.joint = JointNamed(skeleton, spec.joint);
    if (!spec.parent.empty()) {
      const std::uint32_t parent_joint = JointNamed(skeleton, spec.parent);
      const auto parent = std::ranges::find(desc.parts, parent_joint,
                                            &physics::RagdollPart::joint);
      if (parent == desc.parts.end()) {
        throw std::runtime_error("ragdoll: part '" + spec.joint +
                                 "' hangs from '" + spec.parent +
                                 "', which isn't an earlier part");
      }
      part.parent = static_cast<std::uint32_t>(parent - desc.parts.begin());
    }

    // The capsule runs from the joint to its end, in model space
    const physics::Pose& joint = rest.at(part.joint);
    const bx::Vec3 end =
        spec.end.empty()
            ? bx::add(joint.position, bx::mul(bx::Vec3{0.0F, spec.length, 0.0F},
                                              joint.rotation))
            : rest.at(JointNamed(skeleton, spec.end)).position;
    const bx::Vec3 along = bx::sub(end, joint.position);
    const float length = bx::length(along);
    if (length < 1e-4F) {
      throw std::runtime_error("ragdoll: part '" + spec.joint +
                               "' has no length");
    }

    // Stored in the joint's own space, so it follows the joint
    const bx::Quaternion to_joint = bx::conjugate(joint.rotation);
    part.offset.position = bx::mul(bx::mul(along, 0.5F), to_joint);
    part.offset.rotation =
        bx::mul(to_joint, FromUp(bx::mul(along, 1.0F / length)));
    part.half_height = std::max((0.5F * length) - spec.radius, kMinHalfHeight);
    part.radius = spec.radius;
    part.mass = spec.mass;
    part.swing_deg = spec.swing_deg;
    part.twist_deg = spec.twist_deg;
    desc.parts.push_back(part);
  }
  return desc;
}

}  // namespace engine
