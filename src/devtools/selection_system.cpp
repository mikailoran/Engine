#include "devtools/selection_system.h"

#include <optional>
#include <vector>

#include "devtools/selected.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"

using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::MouseButton;
using engine::ScreenSize;

namespace devtools {

namespace {

/** @brief Removes Selected from every entity. */
void DeselectAll(Ecs& ecs) {
  // Gather first: removing a viewed component inside ForEach asserts
  // TODO: implement command buffer to allow component removal in view
  // iteration.
  std::vector<Entity> deselect;
  ecs.View<Selected>().ForEach([&](Entity selected, Selected&) -> void {
    deselect.push_back(selected);
  });

  for (const Entity selected : deselect) {
    ecs.RemoveComponent<Selected>(selected);
  }
}

}  // namespace

void UpdateSelection(Engine& engine, const FrameContext& ctx,
                     bool mouse_over_ui) {
  // Only a fresh click on the scene, once the window has a size
  if (!ctx.input.Pressed(MouseButton::kLeft) || mouse_over_ui ||
      ctx.width <= 1 || ctx.height <= 1) {
    return;
  }

  const auto screen_size = ScreenSize{.width = static_cast<float>(ctx.width),
                                      .height = static_cast<float>(ctx.height)};
  const auto pick = engine.Pick(ctx.input.Mouse(), screen_size);

  // A hit becomes the only selection; a miss clears it
  auto& ecs = engine.World();
  DeselectAll(ecs);
  if (pick) {
    ecs.AddComponent(*pick, Selected{});
  }
}

}  // namespace devtools
