#pragma once
/** @file @brief Transactional widget authoring operations, independent of ImGui. */
#include "mfd/model/PageDefinition.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
namespace mfd::api1
{
class WidgetEditorModel final
{
public:
    static WidgetInstance* Find(PageDefinition& page, std::string_view id) noexcept;
    static const WidgetInstance* Find(const PageDefinition& page, std::string_view id) noexcept;
    static std::string UniqueId(const PageDefinition& page, std::string_view base);
    static std::string Insert(PageDefinition& page, std::shared_ptr<const WidgetDefinition> definition,
                              std::string reference, std::string_view parent, std::string& error);
    static std::string Duplicate(PageDefinition& page, std::string_view id, std::string& error);
    static bool Remove(PageDefinition& page, std::string_view id, std::string& error);
    static bool Replace(PageDefinition& page, std::string_view previousId, WidgetInstance replacement,
                        std::string_view parent, const ContainerChildPlacement* placement, std::string& error);
    /** @brief Applies a page-space drag to the initial instance, respecting parent layout constraints.
     * @param page Page changed only after validation succeeds.
     * @param initial Instance captured when the gesture starts.
     * @param delta Total pointer displacement in page coordinates.
     * @param resize True for a bottom-right corner resize around the instance center.
     * @param error Reason when the gesture is invalid or controlled by parent layout.
     * @return True when an authored geometry change was committed.
     */
    static bool ApplyGesture(PageDefinition& page, const WidgetInstance& initial, Vec2 delta,
                             bool resize, std::string& error);
    static bool ReplaceDefinition(MfdDocument& document, std::shared_ptr<const WidgetDefinition> replacement,
                                  std::string& error);
    static std::shared_ptr<const WidgetDefinition> CreateDefinition(std::filesystem::path file,
                                                                   std::string id, WidgetType type);
};
}
