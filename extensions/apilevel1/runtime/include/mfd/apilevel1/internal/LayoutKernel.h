#pragma once
#include "mfd/apilevel1/ContainerTypes.h"
namespace mfd::api1::detail
{
bool ValidateLayoutNodes(const std::vector<ContainerDefinition>& nodes, std::size_t maximumCount,
                         std::size_t maximumDepth, std::string& error);
bool EvaluateLayoutNodes(const std::vector<ContainerDefinition>& nodes, std::size_t maximumCount,
                         std::size_t maximumDepth, std::vector<ContainerLayoutNode>& output, std::string& error);
}
