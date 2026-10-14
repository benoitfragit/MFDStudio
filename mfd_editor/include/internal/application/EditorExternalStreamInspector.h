#pragma once
#include <optional>
#include "mfd/model/Reticle.h"
#include "ExternalStreamRegistry.h"

namespace editor::app
{
/** @brief Collects an external stream edit without mutating the document or its resources.
 * @param geometry Current authored stream settings.
 * @param registry Stream feedback displayed beneath the controls.
 * @param beganEdit Receives whether this change starts an undoable gesture.
 * @return Valid edited geometry, or no value when unchanged or invalid.
 */
std::optional<mfd::ExternalStreamGeometry> DrawExternalStreamInspector(
    const mfd::ExternalStreamGeometry& geometry, const mfd::ExternalStreamRegistry& registry, bool& beganEdit);
/** @brief Applies a validated stream edit after the caller records an undo snapshot. */
bool ApplyExternalStreamEdit(mfd::Primitive& primitive, const mfd::ExternalStreamGeometry& geometry);
}
