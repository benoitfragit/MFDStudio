/* This file is part of MFDStudio. Project author: Benoit Fra. */
#include "ContainerInspectorGesture.h"

namespace mfd::api1::detail
{
std::uint64_t ContainerInspectorGesture::Update(const int frame, const unsigned int activeKey,
                                               const unsigned int changedKey, const bool activated) noexcept
{
    const unsigned int eventKey = changedKey != 0U ? changedKey : activeKey;
    if (eventKey != 0U && (eventKey != activeKey_ || activated || previousFrame_ != frame - 1))
    {
        ++serial_;
        if (serial_ == 0U) ++serial_;
    }
    activeKey_ = activeKey;
    previousFrame_ = frame;
    return serial_;
}
}
