#include "engine/scene/picking.h"

#include <bgfx_utils.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <array>
#include <optional>

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/screen.h"
#include "engine/resource/asset_registry.h"

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

}  // namespace

auto Pick(Ecs& ecs, const AssetRegistry& assets, Entity camera,
          ScreenPosition position, ScreenSize size) -> std::optional<Entity> {
  const auto ray =
      ScreenPositionToRay(ecs.GetComponent<Camera>(camera),
                          ecs.GetComponent<Transform>(camera), position, size);
  const auto hit = PickEntity(ecs, assets, ray);
  return hit ? std::optional<Entity>{hit->entity} : std::nullopt;
}
