/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Undo-friendly domain mutations used by the LEVEL 1 editor UI. */

#include <string>
#include <string_view>

#include "mfd/model/PageDefinition.h"

namespace mfd::api1
{
/** @brief Named container hierarchy mutations kept independent from ImGui. */
class ContainerEditorModel final
{
public:
    /** @brief Adds one root container and returns its stable identifier. */
    [[nodiscard]] static std::string AddRoot(PageDefinition& page, ContainerType type);
    /** @brief Adds one direct child to an existing container. */
    [[nodiscard]] static std::string AddChild(PageDefinition& page,
                                              std::string_view parentId,
                                              ContainerType type,
                                              std::string& error);
    /** @brief Renames a container and every ownership edge referring to it. */
    [[nodiscard]] static bool Rename(PageDefinition& page,
                                     std::string_view containerId,
                                     std::string newId,
                                     std::string& error);
    /** @brief Duplicates a selected container subtree without sharing ownership. */
    [[nodiscard]] static std::string Duplicate(PageDefinition& page,
                                               std::string_view containerId,
                                               std::string& error);
    /** @brief Deletes a selected container and all descendants it exclusively owns. */
    [[nodiscard]] static bool Delete(PageDefinition& page,
                                     std::string_view containerId,
                                     std::string& error);
    /**
     * @brief Reparents one subtree, or makes it a root when `newParentId` is empty.
     * @return `false` for unknown ids, self-parenting or a hierarchy cycle.
     */
    [[nodiscard]] static bool Reparent(PageDefinition& page,
                                       std::string_view containerId,
                                       std::string_view newParentId,
                                       std::string& error);
};
} // namespace mfd::api1
