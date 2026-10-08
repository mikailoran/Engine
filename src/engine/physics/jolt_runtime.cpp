#include "engine/physics/jolt_runtime.h"

// The header no longer brings it, and every other Jolt header needs it first
#include <Jolt/Jolt.h>  // IWYU pragma: keep
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace engine::physics {

/** @brief The Jolt objects JoltRuntime owns. */
struct JoltRuntime::State {
  /// Published through JPH::Factory::sInstance while this lives.
  JPH::Factory factory;
};

namespace {

/** @brief Whether a JoltRuntime is alive; Jolt's state allows only one. */
auto RuntimeAlive() -> bool& {
  static bool alive = false;
  return alive;
}

// Jolt's Trace hook is printf-style, so the varargs are unavoidable here
// NOLINTBEGIN(modernize-avoid-variadic-functions,cppcoreguidelines-pro-type-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
/** @brief Prints one of Jolt's printf-style trace messages to stderr. */
void TraceToStderr(const char* format, ...) {
  std::array<char, 1024> buffer{};
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);
  std::cerr << "jolt: " << buffer.data() << '\n';
}
// NOLINTEND(modernize-avoid-variadic-functions,cppcoreguidelines-pro-type-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay)

#ifdef JPH_ENABLE_ASSERTS
/** @brief Reports a failed Jolt assert. @return true, to break execution. */
auto ReportAssert(const char* expression, const char* message, const char* file,
                  JPH::uint line) -> bool {
  std::cerr << std::format("jolt: {}:{}: assert ({}) {}\n", file, line,
                           expression, message != nullptr ? message : "");
  return true;
}
#endif

}  // namespace

JoltRuntime::JoltRuntime() {
  assert(!RuntimeAlive() && "only one JoltRuntime may exist");
  // A second one would swap the factory out from under the first
  if (RuntimeAlive()) {
    throw std::logic_error("only one JoltRuntime may exist");
  }

  // The allocator must be in place before Jolt allocates anything
  JPH::RegisterDefaultAllocator();
  JPH::Trace = TraceToStderr;
  JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = ReportAssert;)

  state_ = std::make_unique<State>();
  JPH::Factory::sInstance = &state_->factory;
  JPH::RegisterTypes();
  RuntimeAlive() = true;
}

JoltRuntime::~JoltRuntime() {
  JPH::UnregisterTypes();
  JPH::Factory::sInstance = nullptr;
  RuntimeAlive() = false;
}

}  // namespace engine::physics
