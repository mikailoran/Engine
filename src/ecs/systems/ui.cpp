#include "ui.h"

#include "imgui/imgui.h"

#include "../core/ecs.h"
#include "../core/frame_context.h"

#include "../components/configurable.h"
#include "../components/spin.h"
#include "../components/transform.h"

#include "entry/entry.h"

void UiSystem::Init() { imguiCreate(); }

void UiSystem::Shutdown() { imguiDestroy(); }

void UiSystem::Update(Ecs &ecs, const FrameContext &ctx) {
  const auto &mouse = *ctx.mouse;
  imguiBeginFrame(
      mouse.m_mx, mouse.m_my,
      (mouse.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0) |
          (mouse.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0) |
          (mouse.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0),
      mouse.m_mz, static_cast<std::uint16_t>(ctx.width),
      static_cast<std::uint16_t>(ctx.height));

  ImGui::SetNextWindowPos(ImVec2(ctx.width - (ctx.width / 5.0f) - 10.0f, 10.0f),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(ctx.width / 5.0f, ctx.height / 3.5f),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Settings", nullptr, 0);

  // Sliders of transforms of entities
  for (const auto &entity : entities) {
    auto &transform = ecs.GetComponent<Transform>(entity);
    // TODO: bug: ui system doesn't have the Spin signature
    auto &spin = ecs.GetComponent<Spin>(entity);
    // auto &configurable = ecs.GetComponent<Configurable>(entity);
    ImGui::PushID(static_cast<int>(entity));

    ImGui::Text("Entity %zu", entity);
    ImGui::SliderFloat("x", &transform.position.x, -10.0F, 10.0F);
    ImGui::SliderFloat("y", &transform.position.y, -10.0F, 10.0F);
    ImGui::SliderFloat("Scale", &transform.scale.x, 0.1F, 10.0F);
    ImGui::Checkbox("Spin", &spin.should_spin);
    ImGui::NewLine();

    ImGui::PopID();
    transform.scale.z = transform.scale.y = transform.scale.x;
  }

  ImGui::End();

  imguiEndFrame();
}