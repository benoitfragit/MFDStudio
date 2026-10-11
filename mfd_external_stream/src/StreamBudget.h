#pragma once
#include <cstdint>
#include <stdexcept>

namespace mfd::stream::detail
{
constexpr std::uint64_t kStreamMemoryBudget = 128ULL * 1024 * 1024;
/** @brief Recoverable reservation refusal; never a device or protocol failure. */
class BudgetUnavailable final : public std::runtime_error
{
public:
    BudgetUnavailable();
};
/** @brief Accounts for texture payloads before allocation, including staging and GL copies.
 * @note Declare before the owned resources so destruction releases the reservation last.
 */
class MemoryReservation
{
public:
    explicit MemoryReservation(std::uint64_t bytes);
    ~MemoryReservation();
    MemoryReservation(const MemoryReservation&) = delete;
    MemoryReservation& operator=(const MemoryReservation&) = delete;
private:
    std::uint64_t bytes_;
};
/** @brief Shared accounting observation for internal diagnostics and regression tests. */
std::uint64_t ReservedStreamBytes() noexcept;

/** @brief Limits allocation retries even if the producer changes its request serial.
 * @note All timestamps are monotonic milliseconds; no waiting or allocation occurs here.
 */
class ConfigurationRetry
{
public:
    bool Ready(std::uint64_t now) const noexcept;
    void Defer(std::uint64_t now) noexcept;
    void Reset() noexcept;
    bool Deferred() const noexcept;
private:
    std::uint64_t lastAttempt_ = 0;
    bool deferred_ = false;
};
} // namespace mfd::stream::detail
