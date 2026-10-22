#pragma once
#include "mfd/apilevel1/widgets/WidgetTypes.h"
namespace mfd::api1
{
/** @brief Resolve authored visual state and bounded numeric bindings through the existing reticle renderer. */
MFD_API ReticleGroup BuildWidgetVisual(const WidgetInstance& widget, const WidgetState& state, const ContainerRect& bounds);
MFD_API ContainerRect WidgetHitRegion(const WidgetInstance& widget, const ContainerRect& bounds) noexcept;
}
