#ifndef _PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H_
#define _PANS_INCLUDE_PANS_UTILS_SYSTEM_UTILS_H_

#include <chrono>
#include <string>
#include <pans/types.h>
#include <pans/export.h>

namespace pans {

[[nodiscard]] PANS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept;
[[nodiscard]] PANS_API u64 GetFiberId() noexcept;
[[nodiscard]] PANS_API std::string GetBacktrace(int size = 64, int skip = 1, const std::string& prefix = "");

}

#endif