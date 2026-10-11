#pragma once

#include <QWindow>

#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

class QExposeEvent;

namespace editor {

/**
 * @brief The native window the engine renders the game into.
 *
 * Embedded in the main window through QWidget::createWindowContainer, which
 * takes ownership of it. Qt never paints it: bgfx presents to it directly.
 */
class ViewportWindow : public QWindow {
  Q_OBJECT

 public:
  /** @brief Creates the window as a Vulkan surface, for bgfx to present to. */
  explicit ViewportWindow(QWindow* parent = nullptr);

 protected:
  /** @brief Called when the window is shown on screen or hidden. */
  void exposeEvent(QExposeEvent* event) override;

 private:
  /**
   * @brief The native handles and backbuffer size, for the engine. Valid once
   * the window has been exposed.
   * @throws std::runtime_error If Qt runs on neither Wayland nor xcb, or a
   *         handle is missing.
   */
  [[nodiscard]] auto Surface() const -> engine::NativeSurface;

  /** @brief Current size in device pixels: logical size times the scale. */
  [[nodiscard]] auto BackbufferSize() const -> engine::PixelSize;
};

}  // namespace editor
