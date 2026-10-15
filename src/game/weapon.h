#pragma once

struct Weapon {
  /// Damage done by the weapon.
  int damage{10};
  /// Cooldown between shots in seconds.
  float cooldown_s{0.2F};
  /// Maximal distance the weapon can hit in meters.
  float range_m{1000.0F};

  // Runtime value.
  float cooldown_left_s{0.0F};
};