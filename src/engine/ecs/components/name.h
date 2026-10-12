#pragma once

#include <string>

namespace engine {

/**
 * @brief A human-readable label for an entity, shown by tools and in logs.
 *
 * Not an identifier: names aren't unique and never look an entity up. Scene
 * entities get one from their "name"; runtime-only entities have none.
 */
struct Name {
  /// The label as authored in the scene file.
  std::string value;
};

}  // namespace engine
