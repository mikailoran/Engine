#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>

#include <memory>

/**
 * @brief Owns Jolt's process-wide state: allocator, callbacks, factory and
 * registered types.
 *
 * Must be built before, and destroyed after, every other Jolt object. At most
 * one instance may exist.
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
  /// Published through JPH::Factory::sInstance while this object lives.
  std::unique_ptr<JPH::Factory> factory_;
};
