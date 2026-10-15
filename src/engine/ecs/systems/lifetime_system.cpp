#include "engine/ecs/systems/lifetime_system.h"

#include "engine/ecs/components/lifetime.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/platform/frame_context.h"

namespace engine {

auto ExpireLifetimes(Ecs& ecs, const FrameContext& ctx) -> void {
  const auto dt = ctx.dt;
  ecs.View<Lifetime>().ForEach(
      [&ecs, dt](Entity entity, Lifetime& lifetime) -> void {
        lifetime.remaining_s -= dt;

        if (lifetime.remaining_s <= 0.0F) {
          ecs.DestroyEntity(entity);
        }
      });
}

}  // namespace engine