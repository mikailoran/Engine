#pragma once

#include <memory>

namespace engine::physics {

/**
 * @brief Owns Jolt's process-wide state: allocator, callbacks, factory and
 * registered types.
 *
 * Must be built before, and destroyed after, every other Jolt object. A second
 * live instance asserts, and throws std::logic_error in Release.
 */
class JoltRuntime {
 public:
  /** @brief Installs Jolt's allocator and callbacks, then registers types. */
  JoltRuntime();

  /** @brief Unregisters Jolt's types and destroys its factory. */
  ~JoltRuntime();

  // Jolt's state is process-wide: exactly one owner
  JoltRuntime(const JoltRuntime&) = delete;
  auto operator=(const JoltRuntime&) -> JoltRuntime& = delete;
  JoltRuntime(JoltRuntime&&) = delete;
  auto operator=(JoltRuntime&&) -> JoltRuntime& = delete;

 private:
  /// Jolt's factory, defined in the .cpp so this header needs no Jolt.
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace engine::physics
