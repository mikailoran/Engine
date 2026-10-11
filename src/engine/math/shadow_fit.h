#pragma once

#include <bx/math.h>

#include <array>
#include <cstdint>

namespace engine {

/**
 * @brief The part of a camera's view a directional shadow map must cover.
 *
 * Distances are metres from the eye along the view axis.
 */
struct ShadowFitInput {
  /// Camera position.
  bx::Vec3 eye{0.0F, 0.0F, 0.0F};
  /// Camera rotation; the camera looks along local -Z.
  bx::Quaternion rotation{0.0F, 0.0F, 0.0F, 1.0F};
  /// Vertical field of view.
  float fov_degrees{60.0F};
  /// Viewport width over height.
  float aspect{1.0F};
  /// Start of the shadowed slice, usually the camera's near plane.
  float slice_near{0.1F};
  /// End of the shadowed slice; nothing farther is shadowed.
  float slice_far{40.0F};
  /// World direction toward the light. Need not be normalized.
  bx::Vec3 light_dir{0.0F, 1.0F, 0.0F};
  /// Shadow map texels per side.
  std::uint16_t resolution{2048};
  /// Metres the light volume extends past the slice toward the light, so
  /// casters outside the view still shadow it.
  float caster_margin{50.0F};
};

/** @brief The light's view and orthographic projection for a shadow map. */
struct LightMatrices {
  std::array<float, 16> view{};
  std::array<float, 16> proj{};
  /// World-space width of one shadow map texel, in metres.
  float texel_size{0.0F};
};

/**
 * @brief Fits a directional light's ortho volume around a camera's slice.
 *
 * The volume bounds a sphere around the slice, so its size depends only on the
 * lens, and its position snaps to whole texels; shadows therefore neither
 * resize when the camera turns nor shimmer when it moves.
 *
 * @param homogeneous_depth NDC depth -1..1 if true, else 0..1; pass
 *        bgfx::getCaps()->homogeneousDepth when rendering.
 */
auto FitDirectionalShadow(const ShadowFitInput& input, bool homogeneous_depth,
                          bx::Handedness::Enum handedness) -> LightMatrices;

/**
 * @brief World to shadow map texture space: UV in x and y, compared depth 0..1
 * in z.
 *
 * Pass the same @p homogeneous_depth as to FitDirectionalShadow, and
 * bgfx::getCaps()->originBottomLeft for @p origin_bottom_left.
 */
auto ShadowTextureMatrix(const LightMatrices& light, bool homogeneous_depth,
                         bool origin_bottom_left) -> std::array<float, 16>;

}  // namespace engine
