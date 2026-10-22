/* This file is part of MFDStudio. Project author: Benoit Fra. */
#pragma once
#include <array>
#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1::detail
{
/** @brief Per-query index; never survives a layout mutation or pointer event. */
class ContainerPointerQuery final
{
public:
    ContainerPointerQuery(const std::vector<ContainerDefinition>& definitions,
                          const std::vector<ContainerLayoutNode>& layouts, Vec2 pointer) noexcept;
    [[nodiscard]] bool CanReceive(const ContainerLayoutNode& layout) const noexcept;
private:
    const std::vector<ContainerDefinition>& definitions_;
    Vec2 pointer_;
    std::array<const ContainerLayoutNode*, kMaximumContainerCount> bySource_ {};
    bool valid_ = false;
};
}
