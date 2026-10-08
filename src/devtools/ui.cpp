#include "devtools/ui.h"

#include <bx/math.h>
#include <dear-imgui/imgui.h>
#include <imgui/imgui.h>

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <string>

#include "devtools/selected.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/math/rotation.h"
#include "engine/physics/shape.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"
#include "engine/resource/asset_registry.h"
#include "engine/resource/mesh_handle.h"
#include "engine/resource/texture_handle.h"

using engine::AssetRegistry;
using engine::BoxColliderAround;
using engine::Collider;
using engine::Ecs;
using engine::Entity;
using engine::EulerToQuat;
using engine::FrameContext;
using engine::Input;
using engine::Key;
using engine::MeshHandle;
using engine::MouseButton;
using engine::PhysicsLink;
using engine::QuatToEuler;
using engine::Renderable;
using engine::RigidBody;
using engine::ScreenPosition;
using engine::Transform;
using engine::physics::ShapeKind;

namespace devtools {

namespace {

/** @brief Spawns a bunny. @return The new entity. */
auto SpawnEntity(Ecs& ecs, AssetRegistry& assets) -> Entity {
  // TODO: remove paths
  const MeshHandle mesh_handle = assets.LoadMesh("assets/meshes/bunny.bin");
  const auto entity = ecs.CreateEntity();
  ecs.AddComponent(entity, Transform{.position = {0.0F, 3.0F, 0.0F}});
  ecs.AddComponent(entity, RigidBody{});
  ecs.AddComponent(entity,
                   BoxColliderAround(assets.GetMeshBounds(mesh_handle)));
  ecs.AddComponent(entity, Renderable{.mesh_handle = mesh_handle});

  return entity;
}

/** @brief Draws a Collider's shape picker and the size fields it uses. */
auto DrawShapeKind(Collider& collider) -> void {
  constexpr std::array<const char*, 2> kShapeNames{"Box", "Sphere"};
  int shape = static_cast<int>(collider.shape.kind);
  if (ImGui::Combo("Shape", &shape, kShapeNames.data(),
                   static_cast<int>(kShapeNames.size()))) {
    collider.shape.kind = static_cast<ShapeKind>(shape);
  }
  // A zero size is a degenerate shape Jolt rejects
  if (collider.shape.kind == ShapeKind::kBox) {
    ImGui::DragFloat3("Half Extents", &collider.shape.half_extents.x, 0.05F,
                      0.01F, std::numeric_limits<float>::max(), "%.2f",
                      ImGuiSliderFlags_AlwaysClamp);
  } else {
    ImGui::DragFloat("Radius", &collider.shape.radius, 0.05F, 0.01F,
                     std::numeric_limits<float>::max(), "%.2f",
                     ImGuiSliderFlags_AlwaysClamp);
  }
  ImGui::DragFloat3("Offset", &collider.shape.offset.x, 0.05F, 0.0F, 0.0F,
                    "%.2f");
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
        ecs, entity, "Collider", [&ecs, entity](Collider& collider) -> void {
          DrawShapeKind(collider);
          ImGui::DragFloat("Restitution", &collider.material.restitution, 0.1F,
                           0.0F, 1.0F, "%.1f", ImGuiSliderFlags_AlwaysClamp);
          ImGui::DragFloat("Friction", &collider.material.friction, 0.1F, 0.0F,
                           1.0F, "%.1f", ImGuiSliderFlags_AlwaysClamp);
          // Read-only: PhysicsSystem owns the link to the body
          const auto* link = ecs.TryGetComponent<PhysicsLink>(entity);
          const std::string body =
              link == nullptr ? "none"
                              : std::format("{}", link->Body().Value());
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

/** @brief An engine key and the ImGui key it drives. */
struct ImguiKeyBinding {
  Key key;
  ImGuiKey imgui_key;
};

// Text editing, navigation and shortcut keys; W/S/D/Q/E only fly the camera
constexpr auto kImguiKeys = std::to_array<ImguiKeyBinding>({
    // clang-format off
    {.key = Key::kTab,        .imgui_key = ImGuiKey_Tab},
    {.key = Key::kLeft,       .imgui_key = ImGuiKey_LeftArrow},
    {.key = Key::kRight,      .imgui_key = ImGuiKey_RightArrow},
    {.key = Key::kUp,         .imgui_key = ImGuiKey_UpArrow},
    {.key = Key::kDown,       .imgui_key = ImGuiKey_DownArrow},
    {.key = Key::kPageUp,     .imgui_key = ImGuiKey_PageUp},
    {.key = Key::kPageDown,   .imgui_key = ImGuiKey_PageDown},
    {.key = Key::kHome,       .imgui_key = ImGuiKey_Home},
    {.key = Key::kEnd,        .imgui_key = ImGuiKey_End},
    {.key = Key::kDelete,     .imgui_key = ImGuiKey_Delete},
    {.key = Key::kBackspace,  .imgui_key = ImGuiKey_Backspace},
    {.key = Key::kEnter,      .imgui_key = ImGuiKey_Enter},
    {.key = Key::kEscape,     .imgui_key = ImGuiKey_Escape},
    {.key = Key::kLeftCtrl,   .imgui_key = ImGuiKey_LeftCtrl},
    {.key = Key::kRightCtrl,  .imgui_key = ImGuiKey_RightCtrl},
    {.key = Key::kLeftShift,  .imgui_key = ImGuiKey_LeftShift},
    {.key = Key::kRightShift, .imgui_key = ImGuiKey_RightShift},
    {.key = Key::kLeftAlt,    .imgui_key = ImGuiKey_LeftAlt},
    {.key = Key::kRightAlt,   .imgui_key = ImGuiKey_RightAlt},
    {.key = Key::kA,          .imgui_key = ImGuiKey_A},
    {.key = Key::kC,          .imgui_key = ImGuiKey_C},
    {.key = Key::kV,          .imgui_key = ImGuiKey_V},
    {.key = Key::kX,          .imgui_key = ImGuiKey_X},
    {.key = Key::kY,          .imgui_key = ImGuiKey_Y},
    {.key = Key::kZ,          .imgui_key = ImGuiKey_Z},
    // clang-format on
});

/**
 * @brief Queues this frame's keys, modifiers and typed text into the current
 * ImGui context. Must precede its NewFrame.
 */
void ForwardKeyboard(const Input& input) {
  ImGuiIO& io = ImGui::GetIO();

  // Held state every frame; ImGui drops repeats and makes its own key repeat
  for (const ImguiKeyBinding& binding : kImguiKeys) {
    io.AddKeyEvent(binding.imgui_key, input.Down(binding.key));
  }
  io.AddKeyEvent(ImGuiMod_Ctrl,
                 input.Down(Key::kLeftCtrl) || input.Down(Key::kRightCtrl));
  io.AddKeyEvent(ImGuiMod_Shift,
                 input.Down(Key::kLeftShift) || input.Down(Key::kRightShift));
  io.AddKeyEvent(ImGuiMod_Alt,
                 input.Down(Key::kLeftAlt) || input.Down(Key::kRightAlt));

  if (!input.Text().empty()) {
    io.AddInputCharactersUTF8(input.Text().c_str());
  }
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

auto UiSystem::WantsKeyboard() const -> bool {
  ImGui::SetCurrentContext(context_);
  return ImGui::GetIO().WantCaptureKeyboard;
}

auto UiSystem::WantsText() const -> bool {
  ImGui::SetCurrentContext(context_);
  return ImGui::GetIO().WantTextInput;
}

auto UiSystem::DebugDrawEnabled() const -> bool { return debug_draw_enabled_; }

void UiSystem::Update(Ecs& ecs, AssetRegistry& assets,
                      const FrameContext& ctx) {
  // Draw into this system's context, not whichever is current
  ImGui::SetCurrentContext(context_);

  const Input& input = ctx.input;
  // imguiBeginFrame wants a running total, not this frame's notches
  wheel_total_ += input.Wheel();

  // Wait for windowing set up to finish
  if (ctx.width <= 1 || ctx.height <= 1) {
    return;
  }

  ForwardKeyboard(input);
  const ScreenPosition mouse = input.Mouse();
  imguiBeginFrame(
      static_cast<std::int32_t>(mouse.x), static_cast<std::int32_t>(mouse.y),
      (input.Down(MouseButton::kLeft) ? IMGUI_MBUT_LEFT : 0) |
          (input.Down(MouseButton::kRight) ? IMGUI_MBUT_RIGHT : 0) |
          (input.Down(MouseButton::kMiddle) ? IMGUI_MBUT_MIDDLE : 0),
      static_cast<std::int32_t>(wheel_total_),
      static_cast<std::uint16_t>(ctx.width),
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
  ImGui::Checkbox("Debug Draw", &debug_draw_enabled_);
  ImGui::Separator();
  DrawInspector(ecs, assets);
  ImGui::End();

  imguiEndFrame();
}

}  // namespace devtools
