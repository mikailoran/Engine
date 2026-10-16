#include "engine/ecs/systems/animation_system.h"

#include <bx/math.h>

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "engine/animation/evaluate.h"
#include "engine/animation/mask.h"
#include "engine/animation/playback.h"
#include "engine/animation/rig.h"
#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/ragdoll_link.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/skin_pose.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"
#include "engine/resource/animation_handles.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/skeleton.h"
#include "engine/util/mat4.h"

namespace engine {

namespace {

static_assert(animation::kAnimatorLayers * 2 <= animation::kMaxLayers,
              "every animator layer may crossfade, filling two");

/** @brief Seconds one pass of @p track takes, blending its clips' lengths. */
auto TrackDuration(const animation::Track& track, const AssetRegistry& assets)
    -> float {
  const float first = assets.GetClip(track.clip).Duration();
  if (!track.blend_with.IsValid()) {
    return first;
  }
  const float second = assets.GetClip(track.blend_with).Duration();
  return first + ((second - first) * track.blend);
}

/** @brief @p track as an evaluation layer of @p weight under @p mask. */
auto Sample(const animation::Track& track, float weight,
            const std::vector<float>* mask, const AssetRegistry& assets)
    -> animation::LayerSample {
  return {.clip = &assets.GetClip(track.clip),
          .blend_with = track.blend_with.IsValid()
                            ? &assets.GetClip(track.blend_with)
                            : nullptr,
          .blend = track.blend,
          .phase = track.phase,
          .weight = weight,
          .joint_weights = mask};
}

/**
 * @brief Steps @p layer by @p dt and appends its evaluation layers, under
 * @p mask (null: whole body): what is fading out under what fades in.
 */
void StepLayer(animation::LayerState& layer, float dt,
               const std::vector<float>* mask, const AssetRegistry& assets,
               std::vector<animation::LayerSample>& samples) {
  animation::AdvanceFade(layer, dt);
  const bool fading = layer.previous.clip.IsValid();
  const bool playing = layer.current.clip.IsValid();
  if (fading) {
    animation::Advance(layer.previous, dt,
                       TrackDuration(layer.previous, assets));
  }
  if (playing) {
    animation::Advance(layer.current, dt, TrackDuration(layer.current, assets));
  }

  // The outgoing track stays full under the incoming one, which fades in
  const float alpha = animation::FadeAlpha(layer);
  if (fading) {
    samples.push_back(Sample(layer.previous,
                             layer.weight * (playing ? 1.0F : 1.0F - alpha),
                             mask, assets));
  }
  if (playing) {
    samples.push_back(Sample(
        layer.current, layer.weight * (fading ? alpha : 1.0F), mask, assets));
  }
}

/** @brief The product applying @p first, then @p second (row vectors). */
auto Then(const Mat4& first, const Mat4& second) -> Mat4 {
  Mat4 out{};
  bx::mtxMul(out.data(), first.data(), second.data());
  return out;
}

/** @brief The inverse of @p m. */
auto Inverse(const Mat4& m) -> Mat4 {
  Mat4 out{};
  bx::mtxInverse(out.data(), m.data());
  return out;
}

/**
 * @brief Rebuilds @p model around a simulated ragdoll: the joints its parts
 * move go where physics put them, and every other joint follows its parent
 * with the local transform it had in the last pose.
 * @param entity_model The entity's world matrix.
 */
void FollowRagdoll(const Skeleton& skeleton, const Mat4& entity_model,
                   const std::vector<DrivenJoint>& driven,
                   std::vector<Mat4>& model) {
  const std::vector<Mat4> before = model;
  std::vector<bool> done(model.size(), false);
  const Mat4 to_model = Inverse(entity_model);
  for (const DrivenJoint& joint : driven) {
    model.at(joint.joint) = Then(joint.world, to_model);
    done.at(joint.joint) = true;
  }

  for (std::size_t i = 0; i < model.size(); ++i) {
    // Parents may follow their children: resolve the chain from its top
    std::vector<std::size_t> chain{i};
    for (auto parent = skeleton.joints.at(i).parent;
         parent && !done.at(chain.back());
         parent = skeleton.joints.at(*parent).parent) {
      chain.push_back(*parent);
    }
    for (const std::size_t joint : std::views::reverse(chain)) {
      if (done.at(joint)) {
        continue;
      }
      // Roots above the parts stay; the rest keep their local transform
      if (const auto parent = skeleton.joints.at(joint).parent) {
        const Mat4 local = Then(before.at(joint), Inverse(before.at(*parent)));
        model.at(joint) = Then(local, model.at(*parent));
      }
      done.at(joint) = true;
    }
  }
}

}  // namespace

void AnimationSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                             const FrameContext& ctx) {
  // Give skinned, animated entities a pose; adding inside ForEach is unsafe
  std::vector<Entity> unposed;
  ecs.View<Animator, Renderable>().ForEach(
      [&](Entity entity, const Animator& /*animator*/,
          const Renderable& renderable) -> void {
        if (!ecs.HasComponent<SkinPose>(entity) &&
            assets.GetMesh(renderable.mesh_handle).rig.IsValid()) {
          unposed.push_back(entity);
        }
      });
  for (const Entity entity : unposed) {
    ecs.AddComponent(entity, SkinPose{});
  }

  ecs.View<Animator, Renderable, SkinPose>().ForEach(
      [&](Entity entity, Animator& animator, const Renderable& renderable,
          SkinPose& pose) -> void {
        const RigHandle rig_handle = assets.GetMesh(renderable.mesh_handle).rig;
        if (!rig_handle.IsValid()) {
          return;
        }
        const animation::Rig& rig = assets.GetRig(rig_handle);

        // A simulated ragdoll poses the skeleton instead of the clips
        const auto* link = ecs.TryGetComponent<RagdollLink>(entity);
        const auto* transform = ecs.TryGetComponent<Transform>(entity);
        if (link != nullptr && link->Simulated() && !link->Driven().empty() &&
            transform != nullptr && pose.model.size() == rig.JointCount()) {
          FollowRagdoll(rig.Source(), ModelMatrix(*transform), link->Driven(),
                        pose.model);
          animation::Palette(rig.Source(), pose.model, pose.palette);
          return;
        }

        samples_.clear();
        for (animation::LayerState& layer : animator.layers) {
          const auto* mask =
              layer.mask.empty()
                  ? nullptr
                  : &Mask(assets, rig_handle.Value(), layer.mask);
          StepLayer(layer, ctx.dt, mask, assets, samples_);
        }
        if (samples_.empty()) {
          animation::RestPose(rig, pose.model);
        } else {
          evaluator_.Evaluate(rig, samples_, pose.model);
        }
        animation::Palette(rig.Source(), pose.model, pose.palette);
      });
}

auto AnimationSystem::Mask(const AssetRegistry& assets, std::uint16_t rig_value,
                           const std::string& root)
    -> const std::vector<float>& {
  auto key = std::make_pair(rig_value, root);
  if (auto it = masks_.find(key); it != masks_.end()) {
    return it->second;
  }
  const auto& skeleton = assets.GetRig(RigHandle(rig_value)).Source();
  return masks_.emplace(std::move(key), animation::JointMask(skeleton, root))
      .first->second;
}

}  // namespace engine
