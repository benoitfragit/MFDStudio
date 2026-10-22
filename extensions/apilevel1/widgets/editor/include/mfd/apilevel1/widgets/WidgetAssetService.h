#pragma once
#include "EditorDocumentSerializer.h"
#include <functional>
namespace editor
{
using WidgetCapabilityProbe = std::function<bool(const std::filesystem::path&, std::string&)>;
/** @brief Probe the widget editor ABI without invoking UI callbacks. */
bool ProbeWidgetEditor(const std::filesystem::path& windowFile, std::string& error);
/** @brief Source bytes used to reject stale import plans; bounded to 64 MiB. */
using WidgetSourceSnapshot = std::map<std::filesystem::path, std::string>;
class WidgetAssetService final
{
public:
    static std::string ReadBounded(const std::filesystem::path& file, std::size_t maximum = 16U * 1024U * 1024U);
    static WidgetSourceSnapshot Snapshot(const mfd::MfdDocument& document, const std::filesystem::path& pageFile,
                                         const std::filesystem::path& assetRoot);
    /** @brief Stages an expanded definition/resources, deduplicates content and resolves name collisions. */
    static std::shared_ptr<const mfd::api1::WidgetDefinition> Stage(
        const mfd::api1::WidgetDefinition& source, const std::filesystem::path& sourceRoot,
        const std::filesystem::path& targetRoot, mfd::MfdDocument& document, EditorFileLayout& files);
    static void StageReticle(mfd::ReticleGroup& reticle, const std::filesystem::path& sourceRoot,
                             const std::filesystem::path& targetRoot, EditorFileLayout& files);
    static void Export(const mfd::api1::WidgetDefinition& source, const std::filesystem::path& sourceRoot,
                       const std::filesystem::path& folder, const EditorFileLayout& sourceFiles);
};
}
