#pragma once

#include <array>
#include <cstddef>

#include "engine/animation/playback.h"
#include "engine/resource/animation_handles.h"

namespace engine {

/**
 * @brief Plays clips on its entity's skinned mesh, in up to kAnimatorLayers
 * stacked layers. AnimationSystem advances it and writes the entity's
 * SkinPose.
 *
 * Layer 0 is the base (e.g. locomotion); each later layer overrides the
 * ones below by its weight. Gameplay steers it with Play, PlayBlend and
 * Stop. Every clip must come from `clips`.
 */
struct Animator {
  /// The clips available, built for the entity's mesh rig (LoadClips).
  ClipSetHandle clips{ClipSetHandle::kInvalid};
  std::array<animation::LayerState, animation::kAnimatorLayers> layers{};
};

/** @brief How Play and PlayBlend start a clip. */
struct PlayOptions {
  /// Crossfade from what the layer plays now, in s; 0 cuts.
  float fade_s{0.2F};
  /// Playback rate; 1 is the authored speed.
  float speed{1.0F};
  bool loop{true};
  /// Start over even if the clip already plays on the layer.
  bool restart{false};
};

/**
 * @brief Plays @p clip on @p layer. If it already plays there, only its
 * speed and looping change, unless `restart` is set.
 */
inline void Play(Animator& animator, std::size_t layer, ClipHandle clip,
                 const PlayOptions& options = {}) {
  animation::LayerState& state = animator.layers.at(layer);
  if (!options.restart && state.current.clip == clip &&
      !state.current.blend_with.IsValid()) {
    state.current.speed = options.speed;
    state.current.loop = options.loop;
    return;
  }
  animation::StartTrack(
      state, {.clip = clip, .speed = options.speed, .loop = options.loop},
      options.fade_s);
}

/**
 * @brief Plays @p from and @p to blended by @p blend (0..1) at one phase on
 * @p layer, e.g. a walk and a run by speed. If that pair already plays,
 * only the blend, speed and looping change, unless `restart` is set.
 */
inline void PlayBlend(Animator& animator, std::size_t layer, ClipHandle from,
                      ClipHandle to, float blend,
                      const PlayOptions& options = {}) {
  animation::LayerState& state = animator.layers.at(layer);
  if (!options.restart && state.current.clip == from &&
      state.current.blend_with == to) {
    state.current.blend = blend;
    state.current.speed = options.speed;
    state.current.loop = options.loop;
    return;
  }
  animation::StartTrack(state,
                        {.clip = from,
                         .blend_with = to,
                         .blend = blend,
                         .speed = options.speed,
                         .loop = options.loop},
                        options.fade_s);
}

/** @brief Fades @p layer out to nothing over @p fade_s seconds. */
inline void Stop(Animator& animator, std::size_t layer, float fade_s) {
  animation::StartTrack(animator.layers.at(layer), {}, fade_s);
}

/** @brief Whether @p layer's non-looping clip has reached its end. */
inline auto IsFinished(const Animator& animator, std::size_t layer) -> bool {
  return animation::IsFinished(animator.layers.at(layer).current);
}

}  // namespace engine
