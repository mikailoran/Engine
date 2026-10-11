#include "game/logic/game_session.h"

#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/frame_context.h"

using engine::Camera;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::Transform;

namespace game {

GameSession::GameSession(Engine& engine) {
  Ecs& ecs = engine.World();
  // PlayerSystem moves this one to the player's eyes
  camera_ = ecs.CreateEntity();
  ecs.AddComponent(camera_, Transform{});
  ecs.AddComponent(camera_, Camera{});
  player_.SetViewCamera(camera_);
}

void GameSession::PrePhysics(Engine& engine, const FrameContext& ctx,
                             bool has_control) {
  player_.Control(engine.World(), ctx, has_control);
}

void GameSession::PostPhysics(Engine& engine) const {
  player_.PlaceCamera(engine.World());
}

auto GameSession::ViewCamera() const -> Entity { return camera_; }

}  // namespace game
