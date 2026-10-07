#pragma once

#include <cstdint>

/** @brief Opaque id of a body in a PhysicsWorld. Stale ids never alias. */
class BodyHandle {
 public:
  static constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFU;

  /** @brief Names no body. */
  BodyHandle() = default;

  /** @brief Wraps a raw id; only PhysicsWorld makes valid ones. */
  explicit BodyHandle(std::uint32_t value) : value_(value) {}

  /** @brief The raw id, for display and comparison. */
  [[nodiscard]] auto Value() const -> std::uint32_t { return value_; }

  /** @brief Whether this names a body (it may since have been destroyed). */
  [[nodiscard]] auto IsValid() const -> bool {
    return value_ != kInvalidHandle;
  }

  /** @brief Compares the ids. */
  auto operator==(const BodyHandle&) const -> bool = default;

 private:
  std::uint32_t value_{kInvalidHandle};
};
