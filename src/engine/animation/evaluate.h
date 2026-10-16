#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "engine/animation/rig.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

/// Most layers one Evaluate call blends.
inline constexpr std::size_t kMaxLayers = 4;

/**
 * @brief One layer of a pose: a clip, or two blended, sampled at a phase.
 *
 * Both clips are sampled at the same phase, so clips of different lengths
 * (a walk and a run) stay in step.
 */
struct LayerSample {
  /// Required; must belong to the Rig being evaluated.
  const Clip* clip{nullptr};
  /// Optional second clip, mixed in by `blend`.
  const Clip* blend_with{nullptr};
  /// 0 samples `clip` alone, 1 `blend_with` alone.
  float blend{0.0F};
  /// Normalized time, 0 (start) to 1 (end); clamped.
  float phase{0.0F};
  /// 0..1: how much this layer overrides the layers below it.
  float weight{1.0F};
  /// Optional per-joint 0..1 scale of `weight`, one per joint in skeleton
  /// order (a mask); null means 1 everywhere. Must outlive the call.
  const std::vector<float>* joint_weights{nullptr};
};

/**
 * @brief Poses rigs from layered clips into model-space joint matrices.
 *
 * Holds scratch buffers, so one Evaluator serves every entity in turn.
 * Movable, not copyable.
 */
class Evaluator {
 public:
  Evaluator();
  ~Evaluator();
  Evaluator(const Evaluator&) = delete;
  auto operator=(const Evaluator&) -> Evaluator& = delete;
  Evaluator(Evaluator&&) noexcept;
  auto operator=(Evaluator&&) noexcept -> Evaluator&;

  /**
   * @brief Writes each joint's model-space matrix for @p layers on @p rig.
   *
   * Layers stack in order: each overrides what's below it by its weight
   * times its joint weight, so the first layer is the base. Joints no layer
   * covers hold the rest pose. Model space includes the skeleton's
   * root_transform.
   * @param layers At most kMaxLayers.
   * @param model Resized to one entry per joint, in skeleton order.
   */
  void Evaluate(const Rig& rig, std::span<const LayerSample> layers,
                std::vector<Mat4>& model);

 private:
  struct State;
  std::unique_ptr<State> state_;
};

/**
 * @brief Writes each joint's model-space matrix in rest pose.
 * @param model Resized to one entry per joint, in skeleton order.
 */
void RestPose(const Rig& rig, std::vector<Mat4>& model);

/**
 * @brief The skinning matrices for a pose: each joint's model matrix times
 * its inverse bind matrix, which takes a bind-pose vertex to its posed
 * place in model space.
 * @param model From Evaluate or RestPose; one per joint of @p skeleton.
 * @param palette Resized to match @p model.
 */
void Palette(const Skeleton& skeleton, const std::vector<Mat4>& model,
             std::vector<Mat4>& palette);

}  // namespace engine::animation
