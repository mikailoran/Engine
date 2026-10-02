#pragma once

/** @brief Non-copyable base for systems, owned by SystemManager. */
struct System {
  // TODO: Make System virtual
  virtual ~System() = default;
  System() = default;
  System(const System &) = delete;
  System &operator=(const System &) = delete;
};
