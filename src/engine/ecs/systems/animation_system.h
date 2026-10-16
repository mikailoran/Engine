#pragma once

#include <vector>

#include "engine/animation/evaluate.h"

namespace engine {

class AssetRegistry;
class Ecs;
struct FrameContext;

/**
 * @brief Advances every Animator and poses its entity's skinned mesh into a
 * SkinPose.
 *
 * Runs in Engine::Update after physics, so this frame's character velocity
 * can have chosen the clips, and before anything reads joint poses (the
 * renderer, attachments). Adds a SkinPose to each entity with an Animator
 * and a skinned Renderable; an Animator on a static mesh is ignored.
 */
class AnimationSystem {
 public:
  /** @brief Steps playback by the frame's dt and writes each SkinPose. */
  void Update(Ecs& ecs, const AssetRegistry& assets, const FrameContext& ctx);

 private:
  animation::Evaluator evaluator_;
  /// This entity's evaluation layers, rebuilt per entity.
  std::vector<animation::LayerSample> samples_;
};

}  // namespace engine
