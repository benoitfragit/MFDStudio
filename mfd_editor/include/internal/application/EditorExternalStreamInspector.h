#pragma once
#include <optional>
#include "mfd/model/Reticle.h"
#include "ExternalStreamRegistry.h"

namespace editor::app
{
/** @brief Collects an external stream edit without mutating the document or its resources. */
std::optional<mfd::ExternalStreamGeometry> DrawExternalStreamInspector(
    const mfd::ExternalStreamGeometry& geometry, const mfd::ExternalStreamRegistry& registry);
/** @brief Applies a validated stream edit after the caller records an undo snapshot. */
bool ApplyExternalStreamEdit(mfd::Primitive& primitive, const mfd::ExternalStreamGeometry& geometry);
}
