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
    /** @brief User-facing strobe name used by the editor and generated APIs. */
    std::string name;
    /** @brief Normalized strobe name used internally for fast lookup. */
    std::string normalizedName;
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
 * @brief Default runtime page layer id injected for legacy or underspecified content.
 */
inline constexpr std::string_view kDefaultPageLayerId = "default";

/**
 * @brief Runtime layer definition owned by one page.
 *
 * @note The order of `PageDefinition::layers` is the source of truth for the
 * main inter-layer render order.
 */
struct PageLayerDefinition
{
    /** @brief Stable page-local layer identifier referenced by static reticles and dynamic bindings. */
    std::string id;
    /**
     * @brief Restricts the reach of clipping reticles drawn in this layer to the layer itself.
     *
     * When `false` (the default), a clipping reticle keeps the historical global behaviour and may
     * erase everything already drawn behind it, including lower layers. When `true`, the clipping
     * only erases content already drawn inside this same layer; pixels belonging to strictly earlier
     * layers in the render order stay visible.
     */
    bool clippingEraseLayerOnly = false;
};

/**
 * @brief Runtime binding associating one dynamic reticle template to one page layer.
 */
struct DynamicReticleLayerBinding
{
    /** @brief Shared reticle template id authorized on the page. */
    std::string templateId;
    /** @brief Runtime page layer id assigned to instances created from the template. */
    std::string layerId;
    /** @brief Stable intra-layer ordering key used to sort dynamic templates inside one layer. */
    int orderInLayer = 0;
};

/**
 * @brief Editor-only layer presentation state attached to one runtime page layer.
 *
 * @note This state never defines runtime layers by itself. It only stores UI
 * metadata keyed by one `PageLayerDefinition::id`.
 */
struct EditorLayerDefinition
{
    /** @brief Runtime page layer id targeted by this editor presentation state. */
    std::string id;
    /** @brief Indicates whether the layer is currently visible in the editor. */
    bool visible = true;
};

/**
 * @brief Editor-only page metadata persisted by the authoring tool.
 */
struct PageEditorState
{
    /** @brief Optional editor-only presentation states keyed by runtime page layer ids. */
    std::vector<EditorLayerDefinition> layers;
};

/**
 * @brief Visual decoration applied around the page title chrome.
 */
enum class PageTitleDecoration
{
    /** @brief Renders only the title text. */
    None,
    /** @brief Draws one underline below the title text. */
    Underline,
    /** @brief Draws one rectangular frame around the title text. */
    Frame
};

/**
 * @brief Runtime and editor presentation state for the page title chrome.
 *
 * @note The title text itself still comes from `PageDefinition::title`. This
 * structure only controls the visibility and styling of the rendered chrome.
 */
struct PageTitleDisplayDefinition
{
    /** @brief Enables or disables the page title and its decoration. */
    bool visible = true;
    /** @brief Page-space transform applied to the generated title chrome. */
    Transform2D transform {{-0.9500f, 0.9250f}, 0.0f, {1.0f, 1.0f}};
    /** @brief Shared stroke color applied to the title text and decoration. */
    ColorRgba color {220, 236, 220, 255};
    /** @brief Outline thickness used by underline and frame decorations. */
    float lineWidth = 0.0042f;
    /** @brief Outline stroke pattern used by underline and frame decorations. */
    LineStyle lineStyle = LineStyle::Solid;
    /** @brief Decoration rendered around the title text. */
    PageTitleDecoration decoration = PageTitleDecoration::Underline;
};

/** @brief Default logical font size used by the generated page title chrome. */
inline constexpr float kPageTitleFontSize = 0.0917f;
/** @brief Default logical letter spacing used by the generated page title chrome. */
inline constexpr float kPageTitleLetterSpacing = 0.0042f;

/**
 * @brief Resolves the final user-facing title text rendered for one page.
 * @param pageName Stable runtime page id.
 * @param pageTitle Human-readable page title.
 * @return Final title text shown by the runtime and the editor preview.
 */
MFD_API std::string ResolvePageDisplayTitleText(std::string_view pageName, std::string_view pageTitle);

/**
 * @brief Builds the generated reticle used to preview and render one page title.
 * @param pageName Stable runtime page id.
 * @param pageTitle Human-readable page title.
 * @param display Title visibility and styling state.
 * @return Reticle representation of the title chrome.
 */
MFD_API ReticleGroup BuildPageTitleDisplayReticle(std::string_view pageName,
                                                  std::string_view pageTitle,
                                                  const PageTitleDisplayDefinition& display);

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
    /** @brief Visibility and styling state of the generated page title chrome. */
    PageTitleDisplayDefinition titleDisplay {};
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
    /** @brief Ordered runtime layers used by the renderer to compute the page draw order. */
    std::vector<PageLayerDefinition> layers;
    /** @brief Editor-only page metadata ignored by the runtime. */
    PageEditorState editor {};
    /** @brief Static reticles drawn whenever the page is active. */
    std::vector<ReticleGroup> staticReticles;
    /** @brief Runtime bindings authorizing dynamic reticle templates on the page. */
    std::vector<DynamicReticleLayerBinding> dynamicReticleBindings;
    /** @brief Named strobe catalog available on the page. */
    std::vector<PageStrobeDefinition> strobes;
    /** @brief User-facing active strobe name selected when the page loads. */
    std::string activeStrobeName;
    /** @brief Normalized active strobe name used internally for fast lookup. */
    std::string normalizedActiveStrobeName;
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

/**
 * @brief Finds one strobe definition inside a page.
 * @param page Page to inspect.
 * @param strobeName Strobe name in user-facing or normalized form.
 * @return Pointer to the matching strobe, or `nullptr` if it does not exist.
 */
inline const PageStrobeDefinition* FindPageStrobeDefinition(const PageDefinition& page,
                                                            const std::string_view strobeName)
{
    const std::string normalizedStrobeName = NormalizePageName(strobeName);

    for (const auto& strobe : page.strobes)
    {
        if (strobe.normalizedName == normalizedStrobeName)
        {
            return &strobe;
        }
    }

    return nullptr;
}

/**
 * @brief Finds one mutable strobe definition inside a page.
 * @param page Page to inspect.
 * @param strobeName Strobe name in user-facing or normalized form.
 * @return Pointer to the matching strobe, or `nullptr` if it does not exist.
 */
inline PageStrobeDefinition* FindPageStrobeDefinition(PageDefinition& page, const std::string_view strobeName)
{
    const std::string normalizedStrobeName = NormalizePageName(strobeName);

    for (PageStrobeDefinition& strobe : page.strobes)
    {
        if (strobe.normalizedName == normalizedStrobeName)
        {
            return &strobe;
        }
    }

    return nullptr;
}

/**
 * @brief Returns the strobe currently marked active on one page.
 * @param page Page to inspect.
 * @return Pointer to the active strobe, or `nullptr` when the page has none.
 */
inline const PageStrobeDefinition* FindActivePageStrobeDefinition(const PageDefinition& page)
{
    if (page.strobes.empty())
    {
        return nullptr;
    }

    if (!page.normalizedActiveStrobeName.empty())
    {
        if (const PageStrobeDefinition* active = FindPageStrobeDefinition(page, page.normalizedActiveStrobeName);
            active != nullptr)
        {
            return active;
        }
    }

    return &page.strobes.front();
}

/**
 * @brief Returns the mutable strobe currently marked active on one page.
 * @param page Page to inspect.
 * @return Pointer to the active strobe, or `nullptr` when the page has none.
 */
inline PageStrobeDefinition* FindActivePageStrobeDefinition(PageDefinition& page)
{
    if (page.strobes.empty())
    {
        return nullptr;
    }

    if (!page.normalizedActiveStrobeName.empty())
    {
        if (PageStrobeDefinition* active = FindPageStrobeDefinition(page, page.normalizedActiveStrobeName);
            active != nullptr)
        {
            return active;
        }
    }

    return &page.strobes.front();
}

/**
 * @brief Finds one runtime layer definition inside a page.
 * @param page Page to inspect.
 * @param layerId Layer id in authored or normalized form.
 * @return Pointer to the matching layer, or `nullptr` if it does not exist.
 */
inline const PageLayerDefinition* FindPageLayerDefinition(const PageDefinition& page,
                                                          const std::string_view layerId)
{
    const std::string normalizedLayerId = NormalizePageName(layerId);

    for (const auto& layer : page.layers)
    {
        if (NormalizePageName(layer.id) == normalizedLayerId)
        {
            return &layer;
        }
    }

    return nullptr;
}

/**
 * @brief Finds one runtime layer definition inside a mutable page.
 * @param page Page to inspect.
 * @param layerId Layer id in authored or normalized form.
 * @return Pointer to the matching layer, or `nullptr` if it does not exist.
 */
inline PageLayerDefinition* FindPageLayerDefinition(PageDefinition& page, const std::string_view layerId)
{
    const std::string normalizedLayerId = NormalizePageName(layerId);

    for (PageLayerDefinition& layer : page.layers)
    {
        if (NormalizePageName(layer.id) == normalizedLayerId)
        {
            return &layer;
        }
    }

    return nullptr;
}

/**
 * @brief Finds one dynamic reticle layer binding by template id inside a page.
 * @param page Page to inspect.
 * @param templateId Template id in authored or normalized form.
 * @return Pointer to the matching binding, or `nullptr` if it does not exist.
 */
inline const DynamicReticleLayerBinding* FindDynamicReticleLayerBinding(const PageDefinition& page,
                                                                        const std::string_view templateId)
{
    const std::string normalizedTemplateId = NormalizePageName(templateId);

    for (const auto& binding : page.dynamicReticleBindings)
    {
        if (NormalizePageName(binding.templateId) == normalizedTemplateId)
        {
            return &binding;
        }
    }

    return nullptr;
}

/**
 * @brief Finds one dynamic reticle layer binding by template id inside a mutable page.
 * @param page Page to inspect.
 * @param templateId Template id in authored or normalized form.
 * @return Pointer to the matching binding, or `nullptr` if it does not exist.
 */
inline DynamicReticleLayerBinding* FindDynamicReticleLayerBinding(PageDefinition& page,
                                                                  const std::string_view templateId)
{
    const std::string normalizedTemplateId = NormalizePageName(templateId);

    for (DynamicReticleLayerBinding& binding : page.dynamicReticleBindings)
    {
        if (NormalizePageName(binding.templateId) == normalizedTemplateId)
        {
            return &binding;
        }
    }

    return nullptr;
}
} // namespace mfd
