#pragma once

#include <bx/math.h>

namespace engine {

/**
 * @brief The background drawn where no geometry covers the screen.
 *
 * A gradient from horizon to zenith with the sun's disc and glow, placed and
 * tinted by the scene's DirectionalLight. At most one per scene; without one,
 * the default is drawn. Colors are linear RGB.
 */
struct Sky {
  /// Color straight up.
  bx::Vec3 zenith{0.15F, 0.35F, 0.75F};
  /// Color at the horizon.
  bx::Vec3 horizon{0.6F, 0.7F, 0.85F};
  /// Color below the horizon.
  bx::Vec3 ground{0.12F, 0.1F, 0.08F};
  /// Exponent from horizon to zenith; below 1 keeps the pale band narrow.
  float gradient_falloff{0.5F};

  /// Angular radius of the sun's disc, in degrees. The real sun's is ~0.27.
  float sun_radius_deg{2.0F};
  /// How quickly the glow fades away from the sun; higher is tighter.
  float glow_sharpness{200.0F};
  /// Glow brightness at the sun's centre, relative to the light's color.
  float glow_strength{0.3F};
};

}  // namespace engine
