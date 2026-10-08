#pragma once

#include <concepts>
#include <cstdint>
#include <limits>

namespace engine {

/**
 * @brief Opaque id issued by an owner, typed by @p Tag so ids of different
 * owners never mix. Each alias documents who issues it and whether it can go
 * stale.
 *
 * @tparam Tag Any type, usually declared in place: Handle<struct BodyTag>.
 * @tparam Raw The stored integer; its maximum names nothing.
 */
template <class Tag, std::unsigned_integral Raw = std::uint32_t>
class Handle {
 public:
  static constexpr Raw kInvalid = std::numeric_limits<Raw>::max();

  /** @brief Names nothing. */
  constexpr Handle() = default;

  /** @brief Wraps a raw id; only the issuing owner makes valid ones. */
  constexpr explicit Handle(Raw value) : value_(value) {}

  /** @brief The raw id, for the owner's lookups and for display. */
  [[nodiscard]] constexpr auto Value() const -> Raw { return value_; }

  /** @brief Whether this names something (it may since have been freed). */
  [[nodiscard]] constexpr auto IsValid() const -> bool {
    return value_ != kInvalid;
  }

  /** @brief Compares the ids. */
  constexpr auto operator==(const Handle&) const -> bool = default;

 private:
  Raw value_{kInvalid};
};

}  // namespace engine
