#include "engine/ecs/systems/attachments.h"

#include <bx/math.h>

#include <cassert>
#include <cstddef>

#include "engine/animation/rig.h"
#include "engine/ecs/components/attached_meshes.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/skin_pose.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/skeleton.h"
#include "engine/util/mat4.h"

namespace engine {

namespace {

/** @brief The product applying @p first, then @p second (row vectors). */
auto Then(const Mat4& first, const Mat4& second) -> Mat4 {
  Mat4 out{};
  bx::mtxMul(out.data(), first.data(), second.data());
  return out;
}

/**
 * @brief The matrix of @p attached's joint on @p entity: its model matrix,
 * or its skinning matrix for a bind-space mesh. Rest pose without a
 * SkinPose.
 */
auto JointMatrix(Ecs& ecs, const AssetRegistry& assets, Entity entity,
                 const AttachedMesh& attached) -> Mat4 {
  const std::size_t joint = attached.joint.value_or(0);
  const bool bind_space = attached.bind_space;
  if (const auto* pose = ecs.TryGetComponent<SkinPose>(entity);
      pose != nullptr && joint < pose->model.size()) {
    return bind_space ? pose->palette.at(joint) : pose->model.at(joint);
  }
  // Unposed: the bind pose, where skinning is the identity
  const auto& renderable = ecs.GetComponent<Renderable>(entity);
  const auto rig = assets.GetMesh(renderable.mesh_handle).rig;
  assert(rig.IsValid() && "a joint attachment needs a skinned mesh");
  if (bind_space) {
    return ModelMatrix(Transform{});
  }
  Mat4 rest{};
  const auto& joints = assets.GetRig(rig).Source().joints;
  bx::mtxInverse(rest.data(), joints.at(joint).inverse_bind.data());
  return rest;
}

}  // namespace

auto AttachmentMatrix(Ecs& ecs, const AssetRegistry& assets, Entity entity,
                      const AttachedMesh& attached) -> Mat4 {
  const Mat4 entity_model = ModelMatrix(ecs.GetComponent<Transform>(entity));
  if (!attached.joint) {
    return Then(attached.offset, entity_model);
  }
  const Mat4 joint = JointMatrix(ecs, assets, entity, attached);
  return Then(Then(attached.offset, joint), entity_model);
}

}  // namespace engine
