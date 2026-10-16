#pragma once

#include <vector>

#include "engine/util/mat4.h"

namespace engine {

/**
 * @brief Its entity's current skeleton pose, one entry per joint in skeleton
 * order. Written by AnimationSystem every frame; read by the renderer (the
 * palette) and by anything placed on a joint (the model matrices).
 *
 * Added by AnimationSystem to an entity with an Animator and a skinned
 * Renderable. Without one, the mesh draws in bind pose.
 */
struct SkinPose {
  /// Each joint's transform in the mesh's model space.
  std::vector<Mat4> model;
  /// Each joint's skinning matrix: model × inverse bind.
  std::vector<Mat4> palette;
};

}  // namespace engine
