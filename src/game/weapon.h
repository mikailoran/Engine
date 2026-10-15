#pragma once

struct Weapon {
  /// Damage done by the weapon.
  int damage{10};
  /// Cooldown between shots in seconds.
  float cooldown_s{0.2F};

  // Runtime value.
  float cooldown_left_s{0.0F};
};