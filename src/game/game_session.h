#pragma once

#include "engine/ecs/core/types.h"
#include "game/player_system.h"

namespace engine {
class Engine;
struct FrameContext;
}  // namespace engine

namespace game {

/**
 * @brief One run of the game: its gameplay systems and its own entities, run
 * on an engine the host owns.
 *
 * Each frame the host calls PrePhysics, Engine::Update, then PostPhysics. A
 * new session starts a fresh run; Engine::ClearWorld destroys its entities,
 * so start a new session after clearing.
 */
class GameSession {
 public:
  /** @brief Starts a run: spawns the camera the player looks through. */
  explicit GameSession(engine::Engine& engine);

  /**
   * @brief Turns this frame's input into the player's velocity and facing.
   * Before Engine::Update.
   * @param has_control False stops the player where it stands.
   */
  void PrePhysics(engine::Engine& engine, const engine::FrameContext& ctx,
                  bool has_control);

  /** @brief Moves the view camera to the player's eyes. After Engine::Update.
   */
  void PostPhysics(engine::Engine& engine,
                   const engine::FrameContext& ctx) const;

  /**
   * @brief The camera this run views the game from. The host decides whether
   * to make it active.
   */
  [[nodiscard]] auto ViewCamera() const -> engine::Entity;

 private:
  PlayerSystem player_;
  engine::Entity camera_{};
};

}  // namespace game
