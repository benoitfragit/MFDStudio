/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorPrimitiveClipboardService.h"

/**
 * @file
 * @brief Implementation of the reticle-studio primitive clipboard helper.
 */

#include <algorithm>
#include <string>
#include <string_view>

#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
bool PrimitiveIdExistsNormalized(const mfd::ReticleGroup& reticle, const std::string_view candidateId)
{
    const std::string normalizedCandidateId = mfd::NormalizePageName(candidateId);
    if (normalizedCandidateId.empty())
    {
        return false;
    }

    return std::any_of(reticle.primitives.begin(),
                       reticle.primitives.end(),
                       [&normalizedCandidateId](const mfd::Primitive& primitive)
                       {
                           return mfd::NormalizePageName(primitive.id) == normalizedCandidateId;
                       });
}

std::string MakeStablePrimitivePasteBaseId(const mfd::Primitive& primitive)
{
    const std::string rawBaseId = primitive.id.empty() ? std::string {"primitive_copy"} : primitive.id + "_copy";
    return mfd::NormalizePageName(rawBaseId).empty() ? std::string {"primitive_copy"} : rawBaseId;
}

std::string MakeUniquePrimitivePasteId(const mfd::ReticleGroup& reticle, const mfd::Primitive& primitive)
{
    const std::string stableBaseId = MakeStablePrimitivePasteBaseId(primitive);
    std::string candidate = stableBaseId;
    int suffix = 1;

    while (PrimitiveIdExistsNormalized(reticle, candidate))
    {
        candidate = stableBaseId + "_" + std::to_string(suffix++);
    }

    return candidate;
}

int MakePrimitiveInsertionIndex(const mfd::ReticleGroup& reticle, const int selectedPrimitiveIndex) noexcept
{
    if (selectedPrimitiveIndex < 0 || selectedPrimitiveIndex >= static_cast<int>(reticle.primitives.size()))
    {
        return static_cast<int>(reticle.primitives.size());
    }

    return selectedPrimitiveIndex + 1;
}
} // namespace

PrimitivePastePlan PrimitiveClipboardService::BuildPastePlan(const PrimitivePasteRequest& request) const
{
    PrimitivePastePlan plan;
    plan.insertionIndex = MakePrimitiveInsertionIndex(request.targetReticle, request.selectedPrimitiveIndex);
    plan.primitive = request.copiedPrimitive;
    plan.primitive.id = MakeUniquePrimitivePasteId(request.targetReticle, request.copiedPrimitive);
    plan.primitive.transform.position.x += request.positionOffset.x;
    plan.primitive.transform.position.y += request.positionOffset.y;
    return plan;
}
} // namespace editor
