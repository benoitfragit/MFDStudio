/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Public document, page, blink and strobe definition types loaded from JSON.
 */

#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/model/PageName.h"
#include "mfd/model/Reticle.h"

namespace mfd
{
/**
 * @brief Capture shapes supported by a page strobe.
 */
enum class StrobeCaptureShape
{
    Circle,
    Rectangle
};

/**
 * @brief Visual cue shape optionally applied to a strobe while it is magnetized.
 */
enum class StrobeMagnetVisualShape
{
    Circle,
    Square
};

/**
 * @brief Capture parameters associated with a strobe reticle.
 */
struct StrobeCaptureConfig
{
    /** @brief Capture area shape. */
    StrobeCaptureShape shape = StrobeCaptureShape::Circle;
    /** @brief Circle radius when the capture shape is circular. */
    float radius = 0.0875f;
    /** @brief Rectangle size when the capture shape is rectangular. */
    Vec2 size {0.175f, 0.175f};
};

/**
 * @brief Magnetization parameters associated with a strobe reticle.
 */
struct StrobeMagnetConfig
{
    /** @brief Enables or disables strobe magnetization. */
    bool enabled = false;
    /** @brief Attraction radius in logical coordinates. */
    float radius = 0.075f;
    /** @brief Blend factor applied when the strobe is attracted to a target. */
    float strength = 1.0f;
    /** @brief Enables an optional visual shape override while a target is magnetized. */
    bool visualShapeEnabled = false;
    /** @brief Visual shape applied while `visualShapeEnabled` and magnetization are active. */
    StrobeMagnetVisualShape visualShape = StrobeMagnetVisualShape::Circle;
    /** @brief Logical size of the magnetized visual shape. */
    float visualShapeSize = 0.12f;
};

/**
 * @brief Optional strobe definition attached to a page.
 */
struct PageStrobeDefinition
{
    /** @brief Reticle used to render the strobe cursor itself. */
    ReticleGroup reticle;
    /** @brief Capture parameters applied around the strobe cursor. */
    StrobeCaptureConfig capture;
    /** @brief Optional magnetization parameters applied around the strobe cursor. */
    StrobeMagnetConfig magnet;
};

/**
 * @brief Named blink type declared by one page.
 */
struct PageBlinkDefinition
{
    /** @brief User-facing blink type name used by the API and JSON. */
    std::string name;
    /** @brief Normalized name used internally for fast lookup. */
    std::string normalizedName;
    /** @brief Effective blink period in milliseconds. */
    std::uint32_t durationMs = 1000;
};

/**
 * @brief Editor-only layer definition attached to one page.
 *
 * @note Layers are used exclusively by the editor UI to group page reticles and
 * temporarily hide them while authoring. Runtime rendering ignores them.
 */
struct EditorLayerDefinition
{
    /** @brief User-facing layer id referenced by page reticles in editor mode. */
    std::string id;
    /** @brief Indicates whether the layer is currently visible in the editor. */
    bool visible = true;
};

/**
 * @brief Editor-only page metadata persisted by the authoring tool.
 */
struct PageEditorState
{
    /** @brief Ordered list of editor layers available on the page. */
    std::vector<EditorLayerDefinition> layers;
    /**
     * @brief Optional explicit list of shared reticle templates exposed as generated dynamic sets for this page.
     *
     * @note `std::nullopt` means the page does not opt into generated dynamic
     * sets yet. An empty vector explicitly keeps the generated dynamic API
     * empty for this page as well.
     */
    std::optional<std::vector<std::string>> dynamicReticleTemplateIds;
};

/**
 * @brief Full definition of a renderable MFD page.
 */
struct PageDefinition
{
    /** @brief Display name exposed to user code. */
    std::string name;
    /** @brief Normalized name used internally for fast lookup. */
    std::string normalizedName;
    /** @brief Human-readable title shown by tools and examples. */
    std::string title;
    /** @brief Marks the page selected by default when its parent window first loads. */
    bool defaultPage = false;
    /** @brief Page background color. */
    ColorRgba backgroundColor {6, 14, 20, 255};
    /** @brief Initial view center and zoom for the page. */
    PageViewState view {};
    /** @brief Named blink types declared by this page. */
    std::vector<PageBlinkDefinition> blinkTypes;
    /** @brief Optional default blink type used when a reticle enables blinking without naming a type. */
    std::string defaultBlinkTypeName;
    /** @brief Normalized default blink type used internally for fast lookup. */
    std::string normalizedDefaultBlinkTypeName;
    /** @brief Editor-only page metadata ignored by the runtime. */
    PageEditorState editor {};
    /** @brief Static reticles drawn whenever the page is active. */
    std::vector<ReticleGroup> staticReticles;
    /** @brief Optional strobe definition for pages that support capture. */
    std::optional<PageStrobeDefinition> strobe;
};

/**
 * @brief Entire document loaded from a window/pages JSON configuration.
 */
struct MfdDocument
{
    /** @brief Source JSON used to load the document. */
    std::filesystem::path sourceFile;
    /** @brief Folder containing reusable reticle templates. */
    std::filesystem::path reticleLibraryFolder;
    /** @brief Reusable reticle templates indexed by template id. */
    ReticleLibrary reticleLibrary;
    /** @brief Pages available in the document. */
    std::vector<PageDefinition> pages;
};

/**
 * @brief Finds a page definition by name.
 * @param document Document to search.
 * @param pageName Page name in user-facing or normalized form.
 * @return Pointer to the matching page, or `nullptr` if the page does not exist.
 */
inline const PageDefinition* FindPageDefinition(const MfdDocument& document, const std::string_view pageName)
{
    const std::string normalizedPageName = NormalizePageName(pageName);

    for (const auto& page : document.pages)
    {
        if (page.normalizedName == normalizedPageName)
        {
            return &page;
        }
    }

    return nullptr;
}

/**
 * @brief Finds one blink type definition inside a page.
 * @param page Page to inspect.
 * @param blinkTypeName Blink type name in user-facing or normalized form.
 * @return Pointer to the matching blink type, or `nullptr` if it does not exist.
 */
inline const PageBlinkDefinition* FindPageBlinkDefinition(const PageDefinition& page,
                                                          const std::string_view blinkTypeName)
{
    const std::string normalizedBlinkTypeName = NormalizePageName(blinkTypeName);

    for (const auto& blinkType : page.blinkTypes)
    {
        if (blinkType.normalizedName == normalizedBlinkTypeName)
        {
            return &blinkType;
        }
    }

    return nullptr;
}
} // namespace mfd
