#include <pans/utils/system_utils.h>

namespace pans {

std::chrono::steady_clock::duration GetElapsedTime() noexcept
{
    static const auto START_TIME = std::chrono::steady_clock::now();
    return std::chrono::steady_clock::now() - START_TIME;
}

u64 GetFiberId() noexcept
{
    return 0;
}

}