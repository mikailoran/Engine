#pragma once

#include "engine/ecs/components/attached_meshes.h"
#include "engine/ecs/core/types.h"
#include "engine/util/mat4.h"

namespace engine {

class AssetRegistry;
class Ecs;

/**
 * @brief Where @p attached on @p entity is this frame: its world matrix.
 *
 * Follows the entity's Transform and, for a joint, the entity's SkinPose
 * (the rest pose without one).
 * @pre @p entity has a Transform; for a joint, a skinned Renderable whose
 *      skeleton has that joint.
 */
auto AttachmentMatrix(Ecs& ecs, const AssetRegistry& assets, Entity entity,
                      const AttachedMesh& attached) -> Mat4;

}  // namespace engine
