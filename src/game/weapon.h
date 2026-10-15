#pragma once

struct Weapon {
  /// Damage done by the weapon.
  int damage{10};
  /// Cooldown between shots in seconds.
  float cooldown_s{0.2F};
  /// Maximal distance the weapon can hit in meters.
  float range_m{1000.0F};
  /// How long each shot's beam shows, in s.
  float beam_s{0.1F};

  /// Runtime: s until the next shot, counting down; fires only at 0.
  float cooldown_left_s{0.0F};
};