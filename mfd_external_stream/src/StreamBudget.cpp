#include "StreamBudget.h"
#include <atomic>

namespace mfd::stream::detail
{
namespace
{
std::atomic<std::uint64_t> reservedBytes {0};
bool Reserve(const std::uint64_t bytes) noexcept
{
    if (bytes > kStreamMemoryBudget) return false;
    auto total = reservedBytes.load();
    // Contention is recoverable too: never spin indefinitely on a render thread.
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        if (total > kStreamMemoryBudget - bytes) return false;
        if (reservedBytes.compare_exchange_weak(total, total + bytes)) return true;
    }
    return false;
}
}
BudgetUnavailable::BudgetUnavailable()
    : std::runtime_error("Temporary external stream resource budget shortage (128 MiB); retry deferred") {}
MemoryReservation::MemoryReservation(const std::uint64_t bytes) : bytes_(bytes)
{
    if (!Reserve(bytes)) throw BudgetUnavailable();
}
MemoryReservation::~MemoryReservation() { reservedBytes.fetch_sub(bytes_); }
std::uint64_t ReservedStreamBytes() noexcept { return reservedBytes.load(); }
bool ConfigurationRetry::Ready(const std::uint64_t now) const noexcept
{ return !deferred_ || now - lastAttempt_ >= 250; }
void ConfigurationRetry::Defer(const std::uint64_t now) noexcept
{ lastAttempt_ = now; deferred_ = true; }
void ConfigurationRetry::Reset() noexcept { deferred_ = false; }
bool ConfigurationRetry::Deferred() const noexcept { return deferred_; }
} // namespace mfd::stream::detail
