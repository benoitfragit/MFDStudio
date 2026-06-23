/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Dedicated libFuzzer entry point stressing editor-side JSON serialization.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "EditorDocumentSerializer.h"
#include "FuzzInput.h"
#include "mfd/io/JsonLoader.h"

namespace
{
constexpr std::size_t kMaximumInputSize = 64U * 1024U;
constexpr std::size_t kMaximumGeneratedPoints = 8U;

std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path sourceFile = std::filesystem::path(__FILE__).lexically_normal();
    return sourceFile.parent_path().parent_path().parent_path();
}

std::unique_ptr<mfd::LoadedWindowConfiguration> LoadSeedWindow()
{
    const std::filesystem::path windowFile = RepositoryRoot() / "assets" / "windows" / "demo_pages_minimal.json";
    mfd::JsonLoader loader;

    try
    {
        return std::make_unique<mfd::LoadedWindowConfiguration>(loader.LoadWindowConfiguration(windowFile));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

const mfd::LoadedWindowConfiguration* SeedWindow()
{
    static const std::unique_ptr<mfd::LoadedWindowConfiguration> seed = LoadSeedWindow();
    return seed.get();
}

editor::EditorFileLayout BuildLayout(const mfd::LoadedWindowConfiguration& loaded)
{
    editor::EditorFileLayout layout;
    layout.pageFiles = loaded.window.pageFiles;

    for (const auto& entry : loaded.document.reticleLibrary)
    {
        layout.templateFiles.emplace(entry.first,
                                     editor::DefaultTemplateFilePath(loaded.window.reticleLibraryFolder, entry.first));
    }

    return layout;
}

float NextPossiblyInvalidFloat(mfd::fuzz::FuzzInput& input,
                               const float minimumInclusive,
                               const float maximumInclusive) noexcept
{
    if (input.NextBool())
    {
        return input.NextRawFloat();
    }

    return input.NextFloat(minimumInclusive, maximumInclusive);
}

mfd::Vec2 NextVec2(mfd::fuzz::FuzzInput& input,
                   const float minimumInclusive,
                   const float maximumInclusive) noexcept
{
    return mfd::Vec2 {
        NextPossiblyInvalidFloat(input, minimumInclusive, maximumInclusive),
        NextPossiblyInvalidFloat(input, minimumInclusive, maximumInclusive)};
}

void MutateString(std::string& value, mfd::fuzz::FuzzInput& input, const std::size_t maximumLength)
{
    if (input.NextBool())
    {
        value = input.NextString(maximumLength);
    }
}

void MutateColor(mfd::ColorRgba& value, mfd::fuzz::FuzzInput& input) noexcept
{
    value.r = input.NextByte(value.r);
    value.g = input.NextByte(value.g);
    value.b = input.NextByte(value.b);
    value.a = input.NextByte(value.a);
}

void MutateTransform(mfd::Transform2D& value, mfd::fuzz::FuzzInput& input) noexcept
{
    value.position = NextVec2(input, -4.0f, 4.0f);
    value.rotationDegrees = NextPossiblyInvalidFloat(input, -1080.0f, 1080.0f);
    value.scale = NextVec2(input, -8.0f, 8.0f);
}

void MutatePrimitiveStyle(mfd::PrimitiveStyle& style, mfd::fuzz::FuzzInput& input) noexcept
{
    style.visible = input.NextBool();
    MutateColor(style.color, input);
    style.thickness = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
    style.lineStyle = static_cast<mfd::LineStyle>(input.NextInt(0, 2));
    MutateColor(style.fillColor, input);
    style.filled = input.NextBool();
}

std::vector<mfd::Vec2> BuildPointList(mfd::fuzz::FuzzInput& input)
{
    const std::size_t count = 1U + input.NextIndex(kMaximumGeneratedPoints);
    std::vector<mfd::Vec2> points;
    points.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        points.push_back(NextVec2(input, -4.0f, 4.0f));
    }

    return points;
}

struct GeometryMutator
{
    mfd::fuzz::FuzzInput& input;

    void operator()(mfd::TextGeometry& geometry)
    {
        MutateString(geometry.text, input, 96U);
        geometry.fontSize = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
        geometry.letterSpacing = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
    }

    void operator()(mfd::TimeGeometry& geometry)
    {
        MutateString(geometry.format, input, 32U);
        geometry.utc = input.NextBool();
        geometry.fontSize = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
        geometry.letterSpacing = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
    }

    void operator()(mfd::LineGeometry& geometry) noexcept
    {
        geometry.start = NextVec2(input, -4.0f, 4.0f);
        geometry.end = NextVec2(input, -4.0f, 4.0f);
    }

    void operator()(mfd::CircleGeometry& geometry) noexcept
    {
        geometry.radius = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
    }

    void operator()(mfd::RingGeometry& geometry) noexcept
    {
        geometry.innerRadius = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.outerRadius = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.segments = input.NextInt(-16, 512);
    }

    void operator()(mfd::RectangleGeometry& geometry) noexcept
    {
        geometry.width = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.height = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
    }

    void operator()(mfd::EllipseGeometry& geometry) noexcept
    {
        geometry.width = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.height = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
    }

    void operator()(mfd::SquareGeometry& geometry) noexcept
    {
        // A square is uniform: a single side length drives both dimensions.
        const float side = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.width = side;
        geometry.height = side;
    }

    void operator()(mfd::DiamondGeometry& geometry) noexcept
    {
        geometry.width = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.height = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
    }

    void operator()(mfd::TriangleGeometry& geometry) noexcept
    {
        for (auto& point : geometry.points)
        {
            point = NextVec2(input, -4.0f, 4.0f);
        }
    }

    void operator()(mfd::PolylineGeometry& geometry)
    {
        geometry.points = BuildPointList(input);
        geometry.closed = input.NextBool();
    }

    void operator()(mfd::BezierGeometry& geometry)
    {
        geometry.controlPoints = BuildPointList(input);
        geometry.segments = input.NextInt(-16, 512);
    }

    void operator()(mfd::ArcGeometry& geometry) noexcept
    {
        geometry.radius = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.startAngleDegrees = NextPossiblyInvalidFloat(input, -1080.0f, 1080.0f);
        geometry.endAngleDegrees = NextPossiblyInvalidFloat(input, -1080.0f, 1080.0f);
        geometry.segments = input.NextInt(-16, 512);
    }

    void operator()(mfd::ImageGeometry& geometry)
    {
        geometry.file = std::filesystem::path(input.NextString(64U));
        geometry.width = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
        geometry.height = NextPossiblyInvalidFloat(input, -4.0f, 4.0f);
    }
};

void MutatePrimitive(mfd::Primitive& primitive, mfd::fuzz::FuzzInput& input)
{
    MutateString(primitive.id, input, 64U);
    primitive.type = static_cast<mfd::PrimitiveType>(input.NextInt(0, 13));
    MutateTransform(primitive.transform, input);
    MutatePrimitiveStyle(primitive.style, input);
    primitive.exposed = input.NextBool();
    primitive.reticleRotationSensitive = input.NextBool();
    primitive.reticleScaleSensitive = input.NextBool();

    std::visit(GeometryMutator {input}, primitive.geometry);
}

void MutateReticle(mfd::ReticleGroup& reticle, mfd::fuzz::FuzzInput& input)
{
    MutateString(reticle.id, input, 64U);
    MutateString(reticle.sourceTemplateId, input, 64U);
    MutateString(reticle.layerId, input, 64U);
    MutateString(reticle.info.label, input, 64U);
    MutateString(reticle.info.category, input, 64U);
    MutateString(reticle.blink.typeName, input, 64U);
    MutateString(reticle.blink.normalizedTypeName, input, 64U);
    reticle.blink.enabled = input.NextBool();
    reticle.blink.durationMs = static_cast<std::uint32_t>(input.NextInt(0, 100000));
    reticle.visible = input.NextBool();
    reticle.drawOnTop = input.NextBool();
    MutateTransform(reticle.transform, input);
    reticle.clipping.mode = static_cast<mfd::ReticleClipMode>(input.NextInt(0, 2));
    MutateString(reticle.clipping.primitiveId, input, 64U);

    if (!reticle.primitives.empty())
    {
        const std::size_t primitiveCount = 1U + input.NextIndex(reticle.primitives.size());
        for (std::size_t index = 0; index < primitiveCount; ++index)
        {
            MutatePrimitive(reticle.primitives[input.NextIndex(reticle.primitives.size())], input);
        }
    }
}

void MutatePage(mfd::PageDefinition& page, mfd::fuzz::FuzzInput& input)
{
    MutateString(page.name, input, 64U);
    MutateString(page.normalizedName, input, 64U);
    MutateString(page.title, input, 96U);
    page.defaultPage = input.NextBool();
    MutateColor(page.backgroundColor, input);
    page.view.center = NextVec2(input, -4.0f, 4.0f);
    page.view.zoom = NextPossiblyInvalidFloat(input, -16.0f, 16.0f);
    page.titleDisplay.visible = input.NextBool();
    MutateTransform(page.titleDisplay.transform, input);
    MutateColor(page.titleDisplay.color, input);
    page.titleDisplay.lineWidth = NextPossiblyInvalidFloat(input, -2.0f, 2.0f);
    page.titleDisplay.lineStyle = static_cast<mfd::LineStyle>(input.NextInt(0, 2));
    page.titleDisplay.decoration = static_cast<mfd::PageTitleDecoration>(input.NextInt(0, 2));
    MutateString(page.defaultBlinkTypeName, input, 64U);
    MutateString(page.normalizedDefaultBlinkTypeName, input, 64U);
    MutateString(page.activeStrobeName, input, 64U);
    MutateString(page.normalizedActiveStrobeName, input, 64U);

    if (!page.staticReticles.empty())
    {
        const std::size_t reticleCount = 1U + input.NextIndex(page.staticReticles.size());
        for (std::size_t index = 0; index < reticleCount; ++index)
        {
            MutateReticle(page.staticReticles[input.NextIndex(page.staticReticles.size())], input);
        }
    }
}

void MutateWindow(mfd::WindowAssetDefinition& window, mfd::fuzz::FuzzInput& input)
{
    MutateString(window.title, input, 96U);
    window.width = input.NextInt(-32768, 32768);
    window.height = input.NextInt(-32768, 32768);
    window.positionX = input.NextInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    window.positionY = input.NextInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    window.targetFps = input.NextInt(-1000, 1000);
    window.feedbackFastIntervalSeconds = NextPossiblyInvalidFloat(input, -10.0f, 10.0f);
    window.feedbackHeartbeatIntervalSeconds = NextPossiblyInvalidFloat(input, -10.0f, 10.0f);
}

void ParseSerializedJson(const std::string& text)
{
    nlohmann::json::parse(text);
}

void RunSerializers(mfd::LoadedWindowConfiguration& loaded)
{
    const editor::EditorFileLayout layout = BuildLayout(loaded);

    ParseSerializedJson(editor::SerializeWindowToJsonString(loaded.window, loaded.document, layout));

    for (std::size_t pageIndex = 0; pageIndex < loaded.document.pages.size(); ++pageIndex)
    {
        const mfd::PageDefinition& page = loaded.document.pages[pageIndex];
        ParseSerializedJson(editor::SerializePageToJsonString(page, loaded.document.reticleLibrary, layout, pageIndex));

        for (const auto& reticle : page.staticReticles)
        {
            ParseSerializedJson(
                editor::SerializePageReticleToJsonString(reticle, loaded.document.reticleLibrary));
        }
    }

    for (const auto& entry : loaded.document.reticleLibrary)
    {
        ParseSerializedJson(editor::SerializeReticleTemplateToJsonString(entry.second));
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (data == nullptr || size > kMaximumInputSize)
    {
        return 0;
    }

    const mfd::LoadedWindowConfiguration* const seed = SeedWindow();
    if (seed == nullptr)
    {
        return 0;
    }

    mfd::LoadedWindowConfiguration loaded = *seed;
    mfd::fuzz::FuzzInput input(data, size);

    MutateWindow(loaded.window, input);
    if (!loaded.document.pages.empty())
    {
        const std::size_t pageCount = 1U + input.NextIndex(loaded.document.pages.size());
        for (std::size_t index = 0; index < pageCount; ++index)
        {
            MutatePage(loaded.document.pages[input.NextIndex(loaded.document.pages.size())], input);
        }
    }

    if (!loaded.document.reticleLibrary.empty())
    {
        auto iterator = loaded.document.reticleLibrary.begin();
        std::advance(iterator, static_cast<std::ptrdiff_t>(input.NextIndex(loaded.document.reticleLibrary.size())));
        MutateReticle(iterator->second, input);
    }

    RunSerializers(loaded);
    return 0;
}
