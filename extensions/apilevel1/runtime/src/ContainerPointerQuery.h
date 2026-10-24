/* This file is part of MFDStudio. Project author: Benoit Fra. */
#pragma once
#include <array>
#include "mfd/apilevel1/ContainerTypes.h"
#include "mfd/apilevel1/widgets/WidgetTypes.h"

namespace mfd::api1::detail
{
/** @brief Per-query index; never survives a layout mutation or pointer event. */
class ContainerPointerQuery final
{
public:
    ContainerPointerQuery(const std::vector<ContainerDefinition>& definitions,
                          const std::vector<ContainerLayoutNode>& layouts, Vec2 pointer) noexcept;
    [[nodiscard]] bool VisibleAt(const ContainerLayoutNode& layout, bool testOwnBounds = true) const noexcept;
    [[nodiscard]] bool CanReceive(const ContainerLayoutNode& layout) const noexcept;
    /** @brief Returns the indexed layout node, or null when the query or source index is invalid. */
    [[nodiscard]] const ContainerLayoutNode* LayoutAtSource(std::size_t sourceIndex) const noexcept;
private:
    const std::vector<ContainerDefinition>& definitions_;
    Vec2 pointer_;
    std::array<const ContainerLayoutNode*, kMaximumContainerCount + kMaximumWidgetCount> bySource_ {};
    bool valid_ = false;
};
}
