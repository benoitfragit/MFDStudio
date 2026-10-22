#pragma once
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include <functional>
#include <nlohmann/json_fwd.hpp>
namespace mfd::api1
{
using WidgetVisualParser = std::function<ReticleGroup(const nlohmann::json&, const std::filesystem::path&)>;
using WidgetVisualSerializer = std::function<nlohmann::json(const ReticleGroup&, const std::filesystem::path&)>;
/** @brief Canonicalize a dependency inside the authored asset root; reject escapes, including symlinks. */
MFD_API std::filesystem::path ResolveWidgetAsset(const std::filesystem::path& baseFolder,
    const std::filesystem::path& assetRoot, const std::string& reference);
MFD_API std::shared_ptr<const WidgetDefinition> LoadWidgetDefinition(const std::filesystem::path& file,
    const std::filesystem::path& assetRoot, const WidgetVisualParser& parseVisual);
MFD_API std::vector<WidgetInstance> ParsePageWidgets(const nlohmann::json& page,
    const std::filesystem::path& baseFolder, const std::filesystem::path& assetRoot, const WidgetVisualParser& parseVisual);
MFD_API nlohmann::json SerializePageWidgets(const std::vector<WidgetInstance>& widgets);
MFD_API nlohmann::json SerializeWidgetDefinition(const WidgetDefinition& definition,
    const WidgetVisualSerializer& serializeVisual);
}
