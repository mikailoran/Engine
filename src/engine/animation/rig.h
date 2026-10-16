#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "engine/resource/animation_clip.h"
#include "engine/resource/skeleton.h"

namespace engine::animation {

/**
 * @brief A Skeleton in the animation runtime's form, ready to pose.
 *
 * Built once per skinned mesh and shared by every entity drawing it. Joint
 * indices in and out of the runtime are the Skeleton's (skin order). Movable,
 * not copyable.
 */
class Rig {
 public:
  /**
   * @brief Builds the runtime skeleton from @p skeleton, keeping a copy.
   * @throws std::runtime_error If the runtime rejects the hierarchy.
   */
  explicit Rig(const Skeleton& skeleton);
  ~Rig();
  Rig(const Rig&) = delete;
  auto operator=(const Rig&) -> Rig& = delete;
  Rig(Rig&&) noexcept;
  auto operator=(Rig&&) noexcept -> Rig&;

  /** @brief The skeleton it was built from: names, parents, inverse binds. */
  [[nodiscard]] auto Source() const -> const Skeleton&;

  /** @brief Number of joints, as in Source(). */
  [[nodiscard]] auto JointCount() const -> std::size_t;

 private:
  friend class Clip;
  friend class Evaluator;
  struct State;
  std::unique_ptr<State> state_;
};

/**
 * @brief An AnimationClip in the runtime's compressed, sampleable form, for
 * one Rig.
 *
 * Joints the clip has no keys for hold their rest pose. Usable only with the
 * Rig it was built for. Movable, not copyable.
 */
class Clip {
 public:
  /**
   * @brief Builds @p clip for @p rig.
   * @param clip Tracks must index @p rig's skeleton (see RetargetClip).
   * @throws std::runtime_error If the runtime rejects the keys.
   */
  Clip(const AnimationClip& clip, const Rig& rig);
  ~Clip();
  Clip(const Clip&) = delete;
  auto operator=(const Clip&) -> Clip& = delete;
  Clip(Clip&&) noexcept;
  auto operator=(Clip&&) noexcept -> Clip&;

  /** @brief The glTF animation's name. */
  [[nodiscard]] auto Name() const -> const std::string&;

  /** @brief Length in seconds; positive. */
  [[nodiscard]] auto Duration() const -> float;

 private:
  friend class Evaluator;
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace engine::animation
