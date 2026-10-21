/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Editor-side owner of the optional LEVEL 1 authoring DLL. */

#include <filesystem>
#include <memory>
#include <string>

#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1
{
/** @brief Owns extension registration and bounded host-ImGui facade callbacks. */
class ContainerEditorHost final
{
public:
    ContainerEditorHost();
    ~ContainerEditorHost();
    ContainerEditorHost(const ContainerEditorHost&) = delete;
    ContainerEditorHost& operator=(const ContainerEditorHost&) = delete;

    /** @brief Loads and validates both editor and runtime LEVEL 1 extensions. */
    [[nodiscard]] bool EnsureLoaded(const std::filesystem::path& windowFile);
    /** @brief Unregisters callbacks before releasing both DLL lifetimes. */
    void Unload() noexcept;
    /** @brief Returns whether the editor extension can render its controls. */
    [[nodiscard]] bool IsAvailable() const noexcept;
    /** @brief Returns the last load or handshake diagnostic. */
    [[nodiscard]] const std::string& Diagnostic() const noexcept;
    /**
     * @brief Draws extension-owned controls into the current host ImGui frame.
     * @param container Mutable selected authored container.
     * @param placement Optional parent-owned placement for a non-root container.
     * @param parent Optional parent definition for a non-root container.
     * @return `true` when controls changed the definition.
     */
    [[nodiscard]] bool DrawInspector(ContainerDefinition& container,
                                     ContainerChildPlacement* placement,
                                     const ContainerDefinition* parent) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::api1
