#include "ecs/systems/ui.h"

#include <bx/math.h>
#include <dear-imgui/imgui.h>
#include <entry/entry.h>
#include <imgui/imgui.h>

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <string>

#include "ecs/components/collider.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/selected.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/types.h"
#include "math/rotation.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"
#include "resource/mesh_handle.h"
#include "resource/texture_handle.h"

namespace {

/** @brief Spawns a bunny. @return The new entity. */
auto SpawnEntity(Ecs& ecs, AssetRegistry& assets) -> Entity {
  // TODO: remove paths
  const MeshHandle mesh_handle = assets.LoadMesh("assets/meshes/bunny.bin");
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Transform{.position = {0.0F, 3.0F, 0.0F}});
  ecs.AddComponent(entity, RigidBody{});
  ecs.AddComponent(entity, Collider{});
  ecs.AddComponent(entity, Renderable{.mesh_handle = mesh_handle});

  return entity;
}

/**
 * @brief Draws @p Component's section if @p entity has one; its close button
 * removes it.
 */
template <class Component, std::invocable<Component&> Fn>
auto DrawComponent(Ecs& ecs, Entity entity, const char* name, Fn draw) -> void {
  auto* component = ecs.TryGetComponent<Component>(entity);
  if (component == nullptr) {
    return;
  }

  bool keep = true;
  if (ImGui::CollapsingHeader(name, &keep, ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushID(name);
    draw(*component);
    ImGui::PopID();
  }
  // Last use of component: removal invalidates the pointer
  if (!keep) {
    ecs.RemoveComponent<Component>(entity);
  }
}

/** @brief Menu item that adds @p make()'s result if @p entity lacks one. */
template <class Component, std::invocable<> Make>
auto AddComponentMenuItem(Ecs& ecs, Entity entity, const char* name, Make make)
    -> void {
  if (!ecs.HasComponent<Component>(entity) && ImGui::MenuItem(name)) {
    ecs.AddComponent(entity, make());
  }
}

/** @brief Menu item that adds a default @p Component if @p entity lacks one. */
template <class Component>
auto AddComponentMenuItem(Ecs& ecs, Entity entity, const char* name) -> void {
  AddComponentMenuItem<Component>(ecs, entity, name,
                                  []() -> Component { return Component{}; });
}

/**
 * @brief Draws the selected entity's components, its Destroy button and its
 * Add Component menu.
 */
auto DrawInspector(Ecs& ecs, AssetRegistry& assets) -> void {
  ecs.View<Selected>().ForEach([&ecs, &assets](Entity entity,
                                               const Selected&) -> void {
    ImGui::PushID(static_cast<int>(entity));
    ImGui::TextUnformatted(std::format("Entity {}", entity).c_str());
    ImGui::SameLine();
    if (ImGui::Button("Destroy")) {
      ecs.DestroyEntity(entity);
    }
    ImGui::SeparatorText("Components");
    DrawComponent<Transform>(
        ecs, entity, "Transform", [](Transform& transform) -> void {
          ImGui::DragFloat3("Position", &transform.position.x, 0.1F, 0.0F, 0.0F,
                            "%.1f");
          // Edited as Euler degrees; written back only on change, since the
          // round trip would otherwise rewrite the quaternion every frame
          const bx::Vec3 euler = QuatToEuler(transform.rotation);
          std::array<float, 3> degrees{bx::toDeg(euler.x), bx::toDeg(euler.y),
                                       bx::toDeg(euler.z)};
          if (ImGui::DragFloat3("Rotation", degrees.data(), 1.0F, 0.0F, 0.0F,
                                "%.1f")) {
            transform.rotation =
                EulerToQuat({bx::toRad(degrees.at(0)), bx::toRad(degrees.at(1)),
                             bx::toRad(degrees.at(2))});
          }
          // ImGui ignores bounds unless min < max
          ImGui::DragFloat3("Scale", &transform.scale.x, 0.1F, 0.01F,
                            std::numeric_limits<float>::max(), "%.1f",
                            ImGuiSliderFlags_AlwaysClamp);
        });
    DrawComponent<RigidBody>(
        ecs, entity, "Rigid Body", [](RigidBody& rigid_body) -> void {
          ImGui::Checkbox("Gravity", &rigid_body.has_gravity);
          ImGui::DragFloat3("Acceleration", &rigid_body.acceleration.x, 0.1F,
                            0.0F, 0.0F, "%.1f");
          ImGui::DragFloat3("Velocity", &rigid_body.velocity.x, 0.1F, 0.0F,
                            0.0F, "%.1f");
        });
    DrawComponent<Collider>(
        ecs, entity, "Collider", [](Collider& collider) -> void {
          ImGui::DragFloat("Restitution", &collider.restitution, 0.1F, 0.0F,
                           1.0F, "%.1f", ImGuiSliderFlags_AlwaysClamp);
          ImGui::DragFloat("Friction", &collider.friction, 0.1F, 0.0F, 1.0F,
                           "%.1f", ImGuiSliderFlags_AlwaysClamp);
          // Read-only: Physics owns the body id
          const std::string body = collider.body_id == Collider::kNoBody
                                       ? "none"
                                       : std::format("{}", collider.body_id);
          ImGui::TextUnformatted(std::format("Body: {}", body).c_str());
        });
    DrawComponent<Renderable>(
        ecs, entity, "Renderable", [](Renderable& renderable) -> void {
          ImGui::ColorEdit3("Color", renderable.color.data());
          if (IsValid(renderable.texture)) {
            ImGui::DragFloat("Texture Scale", &renderable.texture_scale, 0.1F,
                             0.01F, 10.0F, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp);
          }
        });

    // After the sections, so a new component's header appears next frame
    if (ImGui::Button("Add Component")) {
      ImGui::OpenPopup("add_component");
    }
    if (ImGui::BeginPopup("add_component")) {
      AddComponentMenuItem<Transform>(ecs, entity, "Transform");
      AddComponentMenuItem<RigidBody>(ecs, entity, "Rigid Body");
      AddComponentMenuItem<Collider>(ecs, entity, "Collider");
      // A default Renderable has no mesh, which the renderer asserts on
      // TODO: Should the renderer assert on no mesh?
      AddComponentMenuItem<Renderable>(
          ecs, entity, "Renderable", [&assets]() -> Renderable {
            return {.mesh_handle = assets.LoadMesh("assets/meshes/cube.bin")};
          });
      ImGui::EndPopup();
    }

    ImGui::PopID();
  });
}

/** @brief Runs imguiCreate. @return The context it made current. */
auto CreateImguiContext() -> ImGuiContext* {
  imguiCreate();
  auto* context = ImGui::GetCurrentContext();
  assert(context != nullptr && "imguiCreate made no ImGui context");
  return context;
}

}  // namespace

UiSystem::UiSystem() : context_(CreateImguiContext()) {}

UiSystem::~UiSystem() { imguiDestroy(); }

auto UiSystem::WantsMouse() const -> bool {
  ImGui::SetCurrentContext(context_);
  return ImGui::GetIO().WantCaptureMouse;
}

void UiSystem::Update(Ecs& ecs, AssetRegistry& assets,
                      const FrameContext& ctx) {
  // Draw into this system's context, not whichever is current
  ImGui::SetCurrentContext(context_);

  // Wait for windowing set up to finish
  if (ctx.width <= 1 || ctx.height <= 1) {
    return;
  }

  const auto& mouse = *ctx.mouse;
  imguiBeginFrame(
      mouse.m_mx, mouse.m_my,
      (mouse.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0) |
          (mouse.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0) |
          (mouse.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0),
      mouse.m_mz, static_cast<std::uint16_t>(ctx.width),
      static_cast<std::uint16_t>(ctx.height));

  const auto screen_width = static_cast<float>(ctx.width);
  const auto screen_height = static_cast<float>(ctx.height);
  const auto window_width = screen_width / 5.0F;
  const auto window_height = screen_height * 0.9F;
  ImGui::SetNextWindowPos(ImVec2(10.0f, 50.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(window_width, window_height),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Settings", nullptr, 0);

  if (ImGui::Button("Spawn Entity")) {
    SpawnEntity(ecs, assets);
  }
  ImGui::Separator();
  DrawInspector(ecs, assets);
  ImGui::End();

  imguiEndFrame();
}