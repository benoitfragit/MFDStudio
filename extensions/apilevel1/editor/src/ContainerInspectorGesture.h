/* This file is part of MFDStudio. Project author: Benoit Fra. */
#pragma once
#include <cstdint>

namespace mfd::api1::detail
{
/** @brief Host-only gesture identity; holds no document pointers or snapshots. */
class ContainerInspectorGesture final
{
public:
    /** @brief Advances UI lifecycle state, including activation frames without changes. */
    std::uint64_t Update(int frame, unsigned int activeKey, unsigned int changedKey, bool activated) noexcept;
private:
    int previousFrame_ = -2;
    unsigned int activeKey_ = 0U;
    std::uint64_t serial_ = 0U;
};
}
