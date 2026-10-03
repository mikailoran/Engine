#pragma once

#include <cstddef>
#include <source_location>
#include <string_view>

// TODO: Figure out Entity vs EntityType
using EntityType = std::size_t;
using Entity = std::size_t;
constexpr const EntityType kMaxEntities = 5000;

/**
 * @brief Compile-time identity for a type, used to key the component arrays.
 *
 * The value is the compiler's own signature for this function with @p T
 * substituted in, so it is distinct for every @p T. It points into static
 * storage and stays valid for the life of the program.
 *
 * Per-build only: the exact text is implementation-defined, so it differs
 * between compilers and must not be persisted or compared across builds.
 *
 * The key is the whole function signature (around 100 characters), so hashing
 * it costs more than hashing a pointer. Irrelevant at present entity counts;
 * if it ever shows up in a profile, hash the string into a std::size_t here
 * and nothing else has to change.
 *
 * @tparam T Type to identify.
 * @return Key unique to @p T within this build.
 */
using TypeKey = std::string_view;

template <class T>
consteval TypeKey TypeKeyOf() {
  return std::source_location::current().function_name();
}
