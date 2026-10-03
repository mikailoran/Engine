#pragma once

#include <bgfx/bgfx.h>

#include <concepts>
#include <utility>

/** @brief A bgfx handle type that bgfx::destroy and bgfx::isValid accept. */
template <class H>
concept BgfxHandle = requires(H handle) {
  bgfx::destroy(handle);
  { bgfx::isValid(handle) } -> std::same_as<bool>;
};

/**
 * @brief Sole owner of one bgfx handle; destroys it on destruction.
 *
 * Move-only, like std::unique_ptr. A default or moved-from instance holds
 * the invalid handle and destroys nothing. Must be destroyed before
 * bgfx::shutdown.
 */
template <BgfxHandle H>
class UniqueHandle {
 public:
  /** @brief Holds the invalid handle. */
  UniqueHandle() noexcept = default;

  /** @brief Takes ownership of @p handle, which may be invalid. */
  explicit UniqueHandle(H handle) noexcept : handle_(handle) {}

  /** @brief Destroys the held handle, if valid. */
  ~UniqueHandle() { Reset(); }

  UniqueHandle(const UniqueHandle&) = delete;
  auto operator=(const UniqueHandle&) -> UniqueHandle& = delete;

  /** @brief Takes @p other's handle, leaving it invalid. */
  UniqueHandle(UniqueHandle&& other) noexcept
      : handle_(std::exchange(other.handle_, kInvalid)) {}

  /** @brief Destroys the held handle, then takes @p other's. */
  auto operator=(UniqueHandle&& other) noexcept -> UniqueHandle& {
    if (this != &other) {
      Reset();
      handle_ = std::exchange(other.handle_, kInvalid);
    }
    return *this;
  }

  /** @brief Returns the handle, still owned by this object. */
  [[nodiscard]] auto Get() const noexcept -> H { return handle_; }

  /** @brief True if a valid handle is held. */
  [[nodiscard]] explicit operator bool() const noexcept {
    return bgfx::isValid(handle_);
  }

  /** @brief Destroys the held handle, if valid, and becomes invalid. */
  void Reset() noexcept {
    if (bgfx::isValid(handle_)) {
      bgfx::destroy(handle_);
    }
    handle_ = kInvalid;
  }

 private:
  static constexpr H kInvalid{bgfx::kInvalidHandle};

  H handle_{kInvalid};
};
