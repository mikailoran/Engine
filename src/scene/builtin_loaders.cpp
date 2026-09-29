#include "builtin_loaders.h"

#include "scene_loader.h"

#include "../ecs/components/renderable.h"
#include "../ecs/components/transform.h"
#include "../ecs/core/ecs.h"
#include "../resource/asset_registry.h"

#include <array>
#include <bx/math.h>
#include <cstddef>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using nlohmann::json;

/** @brief Throws if @p data has a key outside @p allowed. */
void CheckKeys(const json &data,
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
auto ReadFloats(const json &data) -> std::array<float, N> {
  if (!data.is_array() || data.size() != N) {
    throw std::runtime_error("expected an array of " + std::to_string(N) +
                             " numbers, got " + data.dump());
  }
  return data.get<std::array<float, N>>();
}

/** @brief Reads a JSON array of 3 numbers into a bx::Vec3. */
auto ReadVec3(const json &data) -> bx::Vec3 {
  const auto values = ReadFloats<3>(data);
  return {values[0], values[1], values[2]};
}

/** @brief Adds a Transform; every field is optional. Rotation is in degrees. */
void LoadTransform(const json &data, Entity entity, SceneLoadContext &ctx) {
  CheckKeys(data, {"position", "rotation_deg", "scale"});

  Transform transform{};
  if (data.contains("position")) {
    transform.position = ReadVec3(data.at("position"));
  }
  if (data.contains("rotation_deg")) {
    const auto deg = ReadVec3(data.at("rotation_deg"));
    transform.rotation = {bx::toRad(deg.x), bx::toRad(deg.y), bx::toRad(deg.z)};
  }
  if (data.contains("scale")) {
    transform.scale = ReadVec3(data.at("scale"));
  }
  ctx.ecs.AddComponent(entity, transform);
}

/** @brief Adds a Renderable; "mesh" is required, "color" optional. */
void LoadRenderable(const json &data, Entity entity, SceneLoadContext &ctx) {
  CheckKeys(data, {"mesh", "color"});

  Renderable renderable{};
  renderable.mesh_handle =
      ctx.assets.LoadMesh(data.at("mesh").get<std::string>());
  if (data.contains("color")) {
    renderable.color = ReadFloats<4>(data.at("color"));
  }
  ctx.ecs.AddComponent(entity, renderable);
}

} // namespace

void RegisterBuiltinLoaders(SceneLoader &loader) {
  loader.Register("transform", LoadTransform);
  loader.Register("renderable", LoadRenderable);
}
