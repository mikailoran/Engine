#pragma once

#include <array>

namespace engine {

/**
 * @brief How the screen-centre crosshair looks; requested each frame with
 * Engine::DrawCrosshair.
 *
 * Sizes are in backbuffer pixels. Colors are display RGBA (no gamma is
 * applied), alpha blended over the frame.
 */
struct CrosshairStyle {
  /// Distance from the centre to where each arm starts; 0 joins the arms.
  float gap{4.0F};
  /// Length of each of the four arms; 0 draws none.
  float length{8.0F};
  /// Width of each arm.
  float thickness{2.0F};
  /// Width of the border around the arms and dot; 0 draws none.
  float outline{1.0F};
  /// Radius of the centre dot; 0 draws none.
  float dot_radius{0.0F};
  /// Fill of the arms and dot.
  std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
  /// Border color.
  std::array<float, 4> outline_color{0.0F, 0.0F, 0.0F, 0.6F};
};

}  // namespace engine
