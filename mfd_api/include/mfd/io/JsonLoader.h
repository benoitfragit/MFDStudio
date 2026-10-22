/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief JSON loading entry points for windows, pages and reticle libraries.
 */

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mfd/control/CommandTransport.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/MfdExport.h"
#include "mfd/model/ApiLevel.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/runtime/GeneratedTransportMap.h"

namespace mfd
{
/**
 * @brief Window-level assets and runtime settings loaded from the root JSON file.
 */
struct WindowAssetDefinition
{
    /** @brief Source JSON file describing the window. */
    std::filesystem::path sourceFile;
    /** @brief Window title. */
    std::string title = "MFD Demo";
    /** @brief Window width in pixels. */
    int width = 1600;
    /** @brief Window height in pixels. */
    int height = 960;
    /** @brief Initial X screen position in pixels. */
    int positionX = 120;
    /** @brief Initial Y screen position in pixels. */
    int positionY = 80;
    /** @brief Requested refresh rate for the host application. */
    int targetFps = 60;
    /** @brief Optional font file used by the host renderer for text primitives and overlays. */
    std::filesystem::path fontFile;
    /** @brief Optional application icon image used by the host window. */
    std::filesystem::path iconFile;
    /** @brief Folder containing reusable reticle templates. */
    std::filesystem::path reticleLibraryFolder;
    /** @brief JSON files defining the pages loaded by the window. */
    std::vector<std::filesystem::path> pageFiles;
    /** @brief Command transport configuration exposed to external clients. */
    WindowCommandTransportConfig commandTransports {};
    /** @brief Feedback transport configuration exposed by the window. */
    WindowFeedbackTransportConfig feedbackTransports {};
    /** @brief Minimum seconds between changed runtime feedback bursts. */
    float feedbackFastIntervalSeconds = 0.020f;
    /** @brief Minimum seconds between unchanged runtime feedback heartbeat bursts. */
    float feedbackHeartbeatIntervalSeconds = 0.350f;
    /** @brief Highest API capability level available to this window. */
    ApiLevel apiLevel = ApiLevel::Legacy;
};

/**
 * @brief Full result of loading a root window JSON.
 */
struct LoadedWindowConfiguration
{
    /** @brief Window-level configuration. */
    WindowAssetDefinition window;
    /** @brief Document containing the pages and the reticle library. */
    MfdDocument document;
    /** @brief Optional generated transport map loaded next to the window JSON. */
    std::optional<GeneratedTransportMap> generatedTransportMap;
};

/**
 * @brief Loads window and page definitions from JSON files.
 */
class MFD_API JsonLoader
{
public:
    /**
     * @brief Loads a root window JSON and all referenced page/template files.
     * @param windowFile Path to the root window JSON file.
     * @return Loaded window configuration and resolved document.
     *
     * @note Numeric JSON fields must be finite. Pixel-oriented integer fields
     * such as window size, screen position, ports, durations and packet sizes
     * are rounded from JSON numbers and must fit in a signed 32-bit integer.
     */
    LoadedWindowConfiguration LoadWindowConfiguration(const std::filesystem::path& windowFile) const;

    /**
     * @brief Loads authored window assets without inspecting the companion generated transport map.
     * @param windowFile Path to the root window JSON file.
     * @return Loaded authored window configuration and resolved document.
     *
     * @note This authoring-specific entry point keeps derived `*.generated.map`
     * files independent from editor loading. Runtime code should use
     * `LoadWindowConfiguration()` so a present generated map remains strictly
     * validated against the authored document.
     */
    LoadedWindowConfiguration LoadWindowConfigurationForAuthoring(
        const std::filesystem::path& windowFile) const;

    /**
     * @brief Loads a legacy pages JSON or a window JSON and returns only the document.
     * @param pagesFile Path to the JSON file to load.
     * @return Loaded document containing pages and reticle templates.
     *
     * @note Numeric JSON fields must be finite. Pixel-oriented integer fields
     * are rounded from JSON numbers and must fit in a signed 32-bit integer.
     */
    /** @brief Loads one reusable widget definition through the existing reticle parser. */
    std::shared_ptr<const api1::WidgetDefinition> LoadWidgetAsset(const std::filesystem::path& file,
        const std::filesystem::path& assetRoot, const ReticleLibrary& library) const;

    /** @brief Loads a single source Page and its reticles for a content-based authoring import. */
    MfdDocument LoadPageAsset(const std::filesystem::path& file, const std::filesystem::path& assetRoot,
                             const std::filesystem::path& reticleFolder) const;

    MfdDocument LoadDocument(const std::filesystem::path& pagesFile) const;
};
} // namespace mfd
