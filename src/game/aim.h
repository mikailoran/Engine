#pragma once

#include <bx/bounds.h>

namespace game {

/**
 * @brief Where an entity's weapon points this frame, and whether it fires.
 *
 * Added with a Weapon. Whoever controls the entity rewrites it every frame
 * before HandleShots reads it (for the player: trigger in
 * PlayerSystem::Control, ray in PlaceCamera); never authored.
 */
struct Aim {
  /// World-space shot: origin at the eyes, unit direction.
  bx::Ray ray{};
  /// True on the frame the weapon should fire (an edge, not a held button).
  bool trigger{false};
};

}  // namespace game
