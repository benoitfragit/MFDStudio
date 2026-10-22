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
    static bool ReplaceDefinition(MfdDocument& document, std::shared_ptr<const WidgetDefinition> replacement,
                                  std::string& error);
    static std::shared_ptr<const WidgetDefinition> CreateDefinition(std::filesystem::path file,
                                                                   std::string id, WidgetType type);
};
}
