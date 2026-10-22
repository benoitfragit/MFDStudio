#pragma once
/** @file @brief Versioned API LEVEL 1 widget assets, instances and confirmed state. */
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "mfd/apilevel1/ContainerTypes.h"
#include "mfd/model/Reticle.h"

namespace mfd::api1
{
inline constexpr std::size_t kMaximumWidgetCount = 4096U;
inline constexpr std::size_t kMaximumWidgetTextBytes = 4096U;
/** @brief Persisted semantic types; append values without renumbering. */
enum class WidgetType : std::uint32_t
{
    Button = 0, Toggle = 1, Checkbox = 2, Radio = 3, Slider = 4, Knob = 5,
    Gauge = 6, ProgressBar = 7, Indicator = 8, NumericDisplay = 9, Label = 10
};
enum class WidgetVisualState : std::uint32_t
{
    Normal = 0, Hover = 1, Pressed = 2, Checked = 3, Disabled = 4, Warning = 5, Pending = 6
};
enum class WidgetBindingTarget : std::uint32_t
{
    Text = 0, Rotation = 1, ScaleX = 2, ScaleY = 3, PositionX = 4, PositionY = 5, Visible = 6
};
struct WidgetValue
{
    bool checked = false;
    bool warning = false;
    double value = 0.0;
    std::string text;
};
struct WidgetRange
{
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.0;
    std::uint32_t decimals = 2U;
};
struct WidgetBinding
{
    std::string primitiveId;
    WidgetBindingTarget target = WidgetBindingTarget::Text;
    double from = 0.0;
    double to = 1.0;
};
struct WidgetVisual
{
    WidgetVisualState state = WidgetVisualState::Normal;
    ReticleGroup composition;
    std::vector<WidgetBinding> bindings;
};
/** @brief Immutable shared definition. Editing replaces it for every referencing instance. */
struct WidgetDefinition
{
    std::filesystem::path sourceFile;
    std::uint32_t schemaVersion = 1U;
    std::string id;
    WidgetType type = WidgetType::Button;
    Vec2 preferredSize {0.3f, 0.12f};
    /** @brief Hit rectangle in authored visual coordinates, independent of painted primitives. */
    ContainerRect hitRegion {{0.0f, 0.0f}, {0.3f, 0.12f}};
    WidgetRange range;
    WidgetValue defaults;
    std::vector<WidgetVisual> visuals;
};
struct WidgetOverrides
{
    std::optional<bool> checked;
    std::optional<bool> warning;
    std::optional<double> value;
    std::optional<std::string> text;
};
struct WidgetInstance
{
    std::string id;
    std::string definitionFile;
    std::shared_ptr<const WidgetDefinition> definition;
    Vec2 position {};
    std::optional<Vec2> size;
    bool visible = true;
    bool enabled = true;
    bool exposed = false;
    WidgetOverrides overrides;
};
struct WidgetState
{
    WidgetValue confirmed;
    bool visible = true;
    bool enabled = true;
    bool hovered = false;
    bool pressed = false;
    bool pending = false;
    std::uint64_t requestId = 0U;
};
} // namespace mfd::api1
