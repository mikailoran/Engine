#include "engine/animation/evaluate.h"

#include <bx/math.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/internal/simd_math_config.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <span>
#include <vector>

#include "engine/animation/ozz_state.h"
#include "engine/animation/rig.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

namespace {

/// Most clips sampled per Evaluate: two per layer.
constexpr std::size_t kMaxSamples = kMaxLayers * 2;

/** @brief @p m in the runtime's matrix type; the memory layouts match. */
auto ToOzz(const Mat4& m) -> ozz::math::Float4x4 {
  using ozz::math::simd_float4::LoadPtrU;
  ozz::math::Float4x4 out;
  out.cols[0] = LoadPtrU(&m.at(0));
  out.cols[1] = LoadPtrU(&m.at(4));
  out.cols[2] = LoadPtrU(&m.at(8));
  out.cols[3] = LoadPtrU(&m.at(12));
  return out;
}

/** @brief @p m in the engine's matrix type. */
auto FromOzz(const ozz::math::Float4x4& m) -> Mat4 {
  Mat4 out{};
  ozz::math::StorePtrU(m.cols[0], &out.at(0));
  ozz::math::StorePtrU(m.cols[1], &out.at(4));
  ozz::math::StorePtrU(m.cols[2], &out.at(8));
  ozz::math::StorePtrU(m.cols[3], &out.at(12));
  return out;
}

/**
 * @brief Runs local-to-model on @p locals into @p model, in skeleton order.
 * @param joint_of_runtime The skeleton joint of each runtime joint.
 * @return False if the inputs don't fit the skeleton.
 */
auto ToModel(const ozz::animation::Skeleton& skeleton,
             const Mat4& root_transform,
             const std::vector<int>& joint_of_runtime,
             ozz::span<const ozz::math::SoaTransform> locals,
             std::vector<ozz::math::Float4x4>& scratch,
             std::vector<Mat4>& model) -> bool {
  const auto root = ToOzz(root_transform);
  scratch.resize(static_cast<std::size_t>(skeleton.num_joints()));
  ozz::animation::LocalToModelJob job;
  job.skeleton = &skeleton;
  job.root = &root;
  job.input = locals;
  job.output = ozz::make_span(scratch);
  if (!job.Run()) {
    return false;
  }
  model.resize(scratch.size());
  for (std::size_t runtime = 0; runtime < scratch.size(); ++runtime) {
    const auto joint = static_cast<std::size_t>(joint_of_runtime.at(runtime));
    model.at(joint) = FromOzz(scratch.at(runtime));
  }
  return true;
}

/**
 * @brief Each layer's per-joint weight once the layers above it have taken
 * theirs: layer i gets weight × mask × what layers above i leave uncovered.
 */
void CoverLayers(std::span<const LayerSample> layers, std::size_t joints,
                 std::array<std::vector<float>, kMaxLayers>& effective) {
  std::vector<float> uncovered(joints, 1.0F);
  for (std::size_t i = layers.size(); i-- > 0;) {
    const LayerSample& layer =
        *std::next(layers.begin(), static_cast<std::ptrdiff_t>(i));
    auto& weights = effective.at(i);
    weights.resize(joints);
    for (std::size_t j = 0; j < joints; ++j) {
      const float mask =
          layer.joint_weights != nullptr ? layer.joint_weights->at(j) : 1.0F;
      const float own = std::clamp(layer.weight * mask, 0.0F, 1.0F);
      weights.at(j) = own * uncovered.at(j);
      uncovered.at(j) *= 1.0F - own;
    }
  }
}

/**
 * @brief Packs @p weights × @p factor into the runtime's order, four joints
 * per SIMD value, padding with zeros.
 */
void PackWeights(const std::vector<float>& weights, float factor,
                 const std::vector<int>& joint_of_runtime,
                 std::vector<ozz::math::SimdFloat4>& packed) {
  const std::size_t soa = (joint_of_runtime.size() + 3) / 4;
  std::vector<float> flat(soa * 4, 0.0F);
  for (std::size_t r = 0; r < joint_of_runtime.size(); ++r) {
    flat.at(r) =
        weights.at(static_cast<std::size_t>(joint_of_runtime.at(r))) * factor;
  }
  packed.resize(soa);
  for (std::size_t v = 0; v < soa; ++v) {
    packed.at(v) = ozz::math::simd_float4::LoadPtrU(&flat.at(v * 4));
  }
}

}  // namespace

/** @brief Scratch buffers reused across Evaluate calls. */
struct Evaluator::State {
  /// Keyframe caches, one per sample slot.
  std::array<ozz::animation::SamplingJob::Context, kMaxSamples> contexts;
  /// Each sample slot's local transforms, in runtime order.
  std::array<std::vector<ozz::math::SoaTransform>, kMaxSamples> locals;
  /// Each sample slot's per-joint blend weights, in runtime order.
  std::array<std::vector<ozz::math::SimdFloat4>, kMaxSamples> weights;
  /// Each layer's effective per-joint weight, in skeleton order.
  std::array<std::vector<float>, kMaxLayers> effective;
  std::vector<ozz::math::SoaTransform> blended;
  std::vector<ozz::math::Float4x4> model;
};

Evaluator::Evaluator() : state_(std::make_unique<State>()) {}
Evaluator::~Evaluator() = default;
Evaluator::Evaluator(Evaluator&&) noexcept = default;
auto Evaluator::operator=(Evaluator&&) noexcept -> Evaluator& = default;

void Evaluator::Evaluate(const Rig& rig, std::span<const LayerSample> layers,
                         std::vector<Mat4>& model) {
  assert(layers.size() <= kMaxLayers && "too many layers");
  const Rig::State& rig_state = *rig.state_;
  const ozz::animation::Skeleton& skeleton = *rig_state.skeleton;
  const auto soa = static_cast<std::size_t>(skeleton.num_soa_joints());
  State& s = *state_;
  const auto stack = layers.first(std::min(layers.size(), kMaxLayers));
  CoverLayers(stack, rig.JointCount(), s.effective);

  // Sample each clip into its own slot, weighted per joint
  std::array<ozz::animation::BlendingJob::Layer, kMaxSamples> blend_layers;
  std::size_t samples = 0;
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const LayerSample& layer =
        *std::next(stack.begin(), static_cast<std::ptrdiff_t>(i));
    assert(layer.clip != nullptr && "a layer needs a clip");
    const bool mixes = layer.blend_with != nullptr && layer.blend > 0.0F;
    const float blend = std::clamp(layer.blend, 0.0F, 1.0F);
    const std::array<const Clip*, 2> clips{layer.clip, layer.blend_with};
    const std::array<float, 2> factors{mixes ? 1.0F - blend : 1.0F, blend};
    for (std::size_t k = 0; k < (mixes ? 2U : 1U); ++k, ++samples) {
      auto& locals = s.locals.at(samples);
      locals.resize(soa);
      // Resizing drops the context's keyframe cache, so only grow it
      auto& context = s.contexts.at(samples);
      if (context.max_tracks() < skeleton.num_joints()) {
        context.Resize(skeleton.num_joints());
      }
      ozz::animation::SamplingJob sampling;
      sampling.animation = clips.at(k)->state_->animation.get();
      sampling.context = &context;
      sampling.ratio = std::clamp(layer.phase, 0.0F, 1.0F);
      sampling.output = ozz::make_span(locals);
      const bool sampled = sampling.Run();
      assert(sampled && "clip doesn't fit the rig");
      if (!sampled) {
        RestPose(rig, model);
        return;
      }
      PackWeights(s.effective.at(i), factors.at(k), rig_state.joint_of_runtime,
                  s.weights.at(samples));
      blend_layers.at(samples).weight = 1.0F;
      blend_layers.at(samples).transform = ozz::make_span(locals);
      blend_layers.at(samples).joint_weights =
          ozz::make_span(s.weights.at(samples));
    }
  }

  // Joints no layer covers fall back to the rest pose
  s.blended.resize(soa);
  ozz::animation::BlendingJob blending;
  blending.layers = {blend_layers.data(), samples};
  blending.rest_pose = skeleton.joint_rest_poses();
  blending.output = ozz::make_span(s.blended);
  const bool posed =
      blending.Run() && ToModel(skeleton, rig_state.source.root_transform,
                                rig_state.joint_of_runtime,
                                ozz::make_span(s.blended), s.model, model);
  assert(posed && "blend inputs don't fit the rig");
  if (!posed) {
    RestPose(rig, model);
  }
}

void RestPose(const Rig& rig, std::vector<Mat4>& model) {
  const Rig::State& state = *rig.state_;
  std::vector<ozz::math::Float4x4> scratch;
  const bool posed = ToModel(
      *state.skeleton, state.source.root_transform, state.joint_of_runtime,
      state.skeleton->joint_rest_poses(), scratch, model);
  assert(posed && "rest pose doesn't fit the rig");
  static_cast<void>(posed);
}

void Palette(const Skeleton& skeleton, const std::vector<Mat4>& model,
             std::vector<Mat4>& palette) {
  assert(model.size() == skeleton.joints.size() && "one matrix per joint");
  palette.resize(model.size());
  for (std::size_t j = 0; j < model.size(); ++j) {
    // Row vectors: the inverse bind applies first
    bx::mtxMul(palette.at(j).data(), skeleton.joints.at(j).inverse_bind.data(),
               model.at(j).data());
  }
}

}  // namespace engine::animation
