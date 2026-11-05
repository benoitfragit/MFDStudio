/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <imgui.h>
#include <filesystem>
#include <functional>
#include <string>

namespace editor::app
{
/** @brief Bind the primitive inspector's ImGui state to the selected template and primitive.
 * @note The primitive index stays stable during editing, including when its editable ID is empty or renamed.
 */
class PrimitiveInspectorScope final
{
public:
    PrimitiveInspectorScope(const std::filesystem::path& windowFile, const std::string& templateId, int primitiveIndex)
    {
        ImGui::PushID("LibraryPrimitiveInspector");
        // Authored names are data. Do not interpret a literal "###" as an ImGui label suffix.
        ImGui::PushID(static_cast<int>(std::filesystem::hash_value(windowFile)));
        ImGui::PushID(static_cast<int>(std::hash<std::string> {}(templateId)));
        ImGui::PushID(primitiveIndex);
    }

    ~PrimitiveInspectorScope()
    {
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
    }

    PrimitiveInspectorScope(const PrimitiveInspectorScope&) = delete;
    PrimitiveInspectorScope& operator=(const PrimitiveInspectorScope&) = delete;
};
}
