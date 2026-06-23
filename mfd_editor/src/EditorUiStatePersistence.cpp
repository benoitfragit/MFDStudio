/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorUiStatePersistence.h"

/**
 * @file
 * @brief Implementation of the editor workspace layout persistence helpers.
 */

#include <cmath>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>

#include "internal/application/EditorViewportGrid.h"

namespace editor
{
namespace
{
using json = nlohmann::json;

bool IsFinitePositiveWidth(const double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

void ReadOptionalWidth(const json& root, const char* key, std::optional<float>& destination)
{
    if (!root.contains(key) || !root.at(key).is_number())
    {
        return;
    }

    const double value = root.at(key).get<double>();
    if (IsFinitePositiveWidth(value))
    {
        destination = static_cast<float>(value);
    }
}

void ReadOptionalFlag(const json& root, const char* key, std::optional<bool>& destination)
{
    if (!root.contains(key) || !root.at(key).is_boolean())
    {
        return;
    }

    destination = root.at(key).get<bool>();
}

void ReadOptionalGridStep(const json& root, const char* key, std::optional<float>& destination)
{
    if (!root.contains(key) || !root.at(key).is_number())
    {
        return;
    }

    const double value = root.at(key).get<double>();
    if (!std::isfinite(value) || value <= 0.0)
    {
        return;
    }

    destination = editor::app::SanitizeGridStepLogical(static_cast<float>(value));
}
} // namespace

EditorUiPersistentState LoadEditorUiState(const std::filesystem::path& file)
{
    EditorUiPersistentState state;

    std::error_code existsError;
    if (!std::filesystem::exists(file, existsError) || existsError)
    {
        return state;
    }

    try
    {
        std::ifstream stream(file);
        if (!stream.is_open())
        {
            return state;
        }

        json root;
        stream >> root;
        if (!root.is_object())
        {
            return state;
        }

        ReadOptionalWidth(root, "sidebarWidth", state.sidebarWidth);
        ReadOptionalFlag(root, "sidebarVisible", state.sidebarVisible);
        ReadOptionalWidth(root, "inspectorWidth", state.inspectorWidth);
        ReadOptionalFlag(root, "inspectorVisible", state.inspectorVisible);
        ReadOptionalFlag(root, "showGrid", state.showGrid);
        ReadOptionalFlag(root, "snapToGrid", state.snapToGrid);
        ReadOptionalGridStep(root, "gridStepLogical", state.gridStepLogical);

        if (root.contains("sections") && root.at("sections").is_object())
        {
            for (const auto& entry : root.at("sections").items())
            {
                if (entry.value().is_boolean())
                {
                    state.sectionOpen[entry.key()] = entry.value().get<bool>();
                }
            }
        }
    }
    catch (const std::exception&)
    {
        return EditorUiPersistentState {};
    }

    return state;
}

void SaveEditorUiState(const std::filesystem::path& file, const EditorUiPersistentState& state)
{
    try
    {
        json root = json::object();
        if (state.sidebarWidth.has_value())
        {
            root["sidebarWidth"] = *state.sidebarWidth;
        }
        if (state.sidebarVisible.has_value())
        {
            root["sidebarVisible"] = *state.sidebarVisible;
        }
        if (state.inspectorWidth.has_value())
        {
            root["inspectorWidth"] = *state.inspectorWidth;
        }
        if (state.inspectorVisible.has_value())
        {
            root["inspectorVisible"] = *state.inspectorVisible;
        }
        if (state.showGrid.has_value())
        {
            root["showGrid"] = *state.showGrid;
        }
        if (state.snapToGrid.has_value())
        {
            root["snapToGrid"] = *state.snapToGrid;
        }
        if (state.gridStepLogical.has_value())
        {
            root["gridStepLogical"] = editor::app::SanitizeGridStepLogical(*state.gridStepLogical);
        }

        json sections = json::object();
        for (const auto& entry : state.sectionOpen)
        {
            sections[entry.first] = entry.second;
        }
        root["sections"] = std::move(sections);

        if (file.has_parent_path())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(file.parent_path(), directoryError);
        }

        std::ofstream stream(file, std::ios::trunc);
        if (stream.is_open())
        {
            stream << root.dump(2);
        }
    }
    catch (const std::exception&)
    {
        // Best-effort persistence: a read-only asset tree must not break editor shutdown.
    }
}
} // namespace editor
