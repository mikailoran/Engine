#pragma once

#include <bx/bounds.h>
#include <bx/bx.h>
#include <bx/math.h>

namespace game {

/**
 * @brief Where an entity's weapon points this frame, and whether it fires.
 *
 * Added with a Weapon. Whoever controls the entity rewrites it every frame
 * before HandleShots and HandleEnemySpawns read it (for the player: triggers
 * in PlayerSystem::Control, ray and muzzle in PlaceCamera; for enemies: the
 * trigger in DriveEnemies, ray and muzzle in AimEnemies); never authored.
 */
struct Aim {
  /// World-space shot: origin at the eyes (the muzzle for enemies), unit
  /// direction.
  bx::Ray ray{};
  /// World-space point the shot's beam starts from: the gun's muzzle.
  bx::Vec3 muzzle{bx::InitZero};
  /// True while the weapon should fire (held); Weapon's cooldown sets the rate.
  bool weapon_trigger{false};
  /// True on the frame an enemy should spawn where the ray hits (an edge).
  bool spawn_trigger{false};
};

}  // namespace game
