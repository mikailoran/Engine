#pragma once

#include <bgfx/bgfx.h>

namespace engine {

// bgfx renders views in id order, so these ids are the pass order.

/// Depth from the sun, sampled by the scene pass.
inline constexpr bgfx::ViewId kShadowView = 0;
/// Sky and Renderables, into the backbuffer; clears it.
inline constexpr bgfx::ViewId kSceneView = 1;
/// Debug lines over the scene, against its depth.
inline constexpr bgfx::ViewId kDebugView = 2;
/// Screen-space overlays (the crosshair), over everything, no depth.
inline constexpr bgfx::ViewId kHudView = 3;

}  // namespace engine
