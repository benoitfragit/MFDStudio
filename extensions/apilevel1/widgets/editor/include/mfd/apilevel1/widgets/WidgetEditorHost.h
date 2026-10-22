#pragma once
#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
#include <filesystem>
#include <memory>
#include <string>
namespace mfd::api1
{
/** @brief Keeps the module alive for all synchronous calls; never owns an ImGui context. */
class WidgetEditorHost final
{
public:
    WidgetEditorHost();
    ~WidgetEditorHost();
    WidgetEditorHost(const WidgetEditorHost&) = delete;
    WidgetEditorHost& operator=(const WidgetEditorHost&) = delete;
    bool EnsureLoaded(const std::filesystem::path& windowFile);
    bool IsLoaded() const noexcept;
    const std::string& Diagnostic() const noexcept;
    std::uint32_t Draw(MfdWidgetEditorDraftV1& draft) const;
    void Unload() noexcept;
private:
    bool Load(const std::filesystem::path& file);
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string diagnostic_;
};
}
