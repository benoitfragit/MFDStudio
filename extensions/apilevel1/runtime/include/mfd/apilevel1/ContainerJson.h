/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief JSON conversion for API LEVEL 1 page container content. */

#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "mfd/MfdExport.h"
#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1
{
/**
 * @brief Parses and validates a page's optional `containers` array.
 * @param pageNode Page JSON object.
 * @return Validated flat page-local container catalog.
 * @throws std::runtime_error on malformed, cyclic or non-finite content.
 */
MFD_API std::vector<ContainerDefinition> ParsePageContainers(const nlohmann::json& pageNode);

/**
 * @brief Serializes a validated container catalog using the stable LEVEL 1 schema.
 * @param containers Catalog to serialize.
 * @return JSON array; empty when the page has no containers.
 * @throws std::runtime_error when the in-memory hierarchy is invalid.
 */
MFD_API nlohmann::json SerializePageContainers(const std::vector<ContainerDefinition>& containers);
} // namespace mfd::api1
