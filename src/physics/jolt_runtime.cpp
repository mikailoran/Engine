#include "physics/jolt_runtime.h"

#include <Jolt/Core/Core.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <iostream>
#include <memory>

namespace {

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
  // The allocator must be in place before Jolt allocates anything
  JPH::RegisterDefaultAllocator();
  JPH::Trace = TraceToStderr;
  JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = ReportAssert;)

  factory_ = std::make_unique<JPH::Factory>();
  JPH::Factory::sInstance = factory_.get();
  JPH::RegisterTypes();
}

JoltRuntime::~JoltRuntime() {
  JPH::UnregisterTypes();
  JPH::Factory::sInstance = nullptr;
}
