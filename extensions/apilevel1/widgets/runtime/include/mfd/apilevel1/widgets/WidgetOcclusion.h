#pragma once
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/MfdExport.h"
namespace mfd::api1
{
/** @brief Optional host sampler receives normalized image coordinates (top-left origin). */
using WidgetImageOcclusionSampler = bool (*)(void*, const ImageGeometry&, Vec2) noexcept;
/** @brief Tests legacy painted geometry; a host sampler resolves transparent image pixels. */
MFD_API bool ReticleOccludesWidgetPointer(const ReticleGroup& reticle, Vec2 pointer,
    WidgetImageOcclusionSampler imageSampler = nullptr, void* imageContext = nullptr) noexcept;
/** @brief Whether the reticle's supported clipping primitive restores background at this point. */
MFD_API bool ReticleErasesWidgetOcclusion(const ReticleGroup& reticle, Vec2 pointer) noexcept;
}
