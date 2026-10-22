#pragma once
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/MfdExport.h"
namespace mfd::api1
{
/** @brief Conservative geometry hulls block input behind legacy content; fully transparent primitives do not. */
MFD_API bool ReticleOccludesWidgetPointer(const ReticleGroup& reticle, Vec2 pointer) noexcept;
}
