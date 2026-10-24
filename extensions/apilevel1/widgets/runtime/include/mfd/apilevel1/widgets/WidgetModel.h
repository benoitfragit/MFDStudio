#pragma once
/** @file @brief Widget validation and mixed hierarchy projection onto the shared layout kernel. */
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/MfdExport.h"
namespace mfd::api1
{
[[nodiscard]] MFD_API bool ValidateWidgetDefinition(const WidgetDefinition& definition, std::string& error);
[[nodiscard]] MFD_API bool ValidatePageWidgets(const std::vector<ContainerDefinition>& containers,
                                              const std::vector<WidgetInstance>& widgets, std::string& error);
[[nodiscard]] MFD_API WidgetState InitialWidgetState(const WidgetInstance& instance);
[[nodiscard]] MFD_API WidgetVisualState ResolveWidgetVisualState(const WidgetState& state) noexcept;
[[nodiscard]] MFD_API bool IsInteractiveWidget(WidgetType type) noexcept;
/**
 * @brief Projects containers followed by widget leaves; source indices retain that order.
 * @param containers Validated authored container catalog.
 * @param widgets Widget instances with non-null validated definitions.
 * @return Projected layout nodes, or an empty vector when a count limit or definition precondition fails.
 * @note Prefer @ref EvaluateWidgetLayout at external input boundaries because it returns a diagnostic.
 */
[[nodiscard]] MFD_API std::vector<ContainerDefinition> ProjectWidgetLayoutNodes(
    const std::vector<ContainerDefinition>& containers, const std::vector<WidgetInstance>& widgets);
[[nodiscard]] MFD_API bool EvaluateWidgetLayout(const std::vector<ContainerDefinition>& containers,
    const std::vector<WidgetInstance>& widgets, std::vector<ContainerLayoutNode>& output, std::string& error);
/** @brief Resolves mixed children while returning only original container nodes for existing gizmos. */
[[nodiscard]] MFD_API bool EvaluateContainersWithWidgets(const std::vector<ContainerDefinition>& containers,
    const std::vector<WidgetInstance>& widgets, std::vector<ContainerLayoutNode>& output, std::string& error);
} // namespace mfd::api1
