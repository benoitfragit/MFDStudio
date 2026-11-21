#pragma once
#include "mfd/apilevel1/widgets/WidgetEditorAbi.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
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
    /** @brief Detect focus loss before a canvas or sidebar click replaces the inspected widget. */
    bool LeavingTextField() const noexcept;
    /**
     * @brief Draws the editor DLL inspector for one validated draft.
     * @param draft Host-owned draft; validated again after the call.
     * @param parentChoices Container ids offered by the parent picker in addition to the Page root.
     * @return Validated action flags, or 0 when the DLL is missing or corrupted the draft.
     */
    std::uint32_t Draw(MfdWidgetEditorDraftV1& draft, const std::vector<std::string>& parentChoices = {}) const;
    void Unload() noexcept;
private:
    bool Load(const std::filesystem::path& file);
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string diagnostic_;
};
}
