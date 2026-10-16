#include "engine/ecs/systems/animation_system.h"

#include <vector>

#include "engine/animation/evaluate.h"
#include "engine/animation/playback.h"
#include "engine/animation/rig.h"
#include "engine/ecs/components/animator.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/skin_pose.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"
#include "engine/resource/animation_handles.h"
#include "engine/resource/asset_registry.h"

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

/** @brief @p track as an evaluation layer of @p weight. */
auto Sample(const animation::Track& track, float weight,
            const AssetRegistry& assets) -> animation::LayerSample {
  return {.clip = &assets.GetClip(track.clip),
          .blend_with = track.blend_with.IsValid()
                            ? &assets.GetClip(track.blend_with)
                            : nullptr,
          .blend = track.blend,
          .phase = track.phase,
          .weight = weight};
}

/**
 * @brief Steps @p layer by @p dt and appends its evaluation layers: what is
 * fading out under what fades in.
 */
void StepLayer(animation::LayerState& layer, float dt,
               const AssetRegistry& assets,
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
                             assets));
  }
  if (playing) {
    samples.push_back(
        Sample(layer.current, layer.weight * (fading ? alpha : 1.0F), assets));
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
      [&](Entity /*entity*/, Animator& animator, const Renderable& renderable,
          SkinPose& pose) -> void {
        const RigHandle rig_handle = assets.GetMesh(renderable.mesh_handle).rig;
        if (!rig_handle.IsValid()) {
          return;
        }
        const animation::Rig& rig = assets.GetRig(rig_handle);

        samples_.clear();
        for (animation::LayerState& layer : animator.layers) {
          StepLayer(layer, ctx.dt, assets, samples_);
        }
        if (samples_.empty()) {
          animation::RestPose(rig, pose.model);
        } else {
          evaluator_.Evaluate(rig, samples_, pose.model);
        }
        animation::Palette(rig.Source(), pose.model, pose.palette);
      });
}

}  // namespace engine
