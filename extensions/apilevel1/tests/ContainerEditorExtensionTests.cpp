/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Editor DLL UI-facade handshake and lifecycle tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerEditorAbi.h"

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

namespace
{
struct FacadeState
{
    int calls = 0;
};

void MFD_CONTAINER_EDITOR_CALL CountText(void* context, const char*)
{
    ++static_cast<FacadeState*>(context)->calls;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountCheckbox(void* context, const char*, std::uint8_t*)
{
    ++static_cast<FacadeState*>(context)->calls;
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountDragFloat(void* context, const char*, float*, float, float, float)
{
    ++static_cast<FacadeState*>(context)->calls;
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountDragFloat2(void* context, const char*, float*, float, float, float)
{
    ++static_cast<FacadeState*>(context)->calls;
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountCombo(void* context, const char*, std::int32_t*, const char* const*, std::size_t)
{
    ++static_cast<FacadeState*>(context)->calls;
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountColor(void* context, const char*, float*)
{
    ++static_cast<FacadeState*>(context)->calls;
    return 0U;
}
} // namespace

TEST(ContainerEditorExtensionTests, ValidatesAbiAndDrawsThroughHostOwnedFacade)
{
    MfdContainerEditorApiV1 api {};
    api.struct_size = sizeof(api) - 1U;
    EXPECT_NE(mfd_get_container_editor_api_v1(&api), 0U);
    api = {};
    api.struct_size = sizeof(api);
    ASSERT_EQ(mfd_get_container_editor_api_v1(&api), 0U);
    ASSERT_NE(api.draw_inspector, nullptr);

    FacadeState state;
    MfdContainerEditorUiFacadeV1 facade {
        sizeof(MfdContainerEditorUiFacadeV1), MFD_CONTAINER_EDITOR_UI_FACADE_VERSION, &state,
        &CountText, &CountCheckbox, &CountDragFloat, &CountDragFloat2, &CountCombo, &CountColor};
    std::uint32_t type = 0U;
    std::uint32_t sizeMode = 0U;
    std::uint32_t orientation = 0U;
    std::uint32_t alignment = 0U;
    std::uint32_t rows = 1U;
    std::uint32_t columns = 1U;
    std::uint32_t horizontalScroll = 0U;
    std::uint32_t verticalScroll = 0U;
    std::uint8_t visible = 1U;
    std::uint8_t enabled = 1U;
    std::uint8_t exposed = 0U;
    std::uint8_t clipContent = 1U;
    std::uint8_t backgroundVisible = 0U;
    std::uint8_t borderVisible = 0U;
    std::array<float, 2U> position {};
    std::array<float, 2U> size {0.5f, 0.5f};
    std::array<float, 2U> minimumSize {0.02f, 0.02f};
    std::array<float, 4U> padding {};
    std::array<float, 2U> scrollOffset {};
    std::array<float, 4U> background {};
    std::array<float, 4U> border {};
    std::array<float, 4U> track {};
    std::array<float, 4U> thumb {};
    std::array<float, 4U> hover {};
    std::array<float, 4U> active {};
    std::array<float, 4U> margins {};
    float spacing = 0.0f;
    float scrollbarThickness = 0.025f;
    float minimumThumb = 0.05f;
    float borderThickness = 0.0f;
    float cornerRadius = 0.0f;
    float scrollbarRadius = 0.0f;
    std::uint32_t childResizeMode = 1U;
    std::uint32_t childHorizontalAlignment = 0U;
    std::uint32_t childVerticalAlignment = 0U;
    std::uint32_t childRow = 0U;
    std::uint32_t childColumn = 0U;
    std::uint32_t childRowSpan = 1U;
    std::uint32_t childColumnSpan = 1U;
    std::int32_t childRenderOrder = 0;
    MfdContainerEditorSelectionV1 selection {
        sizeof(MfdContainerEditorSelectionV1), "main", &type, &sizeMode,
        position.data(), size.data(), minimumSize.data(),
        &visible, &enabled, &exposed, padding.data(), &spacing, &orientation, &alignment, &rows, &columns,
        &horizontalScroll, &verticalScroll, scrollOffset.data(), &scrollbarThickness, &minimumThumb, &clipContent,
        &backgroundVisible, &borderVisible, background.data(), border.data(), track.data(), thumb.data(),
        hover.data(), active.data(), &borderThickness, &cornerRadius, &scrollbarRadius, margins.data(),
        "parent", 1U, 0U, &childResizeMode, &childHorizontalAlignment, &childVerticalAlignment,
        &childRow, &childColumn, &childRowSpan, &childColumnSpan, &childRenderOrder};

    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
    EXPECT_GT(state.calls, 20);
    facade.version = MFD_CONTAINER_EDITOR_UI_FACADE_VERSION + 1U;
    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
}
