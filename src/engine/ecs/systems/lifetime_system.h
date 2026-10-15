#pragma once

namespace engine {
class Ecs;
struct FrameContext;

/**
 * @brief Counts every Lifetime down by the frame's dt and queues expired
 * entities for destruction. Run just before Ecs::Flush, which destroys them.
 */
auto ExpireLifetimes(Ecs& ecs, const FrameContext& ctx) -> void;

}  // namespace engine