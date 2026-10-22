/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief API LEVEL 1 container document and resolved-layout types.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mfd/model/Types.h"

namespace mfd::api1
{
/** @brief Hard document limit protecting recursive layout and editor operations. */
inline constexpr std::size_t kMaximumContainerCount = 4096U;
/** @brief Hard hierarchy-depth limit protecting recursive layout and malformed inputs. */
inline constexpr std::size_t kMaximumContainerDepth = 64U;

/** @brief Container layout strategy. Numeric values are persisted and must remain stable. */
enum class ContainerType : std::uint32_t
{
    Canvas = 0U,
    Linear = 1U,
    Grid = 2U,
    Overlay = 3U
};

/** @brief Determines which authored container dimensions follow nested child content. */
enum class ContainerSizeMode : std::uint32_t
{
    Fixed = 0U,
    FitContent = 1U,
    FitHorizontal = 2U,
    FitVertical = 3U
};

/** @brief Main-axis direction used by a LinearBox. */
enum class LinearOrientation : std::uint32_t
{
    Horizontal = 0U,
    Vertical = 1U
};

/** @brief Cross-axis or cell alignment applied by layout containers. */
enum class ContainerAlignment : std::uint32_t
{
    Start = 0U,
    Center = 1U,
    End = 2U,
    Stretch = 3U
};

/** @brief Resize behavior of one child in CanvasBox or OverlayBox. */
enum class ChildResizeMode : std::uint32_t
{
    Fixed = 0U,
    Proportional = 1U,
    Stretch = 2U
};

/** @brief Scrollbar visibility policy for one axis. */
enum class ScrollMode : std::uint32_t
{
    Disabled = 0U,
    Automatic = 1U,
    AlwaysVisible = 2U
};

/** @brief Finite axis-aligned rectangle expressed in page coordinates. */
struct ContainerRect
{
    /** @brief Rectangle center. */
    Vec2 center {};
    /** @brief Non-negative width and height. */
    Vec2 size {};
};

/** @brief Insets measured inward from the corresponding rectangle edges. */
struct ContainerInsets
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

/** @brief Optional background and border rendered by a container. */
struct ContainerAppearance
{
    bool backgroundVisible = false;
    ColorRgba backgroundColor {0U, 0U, 0U, 0U};
    bool borderVisible = false;
    ColorRgba borderColor {128U, 192U, 192U, 255U};
    float borderThickness = 0.003f;
    float cornerRadius = 0.0f;
};

/** @brief Runtime scrollbar appearance shared by both axes. */
struct ScrollbarAppearance
{
    ColorRgba trackColor {20U, 34U, 40U, 180U};
    ColorRgba thumbColor {112U, 176U, 180U, 220U};
    ColorRgba thumbHoverColor {142U, 210U, 214U, 240U};
    ColorRgba thumbActiveColor {180U, 236U, 240U, 255U};
    float thickness = 0.025f;
    float cornerRadius = 0.006f;
    float minimumThumbSize = 0.05f;
    ContainerInsets margins {};
};

/** @brief Independent horizontal and vertical scrolling configuration. */
struct ContainerScrolling
{
    ScrollMode horizontal = ScrollMode::Disabled;
    ScrollMode vertical = ScrollMode::Disabled;
    Vec2 offset {};
    bool clipContent = true;
    ScrollbarAppearance appearance {};
};

/**
 * @brief Stable ownership and placement record for one child container.
 *
 * @note `childId` is the single ownership edge. Geometry remains stored on the
 * child itself; grid coordinates and resize behavior belong to this edge so
 * future graphical child nodes can reuse the same placement contract.
 */
enum class ContainerChildKind : std::uint32_t { Container = 0, Widget = 1 };

struct ContainerChildPlacement
{
    std::string childId;
    ChildResizeMode resizeMode = ChildResizeMode::Fixed;
    ContainerAlignment horizontalAlignment = ContainerAlignment::Center;
    ContainerAlignment verticalAlignment = ContainerAlignment::Center;
    std::uint32_t row = 0U;
    std::uint32_t column = 0U;
    std::uint32_t rowSpan = 1U;
    std::uint32_t columnSpan = 1U;
    int renderOrder = 0;
    ContainerChildKind kind = ContainerChildKind::Container;
};

/** @brief Complete authored definition of one page-local container. */
struct ContainerDefinition
{
    std::string id;
    std::string name;
    ContainerType type = ContainerType::Canvas;
    ContainerSizeMode sizeMode = ContainerSizeMode::Fixed;
    ContainerRect geometry {{0.0f, 0.0f}, {0.5f, 0.5f}};
    Vec2 minimumSize {0.02f, 0.02f};
    Vec2 layoutReferenceSize {0.5f, 0.5f};
    bool visible = true;
    bool enabled = true;
    bool exposed = false;
    ContainerInsets padding {};
    float spacing = 0.0f;
    LinearOrientation orientation = LinearOrientation::Horizontal;
    ContainerAlignment alignment = ContainerAlignment::Start;
    std::uint32_t rows = 1U;
    std::uint32_t columns = 1U;
    ContainerAppearance appearance {};
    ContainerScrolling scrolling {};
    std::vector<ContainerChildPlacement> children;
};

/** @brief Resolved scrollbar track and thumb rectangles for one axis. */
struct ScrollbarGeometry
{
    bool visible = false;
    ContainerRect track {};
    ContainerRect thumb {};
};

/** @brief Deterministic output of recursive layout for one authored container. */
struct ContainerLayoutNode
{
    /** @brief Index of the authored definition in the caller-owned catalog. */
    std::size_t sourceIndex = 0U;
    /** @brief Parent source index, or `size_t` maximum for a root. */
    std::size_t parentSourceIndex = static_cast<std::size_t>(-1);
    std::string id;
    std::string parentId;
    ContainerRect bounds {};
    ContainerRect viewport {};
    Vec2 contentSize {};
    Vec2 scrollOffset {};
    bool effectiveVisible = true;
    bool effectiveEnabled = true;
    std::size_t depth = 0U;
    int renderOrder = 0;
    ScrollbarGeometry horizontalScrollbar {};
    ScrollbarGeometry verticalScrollbar {};
};
} // namespace mfd::api1
