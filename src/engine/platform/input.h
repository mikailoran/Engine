#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/platform/screen.h"

namespace engine {

/** @brief Mouse buttons the engine reads. */
enum class MouseButton : std::uint8_t { kLeft, kRight, kMiddle };

/** @brief Keyboard keys the engine reads, by physical position. */
enum class Key : std::uint8_t {
  // Letters: fly-camera movement and text-field shortcuts
  kW,
  kA,
  kS,
  kD,
  kQ,
  kE,
  kC,
  kV,
  kX,
  kY,
  kZ,
  // Editing and navigation
  kTab,
  kLeft,
  kRight,
  kUp,
  kDown,
  kPageUp,
  kPageDown,
  kHome,
  kEnd,
  kDelete,
  kBackspace,
  kEnter,
  kEscape,
  // Modifiers
  kLeftCtrl,
  kRightCtrl,
  kLeftShift,
  kRightShift,
  kLeftAlt,
  kRightAlt,
  // Number of keys above; not a key
  kCount,
};

/**
 * @brief One frame of mouse and keyboard input, translated from the host's
 * own events.
 *
 * The host calls BeginFrame once per frame, then records that frame's state.
 * Positions are in backbuffer pixels, matching FrameContext's size.
 */
class Input {
 public:
  /**
   * @brief Starts a frame: keeps last frame's buttons, zeroes the wheel and
   * clears the text.
   */
  void BeginFrame() {
    was_down_ = down_;
    wheel_ = 0.0F;
    text_.clear();
  }

  /** @brief Records whether @p button is held. */
  void SetButton(MouseButton button, bool down) {
    down_.at(Index(button)) = down;
  }

  /** @brief Records whether @p key is held. */
  void SetKey(Key key, bool down) { keys_.at(Index(key)) = down; }

  /** @brief Records the cursor position. */
  void SetMouse(ScreenPosition position) { mouse_ = position; }

  /** @brief Adds wheel notches for this frame; positive is away from you. */
  void AddWheel(float notches) { wheel_ += notches; }

  /** @brief Appends text typed this frame, as UTF-8. */
  void AddText(std::string_view utf8) { text_ += utf8; }

  /** @brief Cursor position in backbuffer pixels, origin top-left. */
  [[nodiscard]] auto Mouse() const -> ScreenPosition { return mouse_; }

  /** @brief Wheel notches this frame; positive is away from you. */
  [[nodiscard]] auto Wheel() const -> float { return wheel_; }

  /**
   * @brief Text typed this frame, as UTF-8, after the layout and any input
   * method. Hosts send it only while something has asked for text input.
   */
  [[nodiscard]] auto Text() const -> const std::string& { return text_; }

  /** @brief Whether @p button is held. */
  [[nodiscard]] auto Down(MouseButton button) const -> bool {
    return down_.at(Index(button));
  }

  /** @brief Whether @p key is held. */
  [[nodiscard]] auto Down(Key key) const -> bool {
    return keys_.at(Index(key));
  }

  /** @brief Whether @p button went down this frame. */
  [[nodiscard]] auto Pressed(MouseButton button) const -> bool {
    return down_.at(Index(button)) && !was_down_.at(Index(button));
  }

 private:
  static constexpr std::size_t kButtonCount = 3;
  static constexpr auto kKeyCount = static_cast<std::size_t>(Key::kCount);

  /** @brief Array slot for @p button. */
  static constexpr auto Index(MouseButton button) -> std::size_t {
    return static_cast<std::size_t>(button);
  }

  /** @brief Array slot for @p key. */
  static constexpr auto Index(Key key) -> std::size_t {
    return static_cast<std::size_t>(key);
  }

  // Held state this frame and last frame, indexed by MouseButton.
  std::array<bool, kButtonCount> down_{};
  std::array<bool, kButtonCount> was_down_{};

  // Held state this frame, indexed by Key.
  std::array<bool, kKeyCount> keys_{};

  ScreenPosition mouse_{};
  float wheel_{0.0F};
  std::string text_;
};

}  // namespace engine
