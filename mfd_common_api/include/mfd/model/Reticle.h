/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Public reticle and primitive types used by the MFD runtime and tools.
 */

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/model/Types.h"

namespace mfd
{
/**
 * @brief Default logical text size used by newly created text primitives.
 */
inline constexpr float kDefaultTextFontSize = 0.0417f;

/**
 * @brief Default logical spacing between consecutive text characters.
 */
inline constexpr float kDefaultTextLetterSpacing = kDefaultTextFontSize * 0.05f;

/**
 * @brief Primitive kinds supported by the reticle model.
 */
enum class PrimitiveType
{
    Text,
    Time,
    Line,
    Circle,
    Ring,
    Rectangle,
    Ellipse,
    Square,
    Diamond,
    Triangle,
    Polyline,
    Bezier,
    Arc,
    Image
};

/**
 * @brief Shared visual style applied to a primitive before reticle-level overrides.
 */
struct PrimitiveStyle
{
    /** @brief Enables or disables the primitive visibility. */
    bool visible = true;
    /** @brief Stroke color used by the primitive outline. */
    ColorRgba color {0, 255, 102, 255};
    /** @brief Stroke thickness expressed in logical units. */
    float thickness = 0.0042f;
    /** @brief Stroke pattern used by the primitive outline. */
    LineStyle lineStyle = LineStyle::Solid;
    /** @brief Fill color used when the primitive supports filling. */
    ColorRgba fillColor {0, 0, 0, 0};
    /** @brief Enables filled rendering when the primitive supports it. */
    bool filled = false;
};

/**
 * @brief Optional style overrides applied at reticle level on top of primitive styles.
 */
struct ReticleStyleOverride
{
    std::optional<ColorRgba> color;
    std::optional<float> thickness;
    std::optional<ColorRgba> fillColor;
    std::optional<bool> filled;
};

/**
 * @brief Geometry payload for a text primitive.
 */
struct TextGeometry
{
    std::string text;
    float fontSize = kDefaultTextFontSize;
    float letterSpacing = kDefaultTextLetterSpacing;
};

/**
 * @brief Geometry payload for a clock/time text primitive.
 */
struct TimeGeometry
{
    std::string format = "%H:%M:%S";
    bool utc = false;
    float fontSize = kDefaultTextFontSize;
    float letterSpacing = kDefaultTextLetterSpacing;
};

/**
 * @brief Geometry payload for a line primitive.
 */
struct LineGeometry
{
    Vec2 start {-0.0208f, 0.0f};
    Vec2 end {0.0208f, 0.0f};
};

/**
 * @brief Geometry payload for a circle primitive.
 */
struct CircleGeometry
{
    float radius = 0.0208f;
};

/**
 * @brief Geometry payload for an annulus / ring primitive.
 */
struct RingGeometry
{
    float innerRadius = 0.0167f;
    float outerRadius = 0.0208f;
    int segments = 64;
};

/**
 * @brief Geometry payload for an axis-aligned rectangle primitive.
 */
struct RectangleGeometry
{
    float width = 0.0417f;
    float height = 0.0208f;
};

/**
 * @brief Geometry payload for an axis-aligned ellipse primitive.
 */
struct EllipseGeometry
{
    float width = 0.0417f;
    float height = 0.0208f;
};

/**
 * @brief Geometry payload for a square primitive.
 */
struct SquareGeometry
{
    float width = 0.0208f;
    float height = 0.0208f;
};

/**
 * @brief Geometry payload for a diamond primitive.
 */
struct DiamondGeometry
{
    float width = 0.0208f;
    float height = 0.0208f;
};

/**
 * @brief Geometry payload for a triangle primitive.
 */
struct TriangleGeometry
{
    std::array<Vec2, 3> points {};
};

/**
 * @brief Geometry payload for a polyline primitive.
 */
struct PolylineGeometry
{
    std::vector<Vec2> points;
    bool closed = false;
};

/**
 * @brief Geometry payload for a bezier primitive.
 */
struct BezierGeometry
{
    std::vector<Vec2> controlPoints;
    int segments = 32;
};

/**
 * @brief Geometry payload for a circular arc primitive.
 */
struct ArcGeometry
{
    float radius = 0.0208f;
    float startAngleDegrees = 0.0f;
    float endAngleDegrees = 180.0f;
    int segments = 48;
};

/**
 * @brief Geometry payload for a raster image primitive.
 */
struct ImageGeometry
{
    std::filesystem::path file;
    float width = 0.2000f;
    float height = 0.2000f;
};

/**
 * @brief Variant storing the geometry data of a primitive.
 */
using PrimitiveGeometry = std::variant<TextGeometry,
                                       TimeGeometry,
                                       LineGeometry,
                                       CircleGeometry,
                                       RingGeometry,
                                       RectangleGeometry,
                                       EllipseGeometry,
                                       SquareGeometry,
                                       DiamondGeometry,
                                       TriangleGeometry,
                                       PolylineGeometry,
                                       BezierGeometry,
                                       ArcGeometry,
                                       ImageGeometry>;

/**
 * @brief Arbitrary key/value metadata attached to a reticle.
 */
using ReticleMetadata = std::unordered_map<std::string, std::string>;

/**
 * @brief User-facing descriptive information attached to a reticle.
 */
struct ReticleInfo
{
    std::string label;
    std::string category;
    ReticleMetadata metadata;
};

/**
 * @brief Optional page-level blink assignment resolved for one reticle instance.
 *
 * @note Reticle templates usually leave this disabled. Page instances and
 * runtime patches can enable it and optionally point to one named page blink
 * type.
 */
struct ReticleBlinkState
{
    /** @brief Enables or disables blinking for this reticle. */
    bool enabled = false;
    /** @brief Optional user-facing blink type name requested for this reticle. */
    std::string typeName;
    /** @brief Normalized blink type name used for fast runtime lookup. */
    std::string normalizedTypeName;
    /** @brief Resolved effective blink duration in milliseconds. */
    std::uint32_t durationMs = 0;
};

/**
 * @brief Reticle masking mode applied against the page content already drawn underneath one reticle instance.
 *
 * @note `Inner` erases the inside of the mask shape toward the page background,
 * while `Outer` erases everything outside of the mask shape.
 */
enum class ReticleClipMode
{
    None,
    Inner,
    Outer
};

/**
 * @brief Optional clipping state applied locally to one reticle.
 */
struct ReticleClipState
{
    ReticleClipMode mode = ReticleClipMode::None;
    std::string primitiveId;
};

/**
 * @brief Single drawable element inside a reticle.
 */
struct Primitive
{
    std::string id;
    PrimitiveType type = PrimitiveType::Line;
    Transform2D transform {};
    PrimitiveStyle style {};
    PrimitiveGeometry geometry {};
    /** @brief Exposes this primitive to the generated client API when enabled. */
    bool exposed = false;
    /**
     * @brief Applies page-reticle or page-strobe parent rotation to this primitive when enabled.
     *
     * @note Explicit primitive rotation still applies even when this flag is `false`.
     */
    bool reticleRotationSensitive = true;
    /**
     * @brief Applies page-reticle or page-strobe parent scale to this primitive when enabled.
     *
     * @note Explicit primitive scale still applies even when this flag is `false`.
     */
    bool reticleScaleSensitive = true;
};

/**
 * @brief Reticle instance or template composed of multiple primitives.
 */
struct ReticleGroup
{
    std::string id;
    std::string sourceTemplateId;
    /** @brief Runtime page layer id used to compute the draw order of page-owned reticles. */
    std::string layerId;
    ReticleInfo info;
    /** @brief Optional page-level blink assignment attached to this instance. */
    ReticleBlinkState blink;
    bool visible = true;
    /** @brief Draws this reticle after regular page reticles when enabled. */
    bool drawOnTop = false;
    Transform2D transform {};
    ReticleStyleOverride overrides {};
    /** @brief Optional local clipping state resolved against one primitive id. */
    ReticleClipState clipping {};
    std::vector<Primitive> primitives;
};

/**
 * @brief Template library indexed by reticle id.
 */
using ReticleLibrary = std::unordered_map<std::string, ReticleGroup>;

/**
 * @brief Merges a primitive style with reticle-level overrides.
 * @param base Base primitive style.
 * @param overrides Optional reticle-level overrides.
 * @return Effective style used for rendering.
 */
MFD_API PrimitiveStyle MergeStyle(const PrimitiveStyle& base, const ReticleStyleOverride& overrides) noexcept;

/**
 * @brief Merges two reticle override sets.
 * @param base Base override set.
 * @param overrides Additional overrides to apply.
 * @return Merged override set.
 */
MFD_API ReticleStyleOverride MergeOverrides(const ReticleStyleOverride& base, const ReticleStyleOverride& overrides) noexcept;

/**
 * @brief Instantiates a reticle template into a runtime reticle instance.
 * @param templ Template reticle.
 * @param instanceId Public id assigned to the new instance.
 * @param transform Additional transform applied to the template transform.
 * @param overrides Additional style overrides applied on top of the template overrides.
 * @note The returned instance leaves `layerId` empty. Page-local layer
 * attachment must be assigned explicitly by the page authoring model or the
 * runtime dynamic binding that owns the instance.
 * @return New reticle instance ready to be inserted in a page or in the runtime scene.
 */
MFD_API ReticleGroup InstantiateReticle(const ReticleGroup& templ,
                                        std::string instanceId,
                                        Transform2D transform = {},
                                        ReticleStyleOverride overrides = {});

/**
 * @brief Finds a primitive by id inside a reticle.
 * @param reticle Reticle to inspect.
 * @param primitiveId Primitive id to search.
 * @return Mutable pointer to the primitive, or `nullptr` if it does not exist.
 */
MFD_API Primitive* FindPrimitive(ReticleGroup& reticle, std::string_view primitiveId) noexcept;

/**
 * @brief Finds a primitive by id inside a const reticle.
 * @param reticle Reticle to inspect.
 * @param primitiveId Primitive id to search.
 * @return Pointer to the primitive, or `nullptr` if it does not exist.
 */
MFD_API const Primitive* FindPrimitive(const ReticleGroup& reticle, std::string_view primitiveId) noexcept;

/**
 * @brief Resolves the effective world transform applied to one primitive inside a reticle.
 * @param primitive Primitive to evaluate.
 * @param reticle Owning reticle or strobe instance.
 * @return World transform applied to the primitive geometry.
 *
 * @note The primitive anchor position always follows the full parent reticle
 * transform. Parent rotation and scale are propagated to the primitive
 * geometry only when the corresponding sensitivity flags stay enabled.
 */
MFD_API Transform2D ResolvePrimitiveWorldTransform(const Primitive& primitive, const ReticleGroup& reticle) noexcept;

/**
 * @brief Applies the effective reticle-aware world transform to one primitive-local point.
 * @param point Point expressed in primitive-local geometry coordinates.
 * @param primitive Primitive to evaluate.
 * @param reticle Owning reticle or strobe instance.
 * @return Point expressed in page logical coordinates.
 */
MFD_API Vec2 ApplyPrimitiveWorldTransform(const Vec2& point,
                                          const Primitive& primitive,
                                          const ReticleGroup& reticle) noexcept;

/**
 * @brief Returns the effective average geometry scale applied to one primitive inside a reticle.
 * @param primitive Primitive to evaluate.
 * @param reticle Owning reticle or strobe instance.
 * @return Average absolute scale propagated to the primitive geometry.
 */
MFD_API float PrimitiveAverageScale(const Primitive& primitive, const ReticleGroup& reticle) noexcept;

/**
 * @brief Returns whether a primitive can be used as a convex reticle clip mask.
 * @param primitive Primitive to inspect.
 * @return `true` for supported convex shapes such as circle, rectangle,
 * ellipse, square or triangle.
 */
MFD_API bool SupportsReticleClipPrimitive(const Primitive& primitive) noexcept;

/**
 * @brief Returns whether a primitive can currently render one filled area.
 * @param primitive Primitive to inspect.
 * @return `true` for fill-capable shapes such as circle, ring, rectangle,
 * ellipse, square, diamond, triangle, arc and closed polylines.
 *
 * @note Open polylines, text-like primitives, lines, bezier curves and images
 * never expose filled rendering.
 */
MFD_API bool SupportsFilledPrimitive(const Primitive& primitive) noexcept;

/**
 * @brief Resolves the effective clip primitive referenced by a reticle.
 * @param reticle Reticle to inspect.
 * @return Mutable pointer to the supported clip primitive, or `nullptr` when
 * clipping is disabled or invalid.
 */
MFD_API Primitive* ResolveClipPrimitive(ReticleGroup& reticle) noexcept;

/**
 * @brief Resolves the effective clip primitive referenced by a const reticle.
 * @param reticle Reticle to inspect.
 * @return Pointer to the supported clip primitive, or `nullptr` when clipping
 * is disabled or invalid.
 */
MFD_API const Primitive* ResolveClipPrimitive(const ReticleGroup& reticle) noexcept;

/**
 * @brief Updates the text value of a named text primitive.
 * @param reticle Reticle containing the text primitive.
 * @param primitiveId Target primitive id.
 * @param value New text value.
 * @return `true` when the primitive exists and is a text primitive.
 */
MFD_API bool SetTextPrimitive(ReticleGroup& reticle, std::string_view primitiveId, std::string value);

/**
 * @brief Updates the character spacing of a named text primitive.
 * @param reticle Reticle containing the text primitive.
 * @param primitiveId Target primitive id.
 * @param letterSpacing New logical spacing value.
 * @return `true` when the primitive exists and is a text primitive.
 */
MFD_API bool SetTextPrimitiveLetterSpacing(ReticleGroup& reticle,
                                           std::string_view primitiveId,
                                           float letterSpacing) noexcept;
} // namespace mfd
