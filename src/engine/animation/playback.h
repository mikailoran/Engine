#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "engine/resource/animation_handles.h"

namespace engine::animation {

// Playback state and its stepping: plain data and pure functions, so the
// ECS component embeds them and tests drive them without a rig.

/// Layers an Animator stacks; each may crossfade, so it fills twice as many
/// evaluation layers.
inline constexpr std::size_t kAnimatorLayers = 4;

/** @brief A clip, or two blended at one phase, playing on a layer. */
struct Track {
  /// Invalid: nothing plays.
  ClipHandle clip{ClipHandle::kInvalid};
  /// Optional second clip, mixed in by `blend` at the same phase.
  ClipHandle blend_with{ClipHandle::kInvalid};
  /// 0 plays `clip` alone, 1 `blend_with` alone.
  float blend{0.0F};
  /// Normalized time, 0 (start) to 1 (end).
  float phase{0.0F};
  /// Playback rate; 1 plays at the authored speed.
  float speed{1.0F};
  /// Wraps at the end when set, else holds the last pose.
  bool loop{true};
};

/** @brief One layer's playback: what plays, and what is fading out. */
struct LayerState {
  Track current;
  /// Fading out under `current`; invalid when no fade runs.
  Track previous;
  /// Length of the running fade, in s.
  float fade_s{0.0F};
  /// Time since the fade began, in s.
  float fade_elapsed_s{0.0F};
  /// 0..1: how much the layer overrides the layers below it.
  float weight{1.0F};
  /// Name of the joint whose subtree the layer covers (e.g. "spine_01"
  /// for the upper body); empty covers the whole skeleton.
  std::string mask;
};

/**
 * @brief Moves @p track on by @p dt seconds of a clip @p duration seconds
 * long, wrapping when it loops and stopping at the end when it doesn't.
 */
inline void Advance(Track& track, float dt, float duration) {
  if (!track.clip.IsValid()) {
    return;
  }
  track.phase += dt * track.speed / std::max(duration, 1e-4F);
  track.phase = track.loop ? track.phase - std::floor(track.phase)
                           : std::min(track.phase, 1.0F);
}

/** @brief Whether a non-looping @p track has reached its end. */
inline auto IsFinished(const Track& track) -> bool {
  return track.clip.IsValid() && !track.loop && track.phase >= 1.0F;
}

/**
 * @brief Makes @p next the layer's track, fading the current one out over
 * @p fade_s seconds; 0 (or nothing playing) switches at once.
 */
inline void StartTrack(LayerState& layer, const Track& next, float fade_s) {
  if (fade_s > 0.0F && layer.current.clip.IsValid()) {
    layer.previous = layer.current;
    layer.fade_s = fade_s;
    layer.fade_elapsed_s = 0.0F;
  } else {
    layer.previous = {};
  }
  layer.current = next;
}

/** @brief Moves the layer's fade on by @p dt, ending it when complete. */
inline void AdvanceFade(LayerState& layer, float dt) {
  if (!layer.previous.clip.IsValid()) {
    return;
  }
  layer.fade_elapsed_s += dt;
  if (layer.fade_elapsed_s >= layer.fade_s) {
    layer.previous = {};
  }
}

/** @brief How far the fade has come, 0 to 1; 1 when no fade runs. */
inline auto FadeAlpha(const LayerState& layer) -> float {
  if (!layer.previous.clip.IsValid()) {
    return 1.0F;
  }
  return std::clamp(layer.fade_elapsed_s / layer.fade_s, 0.0F, 1.0F);
}

}  // namespace engine::animation
