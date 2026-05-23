/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Helpers building the generated page-title chrome reticle.
 */

#include "mfd/model/PageDefinition.h"

#include <algorithm>
#include <cmath>

namespace mfd
{
namespace
{
constexpr float kPageTitlePaddingX = 0.0208f;
constexpr float kPageTitlePaddingY = 0.0167f;
constexpr float kPageTitleUnderlineGap = 0.0180f;
constexpr float kMinPageTitleLineWidth = 0.0005f;
constexpr float kDefaultPageTitleLineWidth = 0.0042f;

float SanitizePositiveFinite(const float value, const float fallback) noexcept
{
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}

Vec2 SanitizeScale(const Vec2& scale) noexcept
{
    return {
        SanitizePositiveFinite(std::abs(scale.x), 1.0f),
        SanitizePositiveFinite(std::abs(scale.y), 1.0f)};
}

float EstimatedTextHalfWidth(const std::string_view text, const float fontSize, const float letterSpacing) noexcept
{
    const std::size_t characterCount = std::max<std::size_t>(1U, text.size());
    const float glyphWidth = fontSize * static_cast<float>(characterCount) * 0.30f;
    const float spacingWidth =
        std::max(0.0f, letterSpacing) * static_cast<float>(characterCount > 0U ? characterCount - 1U : 0U) * 0.5f;
    return std::max(0.03f, glyphWidth + spacingWidth);
}

float EstimatedTextHalfHeight(const float fontSize) noexcept
{
    return std::max(0.02f, fontSize * 0.50f);
}
} // namespace

std::string ResolvePageDisplayTitleText(const std::string_view pageName, const std::string_view pageTitle)
{
    const std::string resolvedTitle = pageTitle.empty() ? std::string(pageName) : std::string(pageTitle);
    if (resolvedTitle.empty())
    {
        return "Page";
    }

    if (pageName.empty() || resolvedTitle == pageName)
    {
        return resolvedTitle;
    }

    return resolvedTitle + " [" + std::string(pageName) + "]";
}

ReticleGroup BuildPageTitleDisplayReticle(const std::string_view pageName,
                                          const std::string_view pageTitle,
                                          const PageTitleDisplayDefinition& display)
{
    const std::string resolvedTitle = ResolvePageDisplayTitleText(pageName, pageTitle);
    const float fontSize = kPageTitleFontSize;
    const float letterSpacing = kPageTitleLetterSpacing;
    const float halfWidth = EstimatedTextHalfWidth(resolvedTitle, fontSize, letterSpacing);
    const float halfHeight = EstimatedTextHalfHeight(fontSize);
    const float lineWidth = std::max(kMinPageTitleLineWidth, SanitizePositiveFinite(display.lineWidth, kDefaultPageTitleLineWidth));
    const float totalWidth = halfWidth * 2.0f + kPageTitlePaddingX * 2.0f;
    const float totalHeight = halfHeight * 2.0f + kPageTitlePaddingY * 2.0f;

    ReticleGroup reticle;
    reticle.id = "__page_title__";
    reticle.visible = display.visible;
    reticle.drawOnTop = true;
    reticle.transform = display.transform;
    reticle.transform.scale = SanitizeScale(display.transform.scale);

    Primitive textPrimitive;
    textPrimitive.id = "__page_title_text__";
    textPrimitive.type = PrimitiveType::Text;
    textPrimitive.style.color = display.color;
    textPrimitive.transform.position = {kPageTitlePaddingX + halfWidth, -(kPageTitlePaddingY + halfHeight)};
    textPrimitive.geometry = TextGeometry {resolvedTitle, fontSize, letterSpacing};
    reticle.primitives.push_back(std::move(textPrimitive));

    if (display.decoration == PageTitleDecoration::Underline)
    {
        Primitive underlinePrimitive;
        underlinePrimitive.id = "__page_title_underline__";
        underlinePrimitive.type = PrimitiveType::Line;
        underlinePrimitive.style.color = display.color;
        underlinePrimitive.style.thickness = lineWidth;
        underlinePrimitive.style.lineStyle = display.lineStyle;
        underlinePrimitive.geometry =
            LineGeometry {{0.0f, -(totalHeight + kPageTitleUnderlineGap)}, {totalWidth, -(totalHeight + kPageTitleUnderlineGap)}};
        reticle.primitives.push_back(std::move(underlinePrimitive));
    }
    else if (display.decoration == PageTitleDecoration::Frame)
    {
        Primitive framePrimitive;
        framePrimitive.id = "__page_title_frame__";
        framePrimitive.type = PrimitiveType::Rectangle;
        framePrimitive.style.color = display.color;
        framePrimitive.style.thickness = lineWidth;
        framePrimitive.style.lineStyle = display.lineStyle;
        framePrimitive.transform.position = {totalWidth * 0.5f, -totalHeight * 0.5f};
        framePrimitive.geometry = RectangleGeometry {totalWidth, totalHeight};
        reticle.primitives.push_back(std::move(framePrimitive));
    }

    return reticle;
}
} // namespace mfd
