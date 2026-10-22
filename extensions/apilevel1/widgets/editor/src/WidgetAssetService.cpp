#include "mfd/apilevel1/widgets/WidgetAssetService.h"
#include "mfd/apilevel1/widgets/WidgetJson.h"
#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
#include "internal/EditorFileCommit.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace editor
{
namespace
{
constexpr std::size_t kResourceBudget = 64U * 1024U * 1024U;
constexpr std::size_t kResourceCount = 512U;
nlohmann::json SerializeVisual(const mfd::ReticleGroup& reticle, const std::filesystem::path& base)
{ return nlohmann::json::parse(SerializeReticleTemplateToJsonString(reticle, base)); }
std::string Signature(const mfd::api1::WidgetDefinition& definition, const std::filesystem::path& root)
{
    auto copy = definition;
    copy.sourceFile = root / "widgets" / "signature.json";
    auto json = mfd::api1::SerializeWidgetDefinition(copy, SerializeVisual);
    json.erase("id");
    return json.dump();
}
std::string SafeStem(const std::string& value)
{
    std::string result;
    for (const auto ch : value)
        if (result.size() < 100) result += ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                          (ch >= '0' && ch <= '9') || ch == '_') ? ch : '_';
    return result.empty() ? "Widget" : result;
}
std::filesystem::path Confined(const std::filesystem::path& file, const std::filesystem::path& root)
{
    return mfd::api1::ResolveWidgetAsset(root, root, file.lexically_relative(root).generic_string());
}
void AddSource(WidgetSourceSnapshot& snapshot, const std::filesystem::path& file, const std::filesystem::path& root)
{
    const auto path = Confined(file, root);
    if (snapshot.count(path)) return;
    if (snapshot.size() >= kResourceCount) throw std::runtime_error("Widget dependency count exceeds 512 files.");
    auto bytes = WidgetAssetService::ReadBounded(path);
    std::size_t total = bytes.size();
    for (const auto& entry : snapshot) total += entry.second.size();
    if (total > kResourceBudget) throw std::runtime_error("Widget dependency budget exceeds 64 MiB.");
    snapshot.emplace(path, std::move(bytes));
}
void AddReticleSources(WidgetSourceSnapshot& snapshot, const mfd::ReticleGroup& reticle, const std::filesystem::path& root)
{
    for (const auto& primitive : reticle.primitives)
        if (const auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry)) AddSource(snapshot, image->file, root);
}
std::filesystem::path StageResource(const std::filesystem::path& source, const std::filesystem::path& sourceRoot,
                                    const std::filesystem::path& targetRoot, EditorFileLayout& files,
                                    const EditorFileLayout* sourceFiles)
{
    std::string bytes;
    if (sourceFiles)
    {
        const auto staged = sourceFiles->stagedResources.find(source);
        if (staged != sourceFiles->stagedResources.end()) bytes = staged->second;
    }
    if (bytes.empty()) bytes = WidgetAssetService::ReadBounded(Confined(source, sourceRoot));
    const std::string name = SafeStem(source.stem().string());
    for (std::size_t index = 0; index <= kResourceCount; ++index)
    {
        const auto file = Confined(targetRoot / "widgets" / "resources" /
            (name + (index ? "_" + std::to_string(index) : "") + source.extension().string()), targetRoot);
        const auto staged = files.stagedResources.find(file);
        if (staged != files.stagedResources.end()) { if (staged->second == bytes) return file; continue; }
        if (std::filesystem::exists(file)) { if (WidgetAssetService::ReadBounded(file) == bytes) return file; continue; }
        std::size_t total = bytes.size();
        for (const auto& entry : files.stagedResources) total += entry.second.size();
        if (files.stagedResources.size() >= kResourceCount || total > kResourceBudget)
            throw std::runtime_error("Staged widget resources exceed 512 files / 64 MiB.");
        files.stagedResources.emplace(file, std::move(bytes));
        return file;
    }
    throw std::runtime_error("Unable to choose a collision-free widget resource path.");
}
std::shared_ptr<mfd::api1::WidgetDefinition> StageResources(const mfd::api1::WidgetDefinition& source,
    const std::filesystem::path& sourceRoot, const std::filesystem::path& targetRoot,
    EditorFileLayout& files, const EditorFileLayout* sourceFiles)
{
    auto definition = std::make_shared<mfd::api1::WidgetDefinition>(source);
    for (auto& visual : definition->visuals)
    {
        visual.composition.sourceTemplateId.clear();
        for (auto& primitive : visual.composition.primitives)
            if (auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
                image->file = StageResource(image->file, sourceRoot, targetRoot, files, sourceFiles);
    }
    return definition;
}
#if defined(_WIN32)
class ScopedWidgetEditorModule final
{
public:
    explicit ScopedWidgetEditorModule(const std::filesystem::path& file) : module_(LoadLibraryW(file.c_str())) {}
    ~ScopedWidgetEditorModule() { if (module_) FreeLibrary(module_); }
    bool Compatible() const
    {
        if (!module_) return false;
        // Exact versioned query signature is required by GetProcAddress's untyped result.
        const auto query = reinterpret_cast<uint32_t(MFD_WIDGET_EDITOR_CALL*)(MfdWidgetEditorApiV1*)>(
            GetProcAddress(module_, "mfd_get_widgets_editor_api_v1"));
        MfdWidgetEditorApiV1 api {}; api.struct_size = sizeof(api);
        return query && query(&api) == 0U && api.struct_size >= sizeof(api) && api.abi_version == 1U &&
               (api.capabilities & MFD_WIDGET_EDITOR_CAPABILITY_DESIGNER) != 0 && api.draw;
    }
private:
    HMODULE module_;
};
#endif
}
bool ProbeWidgetEditor(const std::filesystem::path& windowFile, std::string& error)
{
#if defined(_WIN32)
    std::array<wchar_t, 32768> executable {};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length && length < executable.size() && ScopedWidgetEditorModule(
        std::filesystem::path(std::wstring(executable.data(), length)).parent_path() / "mfd_editor_widgets_api1.dll").Compatible())
    { error.clear(); return true; }
    if (ScopedWidgetEditorModule(windowFile.parent_path() / "mfd_editor_widgets_api1.dll").Compatible())
    { error.clear(); return true; }
#endif
    error = "Widget import requires compatible mfd_editor_widgets_api1.dll for " + windowFile.string();
    return false;
}
std::string WidgetAssetService::ReadBounded(const std::filesystem::path& file, const std::size_t maximum)
{
    if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > maximum)
        throw std::runtime_error("Missing or oversized widget dependency: " + file.string());
    std::ifstream input(file, std::ios::binary);
    std::string bytes(static_cast<std::size_t>(std::filesystem::file_size(file)), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Widget dependency changed while reading: " + file.string());
    return bytes;
}
WidgetSourceSnapshot WidgetAssetService::Snapshot(const mfd::MfdDocument& document,
    const std::filesystem::path& pageFile, const std::filesystem::path& root)
{
    WidgetSourceSnapshot result;
    AddSource(result, pageFile, root);
    for (const auto& page : document.pages)
        for (const auto& widget : page.widgets)
        {
            AddSource(result, widget.definition->sourceFile, root);
            for (const auto& visual : widget.definition->visuals) AddReticleSources(result, visual.composition, root);
        }
    // The existing template loader is flat; snapshot its JSON sources as well as expanded image resources.
    if (std::filesystem::is_directory(document.reticleLibraryFolder))
        for (const auto& entry : std::filesystem::directory_iterator(document.reticleLibraryFolder))
            if (entry.path().extension() == ".json") AddSource(result, entry.path(), root);
    for (const auto& reticle : document.reticleLibrary) AddReticleSources(result, reticle.second, root);
    return result;
}
std::shared_ptr<const mfd::api1::WidgetDefinition> WidgetAssetService::Stage(const mfd::api1::WidgetDefinition& source,
    const std::filesystem::path& sourceRoot, const std::filesystem::path& targetRoot,
    mfd::MfdDocument& document, EditorFileLayout& files)
{
    auto definition = StageResources(source, sourceRoot, targetRoot, files, nullptr);
    const auto signature = Signature(*definition, targetRoot);
    for (const auto& existing : document.widgetLibrary)
        if (Signature(*existing, targetRoot) == signature) return existing;
    const auto stem = SafeStem(source.id);
    for (std::size_t suffix = 0; suffix < 8192; ++suffix)
    {
        definition->id = source.id + (suffix ? "_Imported" + std::to_string(suffix) : "");
        const auto file = Confined(targetRoot / "widgets" /
            (stem + (suffix ? "_Imported" + std::to_string(suffix) : "") + ".json"), targetRoot);
        if (std::any_of(document.widgetLibrary.begin(), document.widgetLibrary.end(),
            [&file, &definition](const auto& value) { return value->sourceFile == file || mfd::PageNamesEqual(value->id, definition->id); })) continue;
        if (std::filesystem::exists(file))
        {
            const auto existing = mfd::JsonLoader {}.LoadWidgetAsset(file, targetRoot, document.reticleLibrary);
            if (Signature(*existing, targetRoot) == signature) { document.widgetLibrary.push_back(existing); return existing; }
            continue;
        }
        definition->sourceFile = file;
        if (document.widgetLibrary.size() >= 4096) throw std::runtime_error("Widget library is full.");
        document.widgetLibrary.push_back(definition);
        return definition;
    }
    throw std::runtime_error("Unable to choose a collision-free widget definition identity.");
}
void WidgetAssetService::StageReticle(mfd::ReticleGroup& reticle, const std::filesystem::path& sourceRoot,
                                      const std::filesystem::path& targetRoot, EditorFileLayout& files)
{
    for (auto& primitive : reticle.primitives)
        if (auto* image = std::get_if<mfd::ImageGeometry>(&primitive.geometry))
            image->file = StageResource(image->file, sourceRoot, targetRoot, files, nullptr);
}
void WidgetAssetService::Export(const mfd::api1::WidgetDefinition& source, const std::filesystem::path& sourceRoot,
                                const std::filesystem::path& folder, const EditorFileLayout& sourceFiles)
{
    const auto root = std::filesystem::weakly_canonical(folder);
    EditorFileLayout staged;
    auto definition = StageResources(source, sourceRoot, root, staged, &sourceFiles);
    definition->sourceFile = Confined(root / "widgets" / (SafeStem(source.id) + ".json"), root);
    std::vector<internal::AssetFileWrite> writes;
    const auto json = mfd::api1::SerializeWidgetDefinition(*definition, SerializeVisual).dump(2) + "\n";
    if (std::filesystem::exists(definition->sourceFile))
    {
        if (ReadBounded(definition->sourceFile) != json) throw std::runtime_error("Export would replace a different widget definition.");
    }
    else writes.push_back({definition->sourceFile, json, false});
    for (const auto& resource : staged.stagedResources) writes.push_back({resource.first, resource.second, false});
    for (const auto& write : writes) std::filesystem::create_directories(write.target.parent_path());
    internal::CommitAssetFiles(writes, {});
}
}
