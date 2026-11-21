/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Editor DLL UI-facade handshake and lifecycle tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerEditorAbi.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
struct FacadeState
{
    int calls = 0;
    int sectionCalls = 0;
    std::vector<std::string> labels {};
};

void Record(void* context, const char* label)
{
    auto& state = *static_cast<FacadeState*>(context);
    ++state.calls;
    state.labels.emplace_back(label == nullptr ? "" : label);
}

bool Drew(const FacadeState& state, const std::string& label)
{
    return std::find(state.labels.begin(), state.labels.end(), label) != state.labels.end();
}

void MFD_CONTAINER_EDITOR_CALL CountText(void* context, const char* label)
{
    Record(context, label);
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountSection(void* context, const char*, const char* label, std::uint8_t)
{
    Record(context, label);
    ++static_cast<FacadeState*>(context)->sectionCalls;
    return 1U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountCheckbox(void* context, const char* label, std::uint8_t*)
{
    Record(context, label);
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountDragFloat(void* context, const char* label, float*, float, float, float)
{
    Record(context, label);
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountDragFloat2(void* context, const char* label, float*, float, float, float)
{
    Record(context, label);
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountCombo(void* context, const char* label, std::int32_t*, const char* const*, std::size_t)
{
    Record(context, label);
    return 0U;
}

std::uint8_t MFD_CONTAINER_EDITOR_CALL CountColor(void* context, const char* label, float*)
{
    Record(context, label);
    return 0U;
}

/** @brief Owns every authored value pointed to by one inspector selection projection. */
class ContainerSelectionFixture
{
public:
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

    MfdContainerEditorSelectionV1 Selection()
    {
        return MfdContainerEditorSelectionV1 {
            sizeof(MfdContainerEditorSelectionV1), "main", &type, &sizeMode,
            position.data(), size.data(), minimumSize.data(),
            &visible, &enabled, &exposed, padding.data(), &spacing, &orientation, &alignment, &rows, &columns,
            &horizontalScroll, &verticalScroll, scrollOffset.data(), &scrollbarThickness, &minimumThumb, &clipContent,
            &backgroundVisible, &borderVisible, background.data(), border.data(), track.data(), thumb.data(),
            hover.data(), active.data(), &borderThickness, &cornerRadius, &scrollbarRadius, margins.data(),
            "parent", 1U, 0U, &childResizeMode, &childHorizontalAlignment, &childVerticalAlignment,
            &childRow, &childColumn, &childRowSpan, &childColumnSpan, &childRenderOrder};
    }
};

MfdContainerEditorApiV1 LoadContainerEditorApi()
{
    MfdContainerEditorApiV1 api {};
    api.struct_size = sizeof(api);
    EXPECT_EQ(mfd_get_container_editor_api_v1(&api), 0U);
    return api;
}

MfdContainerEditorUiFacadeV1 MakeFacade(FacadeState& state)
{
    return MfdContainerEditorUiFacadeV1 {
        sizeof(MfdContainerEditorUiFacadeV1), MFD_CONTAINER_EDITOR_UI_FACADE_VERSION, &state,
        &CountText, &CountCheckbox, &CountDragFloat, &CountDragFloat2, &CountCombo, &CountColor, &CountSection};
}
} // namespace

TEST(ContainerEditorExtensionTests, ValidatesAbiAndDrawsThroughHostOwnedFacade)
{
    MfdContainerEditorApiV1 api {};
    api.struct_size = sizeof(api) - 1U;
    EXPECT_NE(mfd_get_container_editor_api_v1(&api), 0U);
    api = LoadContainerEditorApi();
    ASSERT_NE(api.draw_inspector, nullptr);

    FacadeState state;
    MfdContainerEditorUiFacadeV1 facade = MakeFacade(state);
    ContainerSelectionFixture fixture;
    MfdContainerEditorSelectionV1 selection = fixture.Selection();

    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
    EXPECT_GT(state.calls, 15);
    EXPECT_GE(state.sectionCalls, 5);
    const int validCallCount = state.calls;
    facade.version = MFD_CONTAINER_EDITOR_UI_FACADE_VERSION + 1U;
    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
    EXPECT_EQ(state.calls, validCallCount);

    facade.version = MFD_CONTAINER_EDITOR_UI_FACADE_VERSION;
    facade.struct_size = offsetof(MfdContainerEditorUiFacadeV1, begin_section);
    facade.begin_section = nullptr;
    state = {};
    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
    EXPECT_GT(state.calls, 15);
    EXPECT_EQ(state.sectionCalls, 0);
}

TEST(ContainerEditorExtensionTests, ScrollbarSettingsAppearOnlyWhenScrollingIsEnabled)
{
    const MfdContainerEditorApiV1 api = LoadContainerEditorApi();
    ContainerSelectionFixture fixture;
    MfdContainerEditorSelectionV1 selection = fixture.Selection();

    FacadeState disabled;
    MfdContainerEditorUiFacadeV1 facade = MakeFacade(disabled);
    api.draw_inspector(&facade, &selection);
    EXPECT_TRUE(Drew(disabled, "Vertical scroll"));
    EXPECT_FALSE(Drew(disabled, "Scrollbar thickness"));
    EXPECT_FALSE(Drew(disabled, "Scrollbar style"));
    EXPECT_FALSE(Drew(disabled, "Track color"));

    fixture.verticalScroll = 1U;
    FacadeState enabled;
    facade = MakeFacade(enabled);
    api.draw_inspector(&facade, &selection);
    EXPECT_TRUE(Drew(enabled, "Scrollbar thickness"));
    EXPECT_TRUE(Drew(enabled, "Scrollbar style"));
    EXPECT_TRUE(Drew(enabled, "Track color"));
}

TEST(ContainerEditorExtensionTests, DecorationStyleAppearsOnlyForDrawnDecorationsAndKeepsHiddenValues)
{
    const MfdContainerEditorApiV1 api = LoadContainerEditorApi();
    ContainerSelectionFixture fixture;
    fixture.background = {0.1f, 0.2f, 0.3f, 0.4f};
    fixture.borderThickness = 0.02f;
    MfdContainerEditorSelectionV1 selection = fixture.Selection();

    FacadeState hidden;
    MfdContainerEditorUiFacadeV1 facade = MakeFacade(hidden);
    EXPECT_EQ(api.draw_inspector(&facade, &selection), 0U);
    EXPECT_TRUE(Drew(hidden, "Draw background"));
    EXPECT_FALSE(Drew(hidden, "Background color"));
    EXPECT_FALSE(Drew(hidden, "Border color"));
    EXPECT_FALSE(Drew(hidden, "Corner radius"));
    EXPECT_FLOAT_EQ(fixture.background[3], 0.4f);
    EXPECT_FLOAT_EQ(fixture.borderThickness, 0.02f);

    fixture.borderVisible = 1U;
    FacadeState border;
    facade = MakeFacade(border);
    api.draw_inspector(&facade, &selection);
    EXPECT_FALSE(Drew(border, "Background color"));
    EXPECT_TRUE(Drew(border, "Border color"));
    EXPECT_TRUE(Drew(border, "Border thickness"));
    EXPECT_TRUE(Drew(border, "Corner radius"));
}
