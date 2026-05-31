/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal controller aligning editor page-preview draw order with runtime page-layer order.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "mfd/model/PageDefinition.h"

namespace editor
{
/**
 * @brief Draw passes used by the page preview.
 */
enum class PagePreviewDrawPass : std::uint8_t
{
    Normal = 0,
    DrawOnTop = 1,
    Strobe = 2,
    Title = 3
};

/**
 * @brief Comparable draw-order key used by page-preview rendering and hit-testing.
 */
struct PagePreviewDrawOrderKey
{
    /** @brief Pass grouping matching the preview rendering pipeline. */
    PagePreviewDrawPass pass = PagePreviewDrawPass::Normal;
    /** @brief Authored runtime layer order used for static page reticles. */
    std::size_t layerOrder = 0U;
    /** @brief Stable authored order within one pass. */
    std::size_t authoredOrder = 0U;
};

/**
 * @brief Returns whether the left key is drawn before the right key.
 * @param lhs Left draw-order key.
 * @param rhs Right draw-order key.
 * @return `true` when the left preview element must be rendered first.
 */
[[nodiscard]] bool operator<(const PagePreviewDrawOrderKey& lhs, const PagePreviewDrawOrderKey& rhs) noexcept;

/**
 * @brief Stateless controller resolving page-preview draw order from authored runtime data.
 */
class PagePreviewDrawOrderController
{
public:
    /**
     * @brief Collects static page-reticle indices in the order the preview must draw them.
     * @param page Page whose static reticles must be ordered.
     * @param destination Output vector cleared then filled with zero-based reticle indices.
     */
    void CollectStaticReticleDrawOrder(const mfd::PageDefinition& page, std::vector<int>& destination) const;

    /**
     * @brief Builds the draw-order key for one static page reticle.
     * @param page Page owning the reticle and its runtime layers.
     * @param reticleIndex Zero-based static-reticle index.
     * @return Key matching the runtime layer order plus the preview `drawOnTop` pass.
     */
    [[nodiscard]] PagePreviewDrawOrderKey BuildStaticReticleDrawOrderKey(const mfd::PageDefinition& page,
                                                                         int reticleIndex) const noexcept;

    /**
     * @brief Builds the draw-order key for one page strobe preview reticle.
     * @param strobeIndex Zero-based strobe index in authored order.
     * @return Key placing the strobe after static reticles and before the page title.
     */
    [[nodiscard]] PagePreviewDrawOrderKey BuildStrobeDrawOrderKey(std::size_t strobeIndex) const noexcept;

    /**
     * @brief Builds the draw-order key for the editor-only page-title preview reticle.
     * @return Key placing the page title above static reticles and strobes.
     */
    [[nodiscard]] PagePreviewDrawOrderKey BuildTitleDrawOrderKey() const noexcept;
};
} // namespace editor
