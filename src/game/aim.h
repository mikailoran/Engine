#pragma once

#include <bx/bounds.h>

namespace game {

/**
 * @brief Where an entity's weapon points this frame, and whether it fires.
 *
 * Added with a Weapon. Whoever controls the entity rewrites it every frame
 * before HandleShots and HandleEnemySpawns read it (for the player: triggers
 * in PlayerSystem::Control, ray in PlaceCamera); never authored.
 */
struct Aim {
  /// World-space shot: origin at the eyes, unit direction.
  bx::Ray ray{};
  /// True while the weapon should fire (held); Weapon's cooldown sets the rate.
  bool weapon_trigger{false};
  /// True on the frame an enemy should spawn where the ray hits (an edge).
  bool spawn_trigger{false};
};

}  // namespace game
