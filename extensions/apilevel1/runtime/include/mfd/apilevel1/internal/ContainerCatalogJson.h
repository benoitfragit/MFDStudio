#pragma once
#include "mfd/apilevel1/ContainerJson.h"
namespace mfd::api1::detail
{
MFD_API std::vector<ContainerDefinition> ParseContainerCatalog(const nlohmann::json& pageNode);
MFD_API nlohmann::json SerializeContainerCatalog(const std::vector<ContainerDefinition>& containers);
}
