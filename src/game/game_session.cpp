#include "game/game_session.h"

#include <bx/math.h>

#include <optional>

#include "engine/ecs/components/attached_meshes.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/hud/crosshair.h"
#include "engine/platform/frame_context.h"
#include "engine/resource/asset_registry.h"
#include "game/enemy_spawning.h"
#include "game/enemy_system.h"
#include "game/player_system.h"
#include "game/shooting_system.h"

using engine::AttachedMesh;
using engine::AttachedMeshes;
using engine::Camera;
using engine::CrosshairStyle;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::Transform;

namespace game {

namespace {

/// The player's crosshair; the engine's default style for now.
constexpr CrosshairStyle kCrosshair{};

/// The gun drawn before the player's eyes.
constexpr const char* kViewGun = "assets/meshes/weapons/AssaultRifle_1.glb";
/// Its muzzle in its own frame, in m (printed by tools/assets/export_glb.py).
constexpr bx::Vec3 kViewGunMuzzle{0.0F, 0.1567F, -0.8454F};

}  // namespace

GameSession::GameSession(Engine& engine) {
  Ecs& ecs = engine.World();
  // PlayerSystem moves this one to the player's eyes
  camera_ = ecs.CreateEntity();
  ecs.AddComponent(camera_, Transform{});
  ecs.AddComponent(camera_, Camera{});
  player_.SetViewCamera(camera_);

  // The view gun hangs from the camera, its muzzle where beams start
  const Transform gun_offset{.position = bx::sub(kViewMuzzle, kViewGunMuzzle)};
  ecs.AddComponent(
      camera_, AttachedMeshes{.meshes = {AttachedMesh{
                                  .mesh = engine.Assets().LoadMesh(kViewGun),
                                  .joint = std::nullopt,
                                  .offset = engine::ModelMatrix(gun_offset),
                                  .bind_space = false}}});
}

void GameSession::PrePhysics(Engine& engine, const FrameContext& ctx,
                             bool has_control) {
  player_.Control(engine.World(), ctx, has_control);
  DriveEnemies(engine.World(), ctx.dt, has_control);
  // Game time stands still without control (F1), as physics does
  if (has_control) {
    UpdateCooldowns(engine.World(), ctx.dt);
  }
}

void GameSession::PostPhysics(Engine& engine) const {
  player_.PlaceCamera(engine.World());
  AimEnemies(engine);
  HandleEnemySpawns(engine);
  HandleShots(engine);
  engine.DrawCrosshair(kCrosshair);
}

auto GameSession::ViewCamera() const -> Entity { return camera_; }

}  // namespace game
