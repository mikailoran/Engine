#pragma once

#include <array>
#include <bx/math.h>
#include <cstddef>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

// Helpers for component loaders reading scene JSON. All throw on bad input.

// TODO: Add namespace

/** @brief Throws if @p data has a key outside @p allowed. */
inline void CheckKeys(const nlohmann::json &data,
                      std::initializer_list<std::string_view> allowed) {
  for (const auto &item : data.items()) {
    bool known = false;
    for (const auto key : allowed) {
      known = known || item.key() == key;
    }
    if (!known) {
      throw std::runtime_error("unknown field '" + item.key() + "'");
    }
  }
}

/** @brief Reads a JSON array of exactly @p N numbers. */
template <std::size_t N>
auto ReadFloats(const nlohmann::json &data) -> std::array<float, N> {
  if (!data.is_array() || data.size() != N) {
    throw std::runtime_error("expected an array of " + std::to_string(N) +
                             " numbers, got " + data.dump());
  }
  return data.get<std::array<float, N>>();
}

/** @brief Reads a JSON array of 3 numbers into a bx::Vec3. */
inline auto ReadVec3(const nlohmann::json &data) -> bx::Vec3 {
  const auto values = ReadFloats<3>(data);
  return {values[0], values[1], values[2]};
}
