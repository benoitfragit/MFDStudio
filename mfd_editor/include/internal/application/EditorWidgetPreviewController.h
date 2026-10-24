#pragma once

/**
 * @file
 * @brief Session-only widget interaction controller for the editor page preview.
 */

#include "mfd/apilevel1/widgets/WidgetTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace editor::app
{
/** @brief One logical-space pointer sample consumed by widget preview test mode. */
struct WidgetPreviewPointerSample
{
    mfd::Vec2 pointer {};
    bool down = false;
    bool began = false;
    bool released = false;
    bool cancel = false;
    bool blocked = false;
};

/**
 * @brief Evaluates interactive widgets in the editor without mutating authored assets or undo history.
 *
 * The controller mirrors runtime input semantics while confirming proposed values immediately. Its
 * state is intentionally ephemeral and is discarded whenever test mode is disabled.
 */
class EditorWidgetPreviewController final
{
public:
    /** @brief Returns whether widget test mode currently owns left-button preview input. */
    [[nodiscard]] bool Enabled() const noexcept;

    /**
     * @brief Enables or disables widget test mode.
     * @param enabled New mode state. Disabling clears all transient interaction values.
     */
    void SetEnabled(bool enabled) noexcept;

    /** @brief Clears capture, hover, pressed and locally confirmed test values. */
    void Reset() noexcept;

    /** @brief Cancels the active pointer gesture while preserving locally confirmed test values. */
    void CancelPointerInteraction() noexcept;

    /**
     * @brief Reconciles transient states with the current authored widget list.
     * @param widgets Current active-page widget instances in layout order.
     */
    void Synchronize(const std::vector<mfd::api1::WidgetInstance>& widgets);

    /**
     * @brief Evaluates one pointer sample against the topmost visible widget.
     * @param containers Authored containers used for hierarchy clipping and occlusion.
     * @param layouts Evaluated mixed container/widget layouts.
     * @param widgets Widget instances corresponding to the final projected layout nodes.
     * @param sample Finite logical pointer coordinates and button transition flags.
     * @return `true` when a widget hit region or active capture consumed the sample.
     */
    bool Update(const std::vector<mfd::api1::ContainerDefinition>& containers,
                const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
                const std::vector<mfd::api1::WidgetInstance>& widgets,
                const WidgetPreviewPointerSample& sample);

    /**
     * @brief Resolves the state used to render one widget in the page preview.
     * @param index Widget index in the current active page.
     * @param widget Authored widget instance used when no transient state exists.
     * @param effectiveVisible Visibility resolved by the container layout.
     * @param effectiveEnabled Enabled state resolved by the container layout.
     * @return A value copy safe to adjust for editor-only visual-state inspection.
     */
    [[nodiscard]] mfd::api1::WidgetState PreviewState(std::size_t index,
                                                       const mfd::api1::WidgetInstance& widget,
                                                       bool effectiveVisible,
                                                       bool effectiveEnabled) const;

private:
    struct Entry
    {
        std::string id;
        const mfd::api1::WidgetDefinition* definition = nullptr;
        mfd::api1::WidgetState authoredState;
        mfd::api1::WidgetState state;
    };

    [[nodiscard]] static Entry BuildEntry(const mfd::api1::WidgetInstance& widget);
    [[nodiscard]] static bool MatchesAuthoredState(const Entry& entry,
                                                   const mfd::api1::WidgetInstance& widget) noexcept;

    bool enabled_ = false;
    std::string capturedWidgetId_;
    std::vector<Entry> entries_;
    std::vector<const mfd::api1::ContainerLayoutNode*> layoutsBySource_;
};
} // namespace editor::app
