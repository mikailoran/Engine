#include "ecs/systems/selection_system.h"

#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <entry/entry.h>

#include <array>
#include <optional>
#include <vector>

#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
#include "ecs/components/selected.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "platform/frame_context.h"
#include "platform/screen.h"
#include "resource/asset_registry.h"

namespace {

/** @brief An entity hit by a pick ray. */
struct PickHit {
  Entity entity{};
  // World-space distance from the ray origin.
  float distance{0.0F};
};

/** @brief Finds the Renderable whose mesh bounds @p ray hits first. */
auto PickEntity(Ecs& ecs, const AssetRegistry& assets, const bx::Ray& ray)
    -> std::optional<PickHit> {
  std::optional<PickHit> nearest;

  ecs.View<Transform, Renderable>().ForEach(
      [&](Entity entity, const Transform& transform,
          const Renderable& renderable) -> void {
        const auto model = ModelMatrix(transform);
        std::array<float, 16> inv_model{};
        bx::mtxInverse(inv_model.data(), model.data());

        // Test in mesh space so rotation and scale need no world bounds
        bx::Ray local_ray;
        local_ray.pos = bx::mulH(ray.pos, inv_model.data());
        local_ray.dir = bx::normalize(bx::mulXyz0(ray.dir, inv_model.data()));

        const Mesh* mesh = assets.GetMesh(renderable.mesh_handle);
        for (const Group& group : mesh->m_groups) {
          bx::Hit hit;
          if (!bx::intersect(local_ray, group.m_aabb, &hit)) {
            continue;
          }

          // Compare in world space; local distances differ by scale
          const bx::Vec3 world_pos = bx::mulH(hit.pos, model.data());
          const float distance = bx::distance(world_pos, ray.pos);
          if (!nearest || distance < nearest->distance) {
            nearest = PickHit{.entity = entity, .distance = distance};
          }
        }
      });

  return nearest;
}

/** @brief Makes @p entity the only one carrying Selected, or none if empty. */
void ApplySelection(Ecs& ecs, std::optional<Entity> entity) {
  // Gather first: removing a viewed component inside ForEach asserts
  // TODO: implement command buffer to allow component removal in view
  // iteration.
  std::vector<Entity> deselect;
  ecs.View<Selected>().ForEach([&](Entity selected, Selected&) -> void {
    if (selected != entity) {
      deselect.push_back(selected);
    }
  });

  for (const Entity selected : deselect) {
    ecs.RemoveComponent<Selected>(selected);
  }

  if (entity && !ecs.HasComponent<Selected>(*entity)) {
    ecs.AddComponent(*entity, Selected{});
  }
}

}  // namespace

void SelectionSystem::Update(Ecs& ecs, const AssetRegistry& assets,
                             const FrameContext& ctx, bool mouse_over_ui) {
  if (ctx.mouse == nullptr) {
    return;
  }

  const bool left_down = ctx.mouse->m_buttons[entry::MouseButton::Left] != 0;
  const bool pressed = left_down && !was_left_down_;
  was_left_down_ = left_down;

  // Only a fresh click on the scene, once the window has a size
  if (!pressed || mouse_over_ui || !camera_ || ctx.width <= 1 ||
      ctx.height <= 1) {
    return;
  }

  const auto& camera = ecs.GetComponent<Camera>(*camera_);
  const bx::Ray ray =
      ScreenPointToRay(camera,
                       ScreenPoint{.x = static_cast<float>(ctx.mouse->m_mx),
                                   .y = static_cast<float>(ctx.mouse->m_my)},
                       ScreenSize{.width = static_cast<float>(ctx.width),
                                  .height = static_cast<float>(ctx.height)});

  const auto hit = PickEntity(ecs, assets, ray);
  ApplySelection(ecs, hit ? std::optional<Entity>{hit->entity} : std::nullopt);
}

void SelectionSystem::SetCamera(Entity camera) { camera_ = camera; }
