/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorPagePreviewDrawOrderController.h"

/**
 * @file
 * @brief Implementation of the page-preview draw-order controller.
 */

#include <algorithm>

#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace
{
const mfd::ReticleGroup* FindStaticReticle(const mfd::PageDefinition& page, const int reticleIndex) noexcept
{
    if (reticleIndex < 0 || reticleIndex >= static_cast<int>(page.staticReticles.size()))
    {
        return nullptr;
    }

    return &page.staticReticles[static_cast<std::size_t>(reticleIndex)];
}

struct StaticReticleDrawOrderLess
{
    const editor::PagePreviewDrawOrderController* controller = nullptr;
    const mfd::PageDefinition* page = nullptr;

    bool operator()(const int lhs, const int rhs) const noexcept
    {
        return controller->BuildStaticReticleDrawOrderKey(*page, lhs) <
               controller->BuildStaticReticleDrawOrderKey(*page, rhs);
    }
};
} // namespace

namespace editor
{
bool operator<(const PagePreviewDrawOrderKey& lhs, const PagePreviewDrawOrderKey& rhs) noexcept
{
    if (lhs.pass != rhs.pass)
    {
        return lhs.pass < rhs.pass;
    }

    const bool useLayerOrder =
        lhs.pass == PagePreviewDrawPass::Normal || lhs.pass == PagePreviewDrawPass::DrawOnTop;
    if (useLayerOrder && lhs.layerOrder != rhs.layerOrder)
    {
        return lhs.layerOrder < rhs.layerOrder;
    }

    return lhs.authoredOrder < rhs.authoredOrder;
}

void PagePreviewDrawOrderController::CollectStaticReticleDrawOrder(const mfd::PageDefinition& page,
                                                                   std::vector<int>& destination) const
{
    destination.clear();
    destination.reserve(page.staticReticles.size());

    for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
    {
        destination.push_back(reticleIndex);
    }

    std::sort(destination.begin(), destination.end(), StaticReticleDrawOrderLess {this, &page});
}

PagePreviewDrawOrderKey PagePreviewDrawOrderController::BuildStaticReticleDrawOrderKey(
    const mfd::PageDefinition& page,
    const int reticleIndex) const noexcept
{
    PagePreviewDrawOrderKey key;
    const mfd::ReticleGroup* reticle = FindStaticReticle(page, reticleIndex);
    if (reticle == nullptr)
    {
        key.layerOrder = page.layers.size();
        key.authoredOrder = page.staticReticles.size();
        return key;
    }

    key.pass = reticle->drawOnTop ? PagePreviewDrawPass::DrawOnTop : PagePreviewDrawPass::Normal;
    key.layerOrder = editor::app::PageLayerOrder(page, reticle->layerId);
    key.authoredOrder = static_cast<std::size_t>(reticleIndex);
    return key;
}

PagePreviewDrawOrderKey PagePreviewDrawOrderController::BuildStrobeDrawOrderKey(
    const std::size_t strobeIndex) const noexcept
{
    PagePreviewDrawOrderKey key;
    key.pass = PagePreviewDrawPass::Strobe;
    key.authoredOrder = strobeIndex;
    return key;
}

PagePreviewDrawOrderKey PagePreviewDrawOrderController::BuildTitleDrawOrderKey() const noexcept
{
    PagePreviewDrawOrderKey key;
    key.pass = PagePreviewDrawPass::Title;
    return key;
}
} // namespace editor
