#include "game/window.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "engine/platform/input.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

using engine::Input;
using engine::Key;
using engine::MouseButton;
using engine::NativeSurface;
using engine::PixelSize;
using engine::SurfaceKind;

namespace game {

namespace {

/** @brief An engine key and the physical key that drives it. */
struct KeyBinding {
  Key key;
  SDL_Scancode scancode;
};

// Scancodes are physical positions, so WASD stays put on any layout
constexpr auto kKeyBindings = std::to_array<KeyBinding>({
    // clang-format off
    {.key = Key::kW,          .scancode = SDL_SCANCODE_W},
    {.key = Key::kA,          .scancode = SDL_SCANCODE_A},
    {.key = Key::kS,          .scancode = SDL_SCANCODE_S},
    {.key = Key::kD,          .scancode = SDL_SCANCODE_D},
    {.key = Key::kQ,          .scancode = SDL_SCANCODE_Q},
    {.key = Key::kE,          .scancode = SDL_SCANCODE_E},
    {.key = Key::kC,          .scancode = SDL_SCANCODE_C},
    {.key = Key::kV,          .scancode = SDL_SCANCODE_V},
    {.key = Key::kX,          .scancode = SDL_SCANCODE_X},
    {.key = Key::kY,          .scancode = SDL_SCANCODE_Y},
    {.key = Key::kZ,          .scancode = SDL_SCANCODE_Z},
    {.key = Key::kTab,        .scancode = SDL_SCANCODE_TAB},
    {.key = Key::kLeft,       .scancode = SDL_SCANCODE_LEFT},
    {.key = Key::kRight,      .scancode = SDL_SCANCODE_RIGHT},
    {.key = Key::kUp,         .scancode = SDL_SCANCODE_UP},
    {.key = Key::kDown,       .scancode = SDL_SCANCODE_DOWN},
    {.key = Key::kPageUp,     .scancode = SDL_SCANCODE_PAGEUP},
    {.key = Key::kPageDown,   .scancode = SDL_SCANCODE_PAGEDOWN},
    {.key = Key::kHome,       .scancode = SDL_SCANCODE_HOME},
    {.key = Key::kEnd,        .scancode = SDL_SCANCODE_END},
    {.key = Key::kDelete,     .scancode = SDL_SCANCODE_DELETE},
    {.key = Key::kBackspace,  .scancode = SDL_SCANCODE_BACKSPACE},
    {.key = Key::kEnter,      .scancode = SDL_SCANCODE_RETURN},
    {.key = Key::kEscape,     .scancode = SDL_SCANCODE_ESCAPE},
    {.key = Key::kLeftCtrl,   .scancode = SDL_SCANCODE_LCTRL},
    {.key = Key::kRightCtrl,  .scancode = SDL_SCANCODE_RCTRL},
    {.key = Key::kLeftShift,  .scancode = SDL_SCANCODE_LSHIFT},
    {.key = Key::kRightShift, .scancode = SDL_SCANCODE_RSHIFT},
    {.key = Key::kLeftAlt,    .scancode = SDL_SCANCODE_LALT},
    {.key = Key::kRightAlt,   .scancode = SDL_SCANCODE_RALT},
    // clang-format on
});

/** @brief Whether every Key appears in exactly one of kKeyBindings. */
constexpr auto BindsEveryKeyOnce() -> bool {
  std::array<int, static_cast<std::size_t>(Key::kCount)> uses{};
  for (const KeyBinding& binding : kKeyBindings) {
    ++uses.at(static_cast<std::size_t>(binding.key));
  }
  return std::ranges::all_of(uses,
                             [](int count) -> bool { return count == 1; });
}

static_assert(BindsEveryKeyOnce(), "every Key needs exactly one binding");

static_assert(std::ranges::all_of(kKeyBindings,
                                  [](const KeyBinding& binding) -> bool {
                                    return binding.scancode <
                                           SDL_SCANCODE_COUNT;
                                  }),
              "key binding outside SDL's keyboard state");

/** @brief The name of SDL's video driver, or empty if none is running. */
auto VideoDriver() -> std::string_view {
  const char* driver = SDL_GetCurrentVideoDriver();
  return driver != nullptr ? driver : "";
}

}  // namespace

Window::Window(PixelSize size) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(
        std::format("SDL_Init failed: {}", SDL_GetError()));
  }

  window_ = SDL_CreateWindow(
      "game", static_cast<int>(size.width), static_cast<int>(size.height),
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window_ == nullptr) {
    const std::string error = SDL_GetError();
    // The destructor won't run for a constructor that throws
    SDL_Quit();
    throw std::runtime_error(std::format("SDL_CreateWindow failed: {}", error));
  }

  std::cout << "SDL video driver: " << VideoDriver() << '\n';
}

void Window::SetTextInput(bool enabled) {
  if (enabled == text_input_) {
    return;
  }
  text_input_ = enabled;
  if (enabled) {
    SDL_StartTextInput(window_);
  } else {
    SDL_StopTextInput(window_);
  }
}

Window::~Window() {
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

auto Window::Surface() const -> NativeSurface {
  const SDL_PropertiesID props = SDL_GetWindowProperties(window_);
  NativeSurface surface{.size = BackbufferSize()};

  const std::string_view driver = VideoDriver();
  // TODO: String check for driver seems susceptible to break
  if (driver == "wayland") {
    surface.kind = SurfaceKind::kWayland;
    surface.window = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    surface.display = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
  } else if (driver == "x11") {
    surface.kind = SurfaceKind::kX11;
    // An X11 window is an integer id; bgfx takes it in a pointer
    surface.window = std::bit_cast<void*>(static_cast<std::uintptr_t>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
    surface.display = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
  }

  if (surface.window == nullptr || surface.display == nullptr) {
    throw std::runtime_error(std::format(
        "no native window handles for SDL video driver '{}'", driver));
  }
  return surface;
}

auto Window::BackbufferSize() const -> PixelSize {
  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(window_, &width, &height);
  return {.width = static_cast<std::uint32_t>(width),
          .height = static_cast<std::uint32_t>(height)};
}

auto Window::PumpEvents(Input& input) -> bool {
  input.BeginFrame();

  bool quit = false;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        quit = true;
        break;
      case SDL_EVENT_TEXT_INPUT:
        input.AddText(event.text.text);
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        // Natural scrolling flips the sign; undo it so positive is away
        input.AddWheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                           ? -event.wheel.y
                           : event.wheel.y);
        break;
      default:
        break;
    }
  }

  // Held state, read once the queue is drained; SDL reports points
  float x = 0.0F;
  float y = 0.0F;
  const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
  const float density = SDL_GetWindowPixelDensity(window_);
  input.SetMouse({.x = x * density, .y = y * density});
  input.SetButton(MouseButton::kLeft, (buttons & SDL_BUTTON_LMASK) != 0);
  input.SetButton(MouseButton::kRight, (buttons & SDL_BUTTON_RMASK) != 0);
  input.SetButton(MouseButton::kMiddle, (buttons & SDL_BUTTON_MMASK) != 0);

  int key_count = 0;
  const bool* key_state = SDL_GetKeyboardState(&key_count);
  const std::span<const bool> keys(key_state,
                                   static_cast<std::size_t>(key_count));
  for (const KeyBinding& binding : kKeyBindings) {
    // In bounds: see the static_assert on kKeyBindings
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    input.SetKey(binding.key, keys[static_cast<std::size_t>(binding.scancode)]);
  }

  return quit;
}

}  // namespace game
